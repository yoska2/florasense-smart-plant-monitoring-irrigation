"""
FloraSense - AI Plant Care Recommendation Service

This AWS Lambda function integrates OpenAI's API with the FloraSense smart
plant monitoring and irrigation system. It receives a plant name through
an HTTP request, uses the GPT-4o-mini model to generate recommended soil
moisture and light intensity thresholds, and returns the results in JSON
format. The function validates incoming requests and AI-generated values,
handles errors, and enables the FloraSense web interface to provide
plant-specific recommendations for monitoring and automated irrigation.
"""
"""
FloraSense - Smart Plant Monitoring & Irrigation System

Course: Robotics for Computer Science
Institute: HIT - Holon Institute of Technology

Group Members:
Yosef Yosef
Ziv Ashkenazi
"""
import json
import os

from openai import OpenAI

# OpenAI API key from Lambda Environment Variables
client = OpenAI(api_key=os.environ["OPENAI_API_KEY"])

def ask_openai(plant_name):
    """
    Ask OpenAI for recommended FloraSense thresholds
    for the requested plant.
    """

    instructions = """
You are the plant-care assistant for a smart plant monitoring
and irrigation system called FloraSense.

The user will provide the name of a plant.

Your job is to recommend two minimum thresholds:

1. soilMin:
   Minimum soil moisture percentage from 0 to 100.
   When measured soil moisture falls BELOW this value,
   FloraSense will automatically water the plant.

2. lightMin:
   Minimum light intensity in lux.
   When measured light falls BELOW this value,
   FloraSense will warn that the plant does not have enough light.

Choose practical values suitable for a home plant monitoring system.

Return ONLY valid JSON in exactly this structure:

{
    "plant": "Plant name",
    "soilMin": 45,
    "lightMin": 5000,
    "explanation": "Short explanation"
}

soilMin must be an integer between 0 and 100.
lightMin must be a positive integer.

Do not include Markdown.
Do not include ```json.
Do not include any text outside the JSON.
"""

    response = client.responses.create(
        model="gpt-4o-mini",
        instructions=instructions,
        input=plant_name,
    )

    return response.output_text

def make_response(status_code, body):
    """Build HTTP response for API Gateway."""

    return {
        "statusCode": status_code,
        "headers": {
            "Content-Type": "application/json",
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Headers": "Content-Type",
            "Access-Control-Allow-Methods": "POST,OPTIONS",
        },
        "body": json.dumps(body, ensure_ascii=False),
    }

def lambda_handler(event, context):
    """Main Lambda handler."""

    method = (
        event.get("requestContext", {})
        .get("http", {})
        .get("method")
    )

    method = method or event.get("httpMethod")

    # CORS request
    if method == "OPTIONS":
        return make_response(200, {})

    try:

        # Read request body
        body = json.loads(event.get("body") or "{}")

        plant_name = str(
            body.get("plant", "")
        ).strip()

        # Validate plant name
        if not plant_name:

            return make_response(
                400,
                {
                    "error":
                    "Plant name is required"
                }
            )

        if len(plant_name) > 100:

            return make_response(
                400,
                {
                    "error":
                    "Plant name is too long"
                }
            )

        # Ask OpenAI
        ai_text = ask_openai(plant_name)

        # Convert AI response to JSON
        plant_data = json.loads(ai_text)

        # Validate returned values
        soil_min = int(
            plant_data["soilMin"]
        )

        light_min = int(
            plant_data["lightMin"]
        )

        if soil_min < 0 or soil_min > 100:

            raise ValueError(
                "Invalid soil moisture threshold"
            )

        if light_min < 0:

            raise ValueError(
                "Invalid light threshold"
            )

        # Clean final response
        result = {
            "plant":
                plant_data.get(
                    "plant",
                    plant_name
                ),

            "soilMin":
                soil_min,

            "lightMin":
                light_min,

            "explanation":
                plant_data.get(
                    "explanation",
                    ""
                )
        }

        return make_response(
            200,
            result
        )

    except Exception as error:

        # Full error appears in CloudWatch
        print(
            f"FloraSense AI error: {error}"
        )

        return make_response(
            500,
            {
                "error":
                "Could not get plant recommendation"
            }
        )

