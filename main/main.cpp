#include <esp_log.h>
#include "sdkconfig.h"
#include "secrets.h"
#include "app_camera.hpp"
#include "esp_http_server.h"
#include "CustomVisionClient.h"

#if CONFIG_CAMERA_BOARD_TTGO_TCAM
#include "ssd1306.h"
#endif

#define TAG "APP"

#define STREAM_PREDICTION_INTERVAL 10000000 //10 seconds

// For streaming-related
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static const char* DETECTING_MESSAGE = "Recognizing...";
static const char* DETECT_NO_RESULT_MESSAGE = "Nobody there :(";

typedef enum
{
    APP_BEGAN,
	APP_WAIT_FOR_CONNECT,
	APP_WIFI_CONNECTED,
	APP_START_DETECT,
	APP_START_RECOGNITION,
	APP_DONE_RECOGNITION,
	APP_START_ENROLL,
	APP_START_DELETE,

} app_state_e;

extern "C" {
	void app_main(void);
}

CustomVisionClient *cvc = NULL;
httpd_handle_t camera_httpd = NULL;

app_state_e app_state = APP_BEGAN;

static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "hello", 5);
}

static esp_err_t capture_handler(httpd_req_t *req){
	esp_err_t res = ESP_OK;

    camera_fb_t *fb = esp_camera_fb_get();
	if (!fb) {
		ESP_LOGE(TAG, "Camera buffer is not accessible");
		httpd_resp_send_500(req);
		res = ESP_FAIL;
	}
	else {
		if (fb->format == PIXFORMAT_JPEG) {
			httpd_resp_set_type(req, "image/jpeg");
			httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

			res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
		}
	}
	esp_camera_fb_return(fb);

	return res;
}

static esp_err_t recog_handler(httpd_req_t *req){
	esp_err_t res = ESP_OK;

    camera_fb_t *fb = esp_camera_fb_get();

	if (!fb) {
		ESP_LOGE(TAG, "Camera buffer is not accessible");
		httpd_resp_send_500(req);
		res = ESP_FAIL;
	}
	else {
		if (fb->format == PIXFORMAT_JPEG) {
			httpd_resp_set_type(req, "image/jpeg");
			httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

			uint8_t *out_buff = fb->buf;
			size_t out_len = fb->len;

			// To do object detection
			CustomVisionClient *_cvc = (CustomVisionClient*)req->user_ctx;
			if (_cvc != NULL) {

				// Alternative 1
//				res = _cvc->detect(fb, NULL, NULL, true, &out_buff, &out_len);
//				if (res != ESP_OK) {
//	//				request->send(500, "text/plain", "Prediction is failed");
//	//				return;
//				}

				// Alternative 2
				box_array_t *box_list = nullptr;
				char label[64];
				res = _cvc->detect(fb, &box_list, label);
				if (res == ESP_OK) {
					// Draw box and label
					_cvc->putInfoOnFrame(fb, box_list, label, &out_buff, &out_len);

#if CONFIG_CAMERA_BOARD_TTGO_TCAM
					// Display label on OLED
					ssd1306_clearScreen();
					ssd1306_setFixedFont(ssd1306xled_font5x7);
					ssd1306_printFixedN(5, 5, "Hello", STYLE_BOLD, FONT_SIZE_2X);
					ssd1306_setFixedFont(ssd1306xled_font6x8);
					ssd1306_printFixedN(5, 30, label, STYLE_BOLD, FONT_SIZE_NORMAL);
#endif

					free(box_list->box);
					free(box_list);
				}
			}

			res = httpd_resp_send(req, (const char *)out_buff, out_len);
		}
	}

	esp_camera_fb_return(fb);

	return res;
}

