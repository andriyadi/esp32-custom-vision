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

#define CVC_DEBUG(...)	ESP_LOGI("CVC", __VA_ARGS__)
#define CVC_ERROR(...)	ESP_LOGE("CVC", __VA_ARGS__)

typedef float fptp_t;

typedef struct
{
	fptp_t box_p[4];
} box_t;

typedef struct tag_box_list
{
	box_t *box;
	int len;
} box_array_t;

#define FACE_COLOR_WHITE  0x00FFFFFF
#define FACE_COLOR_BLACK  0x00000000
#define FACE_COLOR_RED    0x000000FF
#define FACE_COLOR_GREEN  0x0000FF00
#define FACE_COLOR_BLUE   0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)

class CustomVisionClient {
public:
	typedef struct {
		const char *host;
		const char *predictionKey;
		const char *iterationId;
		const char *projectId;
	} CustomVisionClientConfig_t;

	typedef struct {
		QueueHandle_t *queue;
		camera_fb_t *camera_fb;
		CustomVisionClient *client;
	} CustomVisionDetectionConfig_t;

	typedef struct {
		box_array_t *box_list  = NULL;
		char *label = NULL;
	} CustomVisionDetectionResult_t;

	CustomVisionClient(const CustomVisionClientConfig_t &config);
	virtual ~CustomVisionClient();

	esp_err_t detect(camera_fb_t *fbIn, box_array_t **out_boxlist = NULL, char *out_label = NULL, bool drawInfo=false, uint8_t **out_buf = NULL, size_t *out_len = NULL);

	// Asynchronously do the detection, and will send result to a queue upon completion
	esp_err_t detectAsync(camera_fb_t *fbIn);

	// Util methods
	esp_err_t putInfoOnFrame(camera_fb_t *fbIn, box_array_t *boxlist, const char *label, uint8_t ** out_buf, size_t * out_len);
	esp_err_t putLabelOnFrame(camera_fb_t *fbIn, const char *label, int32_t x, int32_t y, uint32_t color, uint8_t ** out_buf, size_t * out_len);

	QueueHandle_t detectionQueue = NULL;

private:
	CustomVisionClientConfig_t clientConfig_ = {};
	CustomVisionDetectionConfig_t *detectionCfg_ = NULL;
};

#endif /* MAIN_CUSTOMVISIONCLIENT_H_ */
