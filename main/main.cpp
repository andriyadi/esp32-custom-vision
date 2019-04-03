#include <esp_log.h>
#include "sdkconfig.h"
#include "secrets.h"
#include "app_camera.hpp"
#include "esp_http_server.h"
#include "CustomVisionClient.h"
#include "app_httpd.hpp"

#if CONFIG_CAMERA_BOARD_TTGO_TCAM
#include "ssd1306.h"
#endif

extern "C" {
	void app_main(void);
}

#define TAG "APP"


#if CONFIG_CAMERA_BOARD_TTGO_TCAM
void initOLED() {
//	ssd1306_setFixedFont(ssd1306xled_font6x8);
	ssd1306_setFixedFont(ssd1306xled_font5x7);
	ssd1306_128x64_i2c_init(); //default use I2C_NUM_1. SDA: 21, SCL: 22

	ssd1306_flipHorizontal(1);
	ssd1306_flipVertical(1);

	ssd1306_clearScreen();
	ssd1306_printFixedN((ssd1306_displayWidth() - 10*10)/2, 30, "Waiting...", STYLE_BOLD, FONT_SIZE_2X);
}
#endif

CustomVisionClient *cvc = NULL;

void app_main(void)
{
	esp_err_t res = camera_init();
	if (res != ESP_OK) {
		ESP_LOGE(TAG, "Camera init failed. Exiting app.");
		return;
	}
	else {
		ESP_LOGI(TAG, "Camera init SUCCESS!");
	}

#if CONFIG_CAMERA_BOARD_TTGO_TCAM
	initOLED();
#endif

	CustomVisionClient::CustomVisionClientConfig_t cvcConfig = {
			AZURE_CV_HOST, //Host
			AZURE_CV_PREDICTION_KEY, //Prediction key
			AZURE_CV_ITERATION_ID, //Iteration ID
			AZURE_CV_PROJECT_ID, //Project ID
			false //Should store image for next training or not
	};

	cvc = new CustomVisionClient(cvcConfig);

	// Start HTTP server to serve the UI
	startHttpd(cvc);
}