static esp_err_t stream_handler(httpd_req_t *req){
	camera_fb_t * fb = NULL;
	esp_err_t res = ESP_OK;
	char * part_buf[64];

	size_t _jpg_buf_len = 0;
	uint8_t * _jpg_buf = NULL;

	res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);

	CustomVisionClient *_cvc = (CustomVisionClient*)req->user_ctx;

	int64_t _nextPredictTime = esp_timer_get_time() + STREAM_PREDICTION_INTERVAL/2;
	int64_t _showInfoUntil = 0;

	CustomVisionClient::CustomVisionDetectionResult_t _predRes = {};

	camera_fb_t *_asyncDetectFb = NULL;

	while(true){

		fb = esp_camera_fb_get();

		if (!fb) {
			ESP_LOGE(TAG, "Camera capture failed");
			res = ESP_FAIL;
			break;
		}

		if (fb->format != PIXFORMAT_JPEG) {
			ESP_LOGE(TAG, "Camera captured images is not JPEG");
			res = ESP_FAIL;
		} else {
//			_jpg_buf_len = fb->len;
//			_jpg_buf = fb->buf;
		}

		if(res == ESP_OK) {

			if (esp_timer_get_time() > _nextPredictTime) {
				if (_cvc != NULL) {

					// Clear prev box if any
					if (_predRes.box_list != NULL) {
						free(_predRes.box_list->box);
						free(_predRes.box_list);
						_predRes.box_list = NULL;

						free(_predRes.label);
						_predRes.label = NULL;
					}

					// Copy camera framebuffer to be sent for detection
					_asyncDetectFb =  (camera_fb_t*)malloc(sizeof(camera_fb_t));
					_asyncDetectFb->len = fb->len;
					_asyncDetectFb->width = fb->width;
					_asyncDetectFb->height = fb->height;
					_asyncDetectFb->format = fb->format;
					_asyncDetectFb->buf =  (uint8_t*)malloc(fb->len);
					memcpy(_asyncDetectFb->buf, fb->buf, fb->len);

					// Do detection asynchronously, so the video stream not interrupted
					res = _cvc->detectAsync(_asyncDetectFb);

					app_state = APP_START_RECOGNITION;
					_showInfoUntil = esp_timer_get_time() + 3000000;

					_nextPredictTime = esp_timer_get_time() + (STREAM_PREDICTION_INTERVAL);
//					vTaskDelay(50/portTICK_PERIOD_MS);

					ESP_LOGI(TAG, "Free heap before pred: %ul", esp_get_free_heap_size());
				}
			}

			// Check queue from aysnc prediction
			if (app_state == APP_START_RECOGNITION && _cvc->detectionQueue) {

				if (xQueueReceive(_cvc->detectionQueue, &_predRes, 5) != pdFALSE) {
					ESP_LOGI(TAG, "Got async-ed detection result. label: %s", _predRes.label);

					app_state = APP_DONE_RECOGNITION;
					_showInfoUntil = esp_timer_get_time() + STREAM_PREDICTION_INTERVAL/2;

					// Update next prediction time
					_nextPredictTime = esp_timer_get_time() + (STREAM_PREDICTION_INTERVAL);

					// Free prev resource used for detection
					if (_asyncDetectFb != NULL) {
						free(_asyncDetectFb->buf);
						_asyncDetectFb->buf = NULL;
						free(_asyncDetectFb);
						_asyncDetectFb = NULL;
					}

					ESP_LOGI(TAG, "Free heap after pred: %ul", esp_get_free_heap_size());
				}
			}

			if (app_state == APP_START_RECOGNITION && (esp_timer_get_time() < _showInfoUntil)) {
				int32_t x = (fb->width - (strlen(DETECTING_MESSAGE) * 14)) / 2;
				int32_t y = 10;
				_cvc->putLabelOnFrame(fb, DETECTING_MESSAGE, x, y, FACE_COLOR_YELLOW, &_jpg_buf, &_jpg_buf_len);
			}

			if (app_state == APP_DONE_RECOGNITION && (esp_timer_get_time() < _showInfoUntil)) {
				if (_predRes.box_list != NULL) {
					// If box is there, meaning there's prediction, draw it
					_cvc->putInfoOnFrame(fb, _predRes.box_list, _predRes.label, &_jpg_buf, &_jpg_buf_len);

				} else {
					int32_t x = (fb->width - (strlen(DETECT_NO_RESULT_MESSAGE) * 14)) / 2;
					int32_t y = 10;
					_cvc->putLabelOnFrame(fb, DETECT_NO_RESULT_MESSAGE, x, y, FACE_COLOR_RED, &_jpg_buf, &_jpg_buf_len);
				}
			}
		}

		// If still NULL, just use camera framebuffer
		if (_jpg_buf == NULL) {
			_jpg_buf_len = fb->len;
			_jpg_buf = fb->buf;
		}

		if (res == ESP_OK) {
			size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
			res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
		}
		if (res == ESP_OK) {
			res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
		}
		if (res == ESP_OK) {
			res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
		}

		if (fb) {

			// This should happen when _jpg_buf is NOT coming from camera's framebuffer. Important to free it.
			if (fb->buf != _jpg_buf) {
				free(_jpg_buf);
			}
			_jpg_buf = NULL;

			esp_camera_fb_return(fb);
			fb = NULL;

		}
		else {
			if (_jpg_buf) {
				free(_jpg_buf);
				_jpg_buf = NULL;
			}
		}

		if (res != ESP_OK) {
			break;
		}
	}

	// Clear last box
	if (_predRes.box_list != NULL) {
		free(_predRes.box_list->box);
		free(_predRes.box_list);
		_predRes.box_list = NULL;

		free(_predRes.label);
		_predRes.label = NULL;
	}

