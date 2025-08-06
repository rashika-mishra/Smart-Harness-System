# Smart-Harness-System
This project aims to design a smart harness system in order enhance the safety of onsite workers

# Smart Harness System

## Table of Contents
- [Overview](#overview)
- [Project Objectives](#project-objectives)
- [Use Case and Importance](#use-case-and-importance)
  - [Use Case](#use-case)
  - [Importance](#importance)
- [System Components](#system-components)
  - [Hardware](#hardware)
  - [Software](#software)
- [System Architecture](#system-architecture)
- [Features](#features)
- [How It Works](#how-it-works)
- [Dashboard UI Preview](#dashboard-ui-preview)
- [Getting Started](#getting-started)
  - [Hardware Setup](#hardware-setup)
  - [Software Setup](#software-setup)
  - [Accessing the Dashboard](#accessing-the-dashboard)
- [Future Improvements](#future-improvements)
- [License](#license)
- [Author](#author)

## Overview

The Smart Harness System is an IoT-based safety device, designed specifically for workers operating in hazardous environments (such as construction, industry, or outdoor labor) where falls and accidents are a major risk. The device is worn as a harness and continuously monitors key health and motion indicators such as heart rate, oxygen saturation (SpO₂), fall events, and GPS location. Upon detecting a fall, it immediately alerts caregivers/emergency contacts via **Email** and **SMS**, providing real-time health data and precise GPS coordinates of the worker.

## Project Objectives

- **Real-time Worker Safety:** Ensure immediate detection and reporting of falls and abnormal events.
- **Remote Monitoring:** Enable supervisors and families to monitor health and safety conditions from anywhere.
- **Emergency Alerts:** Deliver automatic, location-tagged notifications in the event of danger.
- **Comprehensive Dashboard:** Offer live web-based monitoring of vital stats and incident history.

## Use Case and Importance

### Use Case

- **Industrial and Construction Workers:** Mitigates risk in high-elevation or hazardous job sites.
- **Lone Workers/Outdoor Laborers:** Increases safety for workers in remote or solitary locations.
- **Elderly or Medically Vulnerable Individuals:** Can be adapted for fall detection and health monitoring at home.

### Importance

- **Immediate Response:** Drastically reduces time-to-intervention after an accident, which can save lives.
- **Accurate Location Reporting:** Ensures rescuers know the exact place of the emergency.
- **Health Monitoring:** Detects and reports potentially dangerous physical conditions (e.g., abnormal heart rate or low oxygen).
- **Compliance & Assurance:** Supports safety regulations and provides peace of mind for workers and their families.

## System Components

### Hardware

- **ESP32 Microcontroller:** Central control and communication.
- **MAX30105/30102 Sensor:** Measures heart rate (BPM) and oxygen saturation (SpO₂).
- **MPU6050 Accelerometer/Gyroscope:** Detects falls and sudden movements.
- **GPS Module (UART):** Acquires real-time geographic location (latitude/longitude).
- **Buzzer & LED:** Provide local alarm/visual alert on fall events.
- **WiFi Module (ESP32 built-in):** Connects system to the Internet for dashboard access and alerts.

### Software

- **ESPAsyncWebServer:** Serves a responsive, secure dashboard for real-time monitoring via web browser.
- **ESP_Mail_Client:** Sends Email alerts using SMTP (e.g., Gmail).
- **Twilio API via HTTPClient:** Delivers SMS alerts to emergency contacts.
- **NTP (Network Time Protocol):** Obtains accurate real-time timestamps.
- **AJAX/JavaScript Web Frontend:** Presents live sensor data and maps on a user-friendly dashboard UI.

## Features

- **Fall Detection:** Highly sensitive to sudden acceleration changes (using MPU6050).
- **Real-time Vitals:** Reports live BPM & SpO₂ readings.
- **Immediate Alerts:** Sends alert Email and SMS with live vitals and precise map location on detection of a fall.
- **Location Tracking:** Fetches GPS data to geolocate the wearer during an incident.
- **Visual & Sound Notification:** Activates LED and buzzer alarm on local device.
- **Secure Dashboard:** Web-based and protected with HTTP basic authentication.
- **Incident History:** Displays timing and history of past fall events.

## How It Works

1. **Initialization:** System sets up all sensors, WiFi, NTP time, and communication interfaces.
2. **Continuous Monitoring:** 
    - Reads heart rate and SpO₂ from MAX30105.
    - Samples acceleration from the MPU6050 to check for falls.
    - Retrieves GPS location.
3. **Fall Detection:**
    - On detecting abnormal acceleration or orientation:  
      - Buzzer and LED are triggered.
      - System logs time and saves incident.
      - Sends Email (via SMTP) and SMS (via Twilio) to recipients, including vitals and map link.
4. **Web Dashboard:**
    - All sensor readings and incident logs are viewable remotely via a user-friendly dashboard, secured by username/password.
    - Map view shows last detected location.
5. **Restoration:** After a certain interval without further abnormal motion, alarms and alert states are reset.

## Dashboard UI Preview

- **Vitals:** BPM, SpO₂
- **Fall Status:** Alerts if fall detected, with last fall time.
- **Location:** Latitude & Longitude with live map display.
- **Incident History:** Chart/record of past events.
- **Secure Access:** Protected by HTTP basic auth.

## Getting Started

### Hardware Setup

- Wire ESP32 to:
    - MAX30105 (I2C)
    - MPU6050 (I2C)
    - GPS module (UART1: RX=16, TX=17)
    - Buzzer (GPIO25), LED (GPIO26)

### Software Setup

1. **Install Required Libraries** in Arduino IDE or PlatformIO:
    - `ESPAsyncWebServer`
    - `ESP_Mail_Client`
    - `MAX30105`
    - `heartRate.h`
    - `Wire`
    - `WiFi.h`
    - `HTTPClient`
    - ... and any supporting dependencies.
2. **Configure Credentials:**  
   Edit WiFi, Email, and Twilio (SMS) settings at the top of the code.
3. **Upload Code** to ESP32.
4. **Power device** and connect to local WiFi.

### Accessing the Dashboard

- Open a browser to `http://[device_ip]/`
- Login with the set username and password.
- Monitor sensors and incident data live.

## Future Improvements

- Battery status monitoring.
- Cloud data storage and analytics.
- Multi-user and role-based dashboard.
- Integration with emergency services.

## License

This project is for educational and research use. Please credit author for commercial or industrial adaptations.

## Author

* [Rashika Mishra]

**Stay safe. Stay connected.**  
*Smart Harness System — Because every second counts in an emergency!*

## System Architecture
[Wearer]
|
[Sensors (MAX30105|MPU6050|GPS)]
|
[ESP32 - Signal Processing & Decision]
|
[WiFi]
|
[Web Dashboard] <--- Live Data --->
| | [Email + SMS Alert]
[Supervisor/Caregiver Devices]
