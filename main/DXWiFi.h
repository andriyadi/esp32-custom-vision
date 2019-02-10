/*
 * DXWiFi.h
 *
 *	Inspired by and most code taken from: https://github.com/espressif/esp-iot-solution/blob/master/components/wifi/wifi_conn
 *
 *  Created on: Feb 8, 2019
 *      Author: andri
 */

#ifndef MAIN_DXWIFI_H_
#define MAIN_DXWIFI_H_

#include "esp_log.h"
#include "esp_wifi.h"

#define WIFI_CONNECTED_EVT	BIT0
#define WIFI_STOP_REQ_EVT   BIT1

typedef enum {
    WIFI_STATUS_STA_DISCONNECTED,           /**< ESP32 station disconnected */
    WIFI_STATUS_STA_CONNECTING,             /**< ESP32 station connecting */
    WIFI_STATUS_STA_CONNECTED,              /**< ESP32 station connected */
} wifi_sta_status_t;

class DXWiFi {
public:

	/**
	 * @brief get the only instance of CWiFi
	 *
	 * @param product_info the information of product
	 *
	 * @return pointer to a CWiFi instance
	 */
	static DXWiFi* GetInstance(wifi_mode_t mode = WIFI_MODE_STA);

	/**
	 * @brief  wifi connect with timeout
	 *
	 * @param  ssid ssid of the target AP
	 * @param  pwd password of the target AP
	 * @param  ticks_to_wait system tick number to wait
	 *
	 * @return
	 *     - ESP_OK: connect to AP successfully
	 *     - ESP_TIMEOUT: timeout
	 *     - ESP_FAIL: fail
	 */
	esp_err_t ConnectSync(const char *ssid, const char *pwd, uint32_t ticks_to_wait);

	/**
	 *  @brief WiFi stop connecting
	 */
	void Disconnect();

	/**
	 * @brief  get wifi status.
	 *
	 * @return status of the wifi station
	 */
	wifi_sta_status_t Status();

private:
    static DXWiFi* m_instance;

    /**
	 * prevent constructing in singleton mode
	 */
    DXWiFi(const DXWiFi&);
    DXWiFi& operator=(const DXWiFi&);

	/**
	 * @brief  constructor of CLED
	 *
	 * @param  mode refer to enum wifi_mode_t
	 */
    DXWiFi(wifi_mode_t mode = WIFI_MODE_STA);

	/**
	 * prevent memory leak of m_instance
	 */
	class CGarbo
	{
	public:
		~CGarbo()
		{
			if (DXWiFi::m_instance) {
				delete DXWiFi::m_instance;
			}
		}
	};
	static CGarbo garbo;

};

#endif /* MAIN_DXWIFI_H_ */
