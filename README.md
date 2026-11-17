# FloraSense

**Smart Plant Monitoring & Automatic Irrigation | Electronics · Web Development · AI & IoT**

FloraSense is a smart plant monitoring and automatic irrigation system built around the **ESP32-WROOM-32**. The project brings together three main parts: an **electronic monitoring and irrigation system**, a **web dashboard**, and an **AI recommendation service** hosted on AWS.

Developed as part of the **Robotics for Computer Science** course at **HIT – Holon Institute of Technology**.

## 1. Electronics & Embedded Systems

The ESP32 is the main controller of the physical system. It reads analog and digital sensors, displays live measurements on a local LCD, and operates a DC water pump through a relay. Automatic irrigation runs directly on the ESP32, even when the website is closed.

### Hardware Components

| Component | Role |
| --- | --- |
| **ESP32-WROOM-32** | Reads sensors, controls irrigation, updates the LCD, and provides Wi-Fi communication |
| **Capacitive soil-moisture sensor** | Measures soil moisture through an analog ADC input |
| **BH1750** | Measures light intensity using I²C |
| **BME280** | Measures air temperature and relative humidity using I²C |
| **Float switch** | Detects water availability in the reservoir |
| **Active-LOW relay module** | Switches power to the DC water pump |
| **DC water pump** | Supplies water during irrigation |
| **20×4 LCD** | Displays sensor readings and system status |

The hardware design includes **four soil-moisture inputs and four relay channels**. The firmware included in this repository operates with **one soil-moisture sensor and one relay-controlled pump**.

### Automatic Irrigation

1. The ESP32 reads the connected sensors and updates the display.
2. When soil moisture falls below the configured threshold, it checks the reservoir float switch.
3. If water is available and the cooldown has elapsed, it runs the pump for **3 seconds**.
4. It then waits **30 seconds** before another automatic irrigation cycle.

The ESP32 firmware was developed in **C++ using Arduino IDE**, with **ADC, GPIO, I²C, Wi-Fi, and HTTP**.

## 2. Web Application & Software

The browser-based dashboard provides a user-friendly way to monitor the physical system and change its settings. It communicates with the ESP32 over the local Wi-Fi network using **HTTP requests and JSON responses**.

### Web Features

- View current sensor readings and system status.
- Change minimum soil-moisture and light-intensity thresholds.
- Start or stop the pump manually.
- Request plant-specific threshold recommendations from the AI service.

The website uses **HTML and JavaScript** and was hosted with **AWS Amplify**.

### ESP32 HTTP API

| Method | Endpoint | Purpose |
| --- | --- | --- |
| `GET` | `/status` | Read system status |
| `GET` | `/sensors` | Read current sensor measurements |
| `GET` | `/settings` | Read saved threshold settings |
| `POST` | `/settings?soilMin=45&lightMin=60` | Update moisture and light thresholds |
| `POST` | `/pump?action=on` | Start a timed manual pump run |
| `POST` | `/pump?action=off` | Stop the pump and return to automatic mode |

The `soilMin` setting accepts values from **0 to 100**; `lightMin` accepts values from **0 to 100000 lux**.

**Manual control:** In the current firmware, manual pump activation runs for up to **3 seconds** and does not perform the reservoir check used in automatic mode.

## 3. AI & Cloud Integration

FloraSense also provides recommendations tailored to the selected plant type. This feature is separate from the ESP32's local irrigation logic: the physical system does not depend on the AI service to perform automatic watering.

### AI Recommendation Process

1. The user selects a plant type through the website.
2. The website sends a request to **Amazon API Gateway**.
3. API Gateway invokes an **AWS Lambda** function written in **Python**.
4. The function uses the **OpenAI API (GPT-4o mini)** to suggest soil-moisture and light-intensity thresholds.
5. The website displays the recommendations, which the user can apply to the ESP32 settings.

**Cloud technologies:** AWS Amplify, Amazon API Gateway, AWS Lambda (Python), OpenAI API.

## Repository Structure

```text
florasense-smart-plant-monitoring-irrigation/
├── src/
│   ├── esp32/
│   │   └── FloraSense_ESP32.ino
│   ├── Website/
│   │   ├── ai.html
│   │   └── index.html
│   └── aws_lambda/
│       └── lambda_function.py
├── docs/
│   ├── Component_Wiring_Diagram.jpeg
│   ├── Image_Documentation.pdf
│   ├── Project_Summary.pdf
│   ├── Project_Summary_HE.pdf
│   ├── System_Wiring_Diagram.jpeg
│   └── Technical_Documentation_HE.pdf
└── README.md
```

## Development & Implementation

**ESP32:** The firmware was developed in **C++ using Arduino IDE**. It handles sensor readings, automatic irrigation, LCD output, and Wi-Fi communication. The electronic system was assembled and tested as part of the project.

**Website:** The web dashboard was developed using **HTML and JavaScript** and hosted on **AWS Amplify**. It communicates with the ESP32 through its local HTTP API, allowing users to monitor sensor readings, adjust settings, and control irrigation.

**AI Service:** The recommendation service was developed in **Python using AWS Lambda**, integrated with **Amazon API Gateway** and the **OpenAI API (GPT-4o mini)**. It provides plant-specific recommendations for soil-moisture and light-intensity thresholds.

## Project Documentation

- [Project Summary (English)](docs/Project_Summary.pdf)
- [Project Summary (Hebrew)](docs/Project_Summary_HE.pdf)
- [Technical Documentation (Hebrew)](docs/Technical_Documentation_HE.pdf)
- [Image Documentation](docs/Image_Documentation.pdf)
- [Component Wiring Diagram](docs/Component_Wiring_Diagram.jpeg)
- [System Wiring Diagram](docs/System_Wiring_Diagram.jpeg)
- [Project Demonstration Video](https://www.youtube.com/watch?v=kOMgph83rLc)

## Contributors

- **Yosef Yosef**
- **Ziv Ashkenazi**