//	if (asyncPredictFb != NULL) {
//		free(asyncPredictFb->buf);
//		free(asyncPredictFb);
//		asyncPredictFb = NULL;
//	}

	// Shit happens!
	if (res != ESP_OK) {
		httpd_resp_send_500(req);
	}

	return res;
}

#include "DXWiFi.h"

void startHttpd() {

	// Start WiFi connection
	app_state = APP_WAIT_FOR_CONNECT;

	DXWiFi *wifi = DXWiFi::GetInstance(WIFI_MODE_STA);
	wifi->ConnectSync(SSID_NAME, SSID_PASS, portMAX_DELAY);

	app_state = APP_WIFI_CONNECTED;

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
//	config.max_open_sockets = 10;
//	config.send_wait_timeout = 30;
//	config.recv_wait_timeout = 30;

	httpd_uri_t index_uri = {
		.uri       = "/",
		.method    = HTTP_GET,
		.handler   = index_handler,
		.user_ctx  = NULL
	};

	httpd_uri_t capture_uri = {
		.uri       = "/capture",
		.method    = HTTP_GET,
		.handler   = capture_handler,
		.user_ctx  = NULL
	};

	httpd_uri_t recog_uri = {
		.uri       = "/recog",
		.method    = HTTP_GET,
		.handler   = recog_handler,
		.user_ctx  = cvc
	};

	httpd_uri_t stream_uri = {
		.uri       = "/stream",
		.method    = HTTP_GET,
		.handler   = stream_handler,
		.user_ctx  = cvc
	};

	ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
	if (httpd_start(&camera_httpd, &config) == ESP_OK) {
		httpd_register_uri_handler(camera_httpd, &index_uri);
		httpd_register_uri_handler(camera_httpd, &capture_uri);
		httpd_register_uri_handler(camera_httpd, &recog_uri);
		httpd_register_uri_handler(camera_httpd, &stream_uri);
	}
}

#if CONFIG_CAMERA_BOARD_TTGO_TCAM

void initOLED() {
//	ssd1306_setFixedFont(ssd1306xled_font6x8);
	ssd1306_setFixedFont(ssd1306xled_font5x7);
	ssd1306_128x64_i2c_init(); //default use I2C_NUM_1. SDA: 21, SCL: 22

	ssd1306_flipHorizontal(1);
	ssd1306_flipVertical(1);

	ssd1306_clearScreen();
	ssd1306_printFixedN((128 - 10*10)/2, 30, "Waiting...", STYLE_BOLD, FONT_SIZE_2X);
}
#endif

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
			AZURE_CV_PROJECT_ID //Project ID
	};

	cvc = new CustomVisionClient(cvcConfig);

	// Start HTTP server to serve the UI
	startHttpd();
}
