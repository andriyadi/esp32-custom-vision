ESP32-CUSTOM-VISION
===================

So, my company just got a free [ESP-EYE](https://www.espressif.com/en/products/hardware/esp-eye/overview) development board from Espressif (the maker of ESP32, ESP8266 chip) itself. It's developed officially by Espresif as an attempt to quickly get started to create image recognition or audio processing-related application.

Espressif developes a sample application that make the most use of the board, namely: [**esp-who**](https://github.com/espressif/esp-who). To me, it's an awesome project that shows how to do speech recognition and face recognition, all done at the edge, or on the board itself, not in the cloud.

While **esp-who** is great for making use of the board and to embrace edge-intelligence, I want to do something else. As a Microsoft Most Valuable Professional of Microsoft Azure, I want to make use of Azure services, specifically Azure Custom Vision, to be used as cloud-based image recognition. That objective is the reason I create this repo.

This project is made with `Espressif IoT Development Framework` ([ESP-IDF](https://github.com/espressif/esp-idf)). Please check ESP-IDF docs for getting started instructions.

## Getting Started

* Clone this repo, recursively: `git clone --recursive https://github.com/andriyadi/esp32-custom-vision.git`
* If you clone project without `--recursive` flag, please go to the `esp32-custom-vision` directory and run command this command to update submodules which it depends on: `git submodule update --init --recursive` 
* Create `secrets.h` file. Explained below.
* Try to `make`. Fingers crossed :)


## `secrets.h` file
Under `main` folder, create a file named `secrets.h` with the content:
```
#ifndef MAIN_SECRETS_H_
#define MAIN_SECRETS_H_


#define SSID_NAME "[YOUR_OWN_WIFI_SSID_NAME]"
#define SSID_PASS "[YOUR_OWN_WIFI_SSID_PASSWORD]"

#define AZURE_CV_PREDICTION_KEY "[YOUR_OWN_AZURE_CUSTOM_VISION_PREDICTION_KEY]"

#endif /* MAIN_SECRETS_H_ */
```

Replace all value with [...] inside quote.
