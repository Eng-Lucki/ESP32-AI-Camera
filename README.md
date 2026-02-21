ESP32-CAM AI Object Detector

An embedded computer vision project using the **ESP32-CAM** module and **Edge Impulse** to perform real-time object detection. The system identifies objects and displays the results on an **OLED screen**.

  <img src="assets/CAM-Project.png" width="600" title="ESP32-CAM AI Detector">
</p>

---

## Features

- **Real-time Inferencing:** On-device machine learning (no cloud required).
- **Visual Feedback:** Results displayed on an SSD1306 OLED display.
- **Low Power:** Optimized for the ESP32-S chip.
- **Custom Model:** Trained via Edge Impulse for [Insert what your camera detects, e.g., "People/Pets"].

## Hardware Requirements

- **Core:** ESP32-CAM Module
- **Display:** 0.96" I2C OLED Screen (SSD1306)
- **Programmer:** FTDI USB-to-TTL Adapter (for uploading code)
- **Misc:** Jumper wires, Breadboard.

## Project Structure

- `src/` - Main C++ source code (.ino / .cpp files).
- `model/` - Trained Edge Impulse library and neural network weights.
- `assets/` - Images and diagrams for this README.

## Wiring Diagram

| ESP32-CAM Pin | OLED Pin |
| :------------ | :------- |
| 5V / 3V3      | VCC      |
| GND           | GND      |
| GPIO 14 (SCL) | SCL      |
| GPIO 15 (SDA) | SDA      |

_Note: Pin mapping may vary based on your specific I2C configuration._

## Installation & Setup

1. **Clone the Repo:**
   ```bash
   git clone https://github.com/Eng-Lucki/ESP32-AI-Camera.git
   ```
2. **Install Libraries:**
   - `Adafruit_SSD1306` & `Adafruit_GFX` (for the display).
   - Your exported `Edge Impulse` library.
3. **Flash the Board:**
   - Connect the FTDI adapter (IO0 to GND for flash mode).
   - Select **AI Thinker ESP32-CAM** in your IDE and hit upload.

## The AI Model

The model was trained using **Edge Impulse** with a dataset of [100] images. It uses a **MobileNetV2** architecture scaled down for microcontroller performance.

---

## 👤 Authors

**Haider Fouad** and **Mohamad Radi**
_Computer Engineering Students at U.O.T_
