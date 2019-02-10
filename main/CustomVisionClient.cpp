/*
 * CustomVisionClient.cpp
 *
 *  Created on: Jan 28, 2019
 *      Author: andri
 */

#include "CustomVisionClient.h"
#include "esp_http_client.h"
#include <string>
#include "cJSON.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"

std::string httpResponseString;

CustomVisionClient::CustomVisionClient(const CustomVisionClientConfig_t& config): clientConfig_(config) {
}

CustomVisionClient::~CustomVisionClient() {
	// TODO Auto-generated destructor stub
}

static void rgb_print(uint8_t *data, size_t image_width, size_t image_height, int32_t x, int32_t y, uint32_t color, const char * str){
    fb_data_t fb;
    fb.width = image_width;
    fb.height = image_height;
    fb.data = data;
    fb.bytes_per_pixel = 3;
    fb.format = FB_BGR888;
    fb_gfx_print(&fb, x, y, color, str);
}

esp_err_t CustomVisionClient::putLabelOnFrame(camera_fb_t* fbIn,
		const char* label, int32_t x, int32_t y, uint32_t color, uint8_t** out_buf,
		size_t* out_len) {

	esp_err_t err = ESP_OK;
	uint8_t *rgb888_out_buf = NULL;

	//SUPER BUGS!
	rgb888_out_buf = (uint8_t*)calloc((fbIn->width*fbIn->height*3), sizeof(uint8_t));
//	rgb888_out_buf = (uint8_t*)malloc((fbIn->width*fbIn->height*3));
//	rgb888_out_buf = (uint8_t*)heap_caps_calloc((fbIn->width*fbIn->height*3), sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

	//First, convert JPEG to RGB888 format
	bool converted = fmt2rgb888(fbIn->buf, fbIn->len, fbIn->format, rgb888_out_buf);
	if (converted) {

		rgb_print(rgb888_out_buf, fbIn->width, fbIn->height, x, y, color, label);

		//convert back RGB888 to JPEG
		if(!fmt2jpg(rgb888_out_buf, fbIn->width*fbIn->height*3, fbIn->width, fbIn->height, PIXFORMAT_RGB888, 80, out_buf, out_len)){
			CVC_ERROR("fmt2jpg failed");
			err = ESP_FAIL;
		}
	}
	else {
		CVC_ERROR("fmt2rgb888 failed");
		err = ESP_FAIL;
	}

	free(rgb888_out_buf);

	return err;
}

esp_err_t CustomVisionClient::putInfoOnFrame(camera_fb_t *fbIn, box_array_t *boxlist, const char *label, uint8_t ** out_buf, size_t * out_len) {

	esp_err_t err = ESP_OK;
	uint8_t *rgb888_out_buf = NULL;

	//SUPER BUGS!
	rgb888_out_buf = (uint8_t*)calloc((fbIn->width*fbIn->height*3), sizeof(uint8_t));
//	rgb888_out_buf = (uint8_t*)malloc((fbIn->width*fbIn->height*3));
//	rgb888_out_buf = (uint8_t*)heap_caps_calloc((fbIn->width*fbIn->height*3), sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

	//First, convert JPEG to RGB888 format
	bool converted = fmt2rgb888(fbIn->buf, fbIn->len, fbIn->format, rgb888_out_buf);
	if (converted) {

		fb_data_t fb;
		fb.width = fbIn->width;
		fb.height = fbIn->height;
		fb.data = rgb888_out_buf;
		fb.bytes_per_pixel = 3;
		fb.format = FB_BGR888;

		int x = 0, y = 0, w, h, i;
		for (i = 0; i < boxlist->len; i++){
			// Rectangle coordinate
			x = (int)boxlist->box[i].box_p[0];
			y = (int)boxlist->box[i].box_p[1];
			w = (int)boxlist->box[i].box_p[2] - x + 1;
			h = (int)boxlist->box[i].box_p[3] - y + 1;

			// Draw it
			fb_gfx_drawFastHLine(&fb, x, y, w, FACE_COLOR_BLUE);
			fb_gfx_drawFastHLine(&fb, x, y+h-1, w, FACE_COLOR_BLUE);
			fb_gfx_drawFastVLine(&fb, x, y, h, FACE_COLOR_BLUE);
			fb_gfx_drawFastVLine(&fb, x+w-1, y, h, FACE_COLOR_BLUE);
		}

		//Draw label above the bounding rectangle
		bool atTop = boxlist->box[0].box_p[1] > 18;
		int yLabel = atTop? (boxlist->box[0].box_p[1] - 22): boxlist->box[0].box_p[3] + 4;
		int xLabel = boxlist->box[0].box_p[0] + (((boxlist->box[0].box_p[2] - boxlist->box[0].box_p[0]) - (strlen(label) * 14)) / 2);

//		CVC_DEBUG("Label on x:%d, y:%d", xLabel, yLabel);
		fb_gfx_print(&fb, xLabel, yLabel, FACE_COLOR_BLUE, label);

		//convert back RGB888 to JPEG
		if(!fmt2jpg(rgb888_out_buf, fbIn->width*fbIn->height*3, fbIn->width, fbIn->height, PIXFORMAT_RGB888, 80, out_buf, out_len)){
			CVC_ERROR("fmt2jpg failed");
			err = ESP_FAIL;
		}
	}
	else {
		CVC_ERROR("fmt2rgb888 failed");
		err = ESP_FAIL;
	}

	free(rgb888_out_buf);

	if (err != ESP_OK) {
		CVC_ERROR("SOMETHING FAILED!");
		*out_buf = fbIn->buf;
		*out_len = fbIn->len;
	}

	return err;
}

