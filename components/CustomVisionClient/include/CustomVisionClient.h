/*
 * CustomVisionClient.h
 *
 *  Created on: Jan 28, 2019
 *      Author: andri
 */

#ifndef MAIN_CUSTOMVISIONCLIENT_H_
#define MAIN_CUSTOMVISIONCLIENT_H_

#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include "esp_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
//#include "image_util.h"
#include <vector>

#define CVC_DEBUG_ENABLED 	1

#define CVC_DEBUG(...)	if (CVC_DEBUG_ENABLED) ESP_LOGI("CVC", __VA_ARGS__);
#define CVC_INFO(...)	ESP_LOGI("CVC", __VA_ARGS__)
#define CVC_ERROR(...)	ESP_LOGE("CVC", __VA_ARGS__)

//typedef float fptp_t;

#define FACE_COLOR_WHITE  0x00FFFFFF
#define FACE_COLOR_BLACK  0x00000000
#define FACE_COLOR_RED    0x000000FF
#define FACE_COLOR_GREEN  0x0000FF00
#define FACE_COLOR_BLUE   0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)

#define FACE_BOX_LABEL_COLOR	FACE_COLOR_YELLOW

class CustomVisionClient {
public:
	struct CustomVisionClientConfig_t {
		const char *host;
		const char *predictionKey;
		const char *iterationId;
		const char *projectId;
		bool shouldStoreImage;
	};

	struct CustomVisionDetectionConfig_t {
		QueueHandle_t *queue;
		camera_fb_t *cameraFb;
		CustomVisionClient *client;
		float bestPredictionThreshold = 0.5f;
	};

	struct BoundingBox_t {
		float left, top, width, height;
	};

	struct CustomVisionDetectionModel_t {
		char tagName[64];	// = NULL; --> Somehow I can not use dynamic allocation for this class
		char tagId[40];		// = NULL;
		float probability = 0.0f;
		BoundingBox_t region = {0, 0, 0, 0};

		CustomVisionDetectionModel_t() {
//			tagName = (char*)calloc(64, sizeof(char));//new char[64]();
//			tagId = (char*)calloc(40, sizeof(char));//new char[40]();

		}

		~CustomVisionDetectionModel_t() {
//			if (tagId != NULL) {
////				delete[] tagId;
////				tagId = NULL;
//				free(tagId);
//			}
//			if (tagName != NULL) {
////				delete[] tagName;
////				tagName = NULL;
//				free(tagName);
//			}
		}
	};

	struct CustomVisionDetectionResult_t {
		std::vector<CustomVisionDetectionModel_t> predictions;
		char *id = NULL;
		float bestPredictionThreshold = 0.5f;

		//Just for convenience
		int16_t bestPredictionIndex = -1;

		CustomVisionDetectionResult_t() {
			predictions.reserve(10);
			id = new char[40]();
		}

		~CustomVisionDetectionResult_t() {
			if (id != NULL) {
				delete[] id;
				id = NULL;
			}
		}

		bool isBestPredictionFound() {
			return (bestPredictionIndex >= 0);
		}

		const CustomVisionClient::CustomVisionDetectionModel_t *getBestPrediction() {
			if (predictions.empty() || !isBestPredictionFound()) {
				return NULL;
			}
			else {
				return &predictions.at(bestPredictionIndex);
			}
		}

		bool getBestPredictionLabel(char *label) {
			if (label == NULL || !isBestPredictionFound()) {
				return false;
			}

			CustomVisionClient::CustomVisionDetectionModel_t bestPred = predictions.at(bestPredictionIndex);
			sprintf(label, "%s (%.2f%s)", bestPred.tagName, (bestPred.probability*100), "%");

			return true;
		}

		void clear() {
			memset(id, 0, 40);
			predictions.clear();
		}
	};

	CustomVisionClient(const CustomVisionClientConfig_t &config);
	virtual ~CustomVisionClient();

	esp_err_t detect(camera_fb_t *fbIn, CustomVisionDetectionResult_t *predResult = NULL, float bestPredThreshold = 0.0f, bool drawInfo=false, uint8_t **out_buf = NULL, size_t *out_len = NULL);

	// Asynchronously do the detection, and will send result to a queue upon completion
	esp_err_t detectAsync(camera_fb_t *fbIn, float threshold = 0.0f);

	// Util methods
	esp_err_t putInfoOnFrame(camera_fb_t *fbIn, const std::vector<CustomVisionDetectionModel_t> &predictions, float minDispThreshold, uint8_t ** out_buf, size_t * out_len);
	esp_err_t putLabelOnFrame(camera_fb_t *fbIn, const char *label, int32_t x, int32_t y, uint32_t color, uint8_t ** out_buf, size_t * out_len);

	QueueHandle_t detectionQueue = NULL;

private:
	CustomVisionClientConfig_t clientConfig_ = {};
	CustomVisionDetectionConfig_t *detectionCfg_ = NULL;
};

#endif /* MAIN_CUSTOMVISIONCLIENT_H_ */
