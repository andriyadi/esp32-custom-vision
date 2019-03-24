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

#define CVC_DEBUG(...)	ESP_LOGI("CVC", __VA_ARGS__)
#define CVC_ERROR(...)	ESP_LOGE("CVC", __VA_ARGS__)

//typedef float fptp_t;
//
//typedef struct
//{
//	fptp_t box_p[4];
//} box_t;
//
//typedef struct tag_box_list
//{
//	box_t *box = NULL;
//	int len = 0;
//} bounding_box_t;

#define FACE_COLOR_WHITE  0x00FFFFFF
#define FACE_COLOR_BLACK  0x00000000
#define FACE_COLOR_RED    0x000000FF
#define FACE_COLOR_GREEN  0x0000FF00
#define FACE_COLOR_BLUE   0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)

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
//		bounding_box_t *region  = NULL;
		char tagName[64];// = NULL;
		char tagId[40];// = NULL;
		float probability = 0.0f;
		BoundingBox_t region = {0, 0, 0, 0};

		CustomVisionDetectionModel_t() {
//			region = (bounding_box_t*)calloc(1, sizeof(bounding_box_t));
//			box_t *box = (box_t*)calloc(1, sizeof(box_t));
//			region->box = box;
//			region->len = 1;

//			tagName = (char*)malloc(64);//new char[64]();
//			memset(tagName, 0, 64);
//			tagId = (char*)malloc(40);//new char[40]();
//			memset(tagId, 0, 64);

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
//			if (region != NULL) {
//				if (region->box != NULL) {
//					free(region->box);
//				}
//				free(region);
//				region = NULL;
//			}
		}
	};

	struct CustomVisionDetectionResult_t {
		std::vector<CustomVisionDetectionModel_t> predictions;
		char *id = NULL;

		//Just for convenience
		int16_t bestPredictionIndex = -1;
		char *bestPredictionLabel = NULL;
		float bestPredictionThreshold = 0.5f;

		CustomVisionDetectionResult_t() {
			predictions.reserve(10);
			id = new char[40]();
			bestPredictionLabel = new char[64]();
		}

		~CustomVisionDetectionResult_t() {
			if (id != NULL) {
				delete[] id;
				id = NULL;
			}

			if (bestPredictionLabel != NULL) {
				delete[] bestPredictionLabel;
				bestPredictionLabel = NULL;
			}
		}

		bool bestPredictionFound() {
			return (bestPredictionIndex >= 0);
		}

		void clear() {
			memset(id, 0, 40);
			memset(bestPredictionLabel, 0, 64);
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
