Web-Controlled ESP32 Autonomous Rover
Overview
This project is a custom-built rover controlled entirely via a local web interface. It integrates multiple motors and servos for complex movements, utilizing a local Wi-Fi network for real-time manual control and monitoring.

Hardware Components
Microcontroller: ESP32

Motor Driver: L298N (for DC gear motors)

PWM/Servo Driver: PCA9685 (for servo control)

Power Supply:12V Power suppply with rechargeable C-Type port and using 5v Buck converter, 20A buck converter for ESP32 and PCA muscle power. 

Actuators: DC Motors, Micro Servos

Features
Web Interface: Custom-built HTML/CSS control page hosted directly on the ESP32.

I2C Integration: Uses the PCA9685 via I2C to offload PWM generation from the main microcontroller.

Smooth Integration: Seamless simultaneous control of steering (servos) and drive (DC motors).