esp_err_t process_json(const char *jsonStr, size_t imgWidth, size_t imgHeight, box_array_t **boxlistout, char *label) {

	cJSON *root = cJSON_Parse(jsonStr);
	if (!root) {
		return ESP_FAIL;
	}

	cJSON *predictions = cJSON_GetObjectItem(root, "predictions");
	if (!predictions) {
		return ESP_FAIL;
	}

	float maxProb = 0; int maxProbIdx = 0, idx = 0;
	cJSON *prediction = NULL;

	cJSON_ArrayForEach(prediction, predictions) {
		cJSON *probability = cJSON_GetObjectItem(prediction, "probability");
		cJSON *tagName = cJSON_GetObjectItem(prediction, "tagName");
		double probF = probability->valuedouble;
		if (probF > maxProb) {
			maxProb = probF;
			maxProbIdx = idx;
		}

		CVC_DEBUG("Prediction => idx: %d, tag: %s. prob: %.2f%s", idx, tagName->valuestring, (probF*100), "%");
		idx++;
	}

	cJSON *bestPrediction = cJSON_GetArrayItem(predictions, maxProbIdx);
	cJSON *bestProbability = cJSON_GetObjectItem(bestPrediction, "probability");
	cJSON *bestTagName = cJSON_GetObjectItem(bestPrediction, "tagName");
	double bestProbF = bestProbability->valuedouble;

	CVC_DEBUG("Best prediction => idx: %d, tag: %s. prob: %.2f%s", maxProbIdx, bestTagName->valuestring, (bestProbF*100), "%");

	if (bestProbF < 0.5) {
		CVC_ERROR("Best prediction is NOT enough!");
		return ESP_OK;
	}

	if (label) {
		sprintf(label, "%s (%.2f%s)", bestTagName->valuestring, (bestProbF*100), "%");
	}

	cJSON *boundingBox = cJSON_GetObjectItem(bestPrediction, "boundingBox");
	if (boundingBox) {
		cJSON *left = cJSON_GetObjectItem(boundingBox, "left");
		cJSON *top = cJSON_GetObjectItem(boundingBox, "top");
		cJSON *width = cJSON_GetObjectItem(boundingBox, "width");
		cJSON *height = cJSON_GetObjectItem(boundingBox, "height");

		double leftF = left->valuedouble * imgWidth;
		double topF = top->valuedouble * imgHeight;
		double widthF = width->valuedouble * imgWidth;
		double heightF = height->valuedouble * imgHeight;

		CVC_DEBUG("Bounding box: (%.2f, %.2f, %.2f, %.2f)", leftF, topF, widthF, heightF);

		box_array_t *boxlist = (box_array_t*)calloc(1, sizeof(box_array_t));
		box_t *box = (box_t*)calloc(1, sizeof(box_t));
		box->box_p[0] = leftF;
		box->box_p[1] = topF;
		box->box_p[2] = leftF + widthF - 1;
		box->box_p[3] = topF + heightF - 1;

		boxlist->box = box;
		boxlist->len = 1;

		*boxlistout = boxlist;
	}
	else {
		*boxlistout = NULL;
	}

	return ESP_OK;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            CVC_ERROR("HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
        	CVC_DEBUG("HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
        	CVC_DEBUG("HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
        	CVC_DEBUG("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
        	CVC_DEBUG("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
//              printf("%.*s", evt->data_len, (char*)evt->data);
            	httpResponseString.append((char*)evt->data, evt->data_len);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
        	CVC_DEBUG("HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
        	CVC_DEBUG("HTTP_EVENT_DISCONNECTED");
            break;
    }

    return ESP_OK;
}

esp_err_t CustomVisionClient::detect(camera_fb_t *fbIn, box_array_t **out_boxlist, char *out_label, bool drawInfo, uint8_t **out_buf, size_t *out_len) {
	esp_err_t err = ESP_OK;

	httpResponseString.clear();

	//Create HTTP client
	char url[255];
//https://southcentralus.api.cognitive.microsoft.com/customvision/v2.0/Prediction/28bdc115-5ec4-48e5-aa96-0f627d67137d/image?iterationId=dce22280-1420-4be9-b2f4-95177def027f
	snprintf(url, 255, "https://%s/customvision/v2.0/Prediction/%s/image?iterationId=%s", clientConfig_.host, clientConfig_.projectId, clientConfig_.iterationId);
	CVC_DEBUG("URL %s", url);

	esp_http_client_config_t config = {};
	config.url = url;
	config.is_async = false;
	config.event_handler = _http_event_handler;
	config.timeout_ms = 30000;

	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_http_client_set_method(client, HTTP_METHOD_POST);
	esp_http_client_set_header(client, "Content-Type", "application/octet-stream");
	esp_http_client_set_header(client, "Prediction-Key", clientConfig_.predictionKey);
	esp_http_client_set_header(client, "User-Agent", "ESP32");

	//Post the image to Azure Custom Vision
	esp_http_client_set_post_field(client, (char*)fbIn->buf, fbIn->len);

	while (1) {
		err = esp_http_client_perform(client);
		if (err != ESP_ERR_HTTP_EAGAIN) {
			break;
		}
	}

	if (err == ESP_OK) {
		CVC_DEBUG("HTTPS Status = %d, content_length = %d",
				esp_http_client_get_status_code(client),
				esp_http_client_get_content_length(client));
	} else {
		CVC_ERROR("Error perform http request %s", esp_err_to_name(err));
	}

	int content_length =  esp_http_client_get_content_length(client);
	if (content_length < 0) {
		CVC_ERROR("Content-Length is not found");
		err = ESP_FAIL;
	}

	CVC_DEBUG("HTTPS Status = %d, content_length = %d", esp_http_client_get_status_code(client), content_length);

	//Get the prediction response (should be in JSON) from Azure Custom Vision
	CVC_DEBUG("Response: %s", httpResponseString.c_str());

	//esp_http_client_close(client);
	esp_http_client_cleanup(client);

	if (err != ESP_OK) {
		httpResponseString.clear();
		return err;
	}

	//Parse json
	box_array_t *box_list = NULL;
	char *label = (out_label)? out_label: (char*)calloc(64, sizeof(char));

	err = process_json(httpResponseString.c_str(), fbIn->width, fbIn->height, &box_list, label);
	if (err != ESP_OK) {
		CVC_ERROR("JSON parsing is failed");
	}
	else if (box_list != NULL){

		CVC_DEBUG("Label: %s", label);

		if (!out_boxlist || drawInfo) {
			//Draw box on top of frame buffer
			putInfoOnFrame(fbIn, box_list, label, out_buf, out_len);
			free(box_list->box);
			free(box_list);
		}
		else {
			if (out_boxlist) {
				*out_boxlist = box_list;
			}
//			if (out_label) {
//				strncpy(out_label, label, strlen(label));
//			}
		}
	}

	if (!out_label) {
		free(label);
	}

	httpResponseString.clear();

	return err;
}

void predict_async_task(void *args) {

	CVC_DEBUG("Begin Prediction");

	CustomVisionClient::CustomVisionDetectionConfig_t *predCfg = (CustomVisionClient::CustomVisionDetectionConfig_t*)args;

	box_array_t *box_list = NULL;
	char *label = (char*)calloc(64, sizeof(char));

	// Do "blocking" detection
	esp_err_t res = predCfg->client->detect(predCfg->camera_fb, &box_list, label);
	if (res != ESP_OK) {
		CVC_ERROR("Detection is failed");
		free(label);
	}
	else {
		CVC_DEBUG("Detection is success");
		CustomVisionClient::CustomVisionDetectionResult_t res = {};

		res.box_list = box_list;
		res.label = label;

		xQueueSend(*predCfg->queue, &res, portMAX_DELAY);
	}

	vTaskDelete(NULL);
}

esp_err_t CustomVisionClient::detectAsync(camera_fb_t* fbIn) {

	if (!detectionQueue) {
		detectionQueue = xQueueCreate(2, sizeof(CustomVisionDetectionResult_t));
	}

	if (detectionCfg_ == NULL) {
		detectionCfg_ = new CustomVisionDetectionConfig_t();
	}

	detectionCfg_->camera_fb = fbIn;
	detectionCfg_->queue = &detectionQueue;
	detectionCfg_->client = this;

	BaseType_t res = xTaskCreate(&predict_async_task, "detect_task", 8192, (void*)detectionCfg_, configMAX_PRIORITIES - 3, NULL);
	if (res == pdTRUE) {
		return ESP_OK;
	}

	CVC_ERROR("Failed creating task");
	return ESP_FAIL;
}
