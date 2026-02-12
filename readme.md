# llc_esp: Networked Light Controller (ESP32-S3)

**llc_esp** is a high-performance C++ firmware for the ESP32-S3. It acts as a networked receiver, listening for "Light Lang" commands sent from a PC via UDP over the local Wi-Fi network to drive LED lighting effects in real-time.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Protocol](https://img.shields.io/badge/protocol-UDP-red.svg)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-green.svg)

## 📡 Features

* **UDP Listener:** Efficiently processes incoming UDP packets on the local network.
* **Light Lang Support:** Parses custom "Light Lang" commands to trigger specific lighting states.
* **Low Latency:** Optimized for real-time synchronization with PC-based audio or logic.
* **WiFi Auto-Connect:** Automatically connects to the local network on boot.

## 🛠️ Hardware Requirements

* **Microcontroller:** ESP32-S3.
* **Lights:** Addressable LEDs (WS2812B, SK6812) or PWM-controlled lights.
* **Network:** 2.4GHz Wi-Fi connection.

## ⚙️ Configuration

Set GPIO and Led Count in `src/main.cpp` at `line 16`:

```cpp
#define LED_COUNT 60
#define STRIP_GPIO 12
```

Configure your led strip in `src/main.cpp` at `line 69`:

```cpp
void configure_led(void) { ... }
```

Set your network credentials and port in `src/main.cpp` at `line 103`:

```cpp
wifi->set_ssid("WIFI_SSID_HERE");
wifi->set_password("WIFI_PASSWORD_HERE");
```