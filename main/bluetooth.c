/****************************************************************************
*
* This demo showcases BLE GATT client.
* It can scan BLE devices and connect to 'AB Shutter3' device.
*
* I used this as a reference.
* https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/ble/gatt_client
* AB Shutter3 has multiple identical UUIDs.
*
****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define GATTC_TAG "GATT_CLIENT"
//#define REMOTE_SERVICE_UUID 0x00FF
#define REMOTE_SERVICE_UUID 0x1812
//#define REMOTE_NOTIFY_CHAR_UUID 0xFF01
#define REMOTE_NOTIFY_CHAR_UUID 0x2a4c
#define PROFILE_NUM		 1
#define PROFILE_A_APP_ID 0
#define INVALID_HANDLE	 0

QueueHandle_t xQueueEvent;

typedef struct {
	int16_t NotificationEvent;
	int16_t NotificationLength;
	uint32_t NotificationValue;
} NOTIFICATION_t;

//static char remote_device_name[ESP_BLE_ADV_NAME_LEN_MAX] = "ESP_GATTS_DEMO";
static char remote_device_name[ESP_BLE_ADV_NAME_LEN_MAX] = "AB Shutter3";
static bool connect    = false;
static bool get_server = false;
static esp_gattc_char_elem_t *char_elem_result	 = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;

/* Declare static functions */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);


static esp_bt_uuid_t remote_filter_service_uuid = {
	.len = ESP_UUID_LEN_16,
	.uuid = {.uuid16 = REMOTE_SERVICE_UUID,},
};

#if 0
static esp_bt_uuid_t remote_filter_char_uuid = {
	.len = ESP_UUID_LEN_16,
	.uuid = {.uuid16 = REMOTE_NOTIFY_CHAR_UUID,},
};

static esp_bt_uuid_t notify_descr_uuid = {
	.len = ESP_UUID_LEN_16,
	.uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};
#endif

static esp_ble_scan_params_t ble_scan_params = {
	.scan_type				= BLE_SCAN_TYPE_ACTIVE,
	.own_addr_type			= BLE_ADDR_TYPE_PUBLIC,
	.scan_filter_policy		= BLE_SCAN_FILTER_ALLOW_ALL,
	.scan_interval			= 0x50,
	.scan_window			= 0x30,
	.scan_duplicate			= BLE_SCAN_DUPLICATE_DISABLE
};

struct gattc_profile_inst {
	esp_gattc_cb_t gattc_cb;
	uint16_t gattc_if;
	uint16_t app_id;
	uint16_t conn_id;
	uint16_t service_start_handle;
	uint16_t service_end_handle;
	uint16_t char_handle;
	esp_bd_addr_t remote_bda;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
	[PROFILE_A_APP_ID] = {
		.gattc_cb = gattc_profile_event_handler,
		.gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
	},
};

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
	esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;
	NOTIFICATION_t Notification;

	switch (event) {
	case ESP_GATTC_REG_EVT:
		ESP_LOGI(GATTC_TAG, "GATT client register, status %d, app_id %d, gattc_if %d", param->reg.status, param->reg.app_id, gattc_if);
		esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
		if (scan_ret){
			ESP_LOGE(GATTC_TAG, "set scan params error, error code = %x", scan_ret);
		}
		break;
	case ESP_GATTC_CONNECT_EVT:{
		ESP_LOGI(GATTC_TAG, "Connected, conn_id %d, remote "ESP_BD_ADDR_STR"", p_data->connect.conn_id,
				 ESP_BD_ADDR_HEX(p_data->connect.remote_bda));
		gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
		memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
		esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
		if (mtu_ret){
			ESP_LOGE(GATTC_TAG, "Config MTU error, error code = %x", mtu_ret);
		}
		break;
	}
	case ESP_GATTC_OPEN_EVT:
		if (param->open.status != ESP_GATT_OK){
			ESP_LOGE(GATTC_TAG, "Open failed, status %d", p_data->open.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "Open successfully, MTU %u", p_data->open.mtu);
		Notification.NotificationEvent = ESP_GATTC_OPEN_EVT;
		xQueueSendFromISR(xQueueEvent, &Notification, NULL);
		break;
	case ESP_GATTC_DIS_SRVC_CMPL_EVT:
		ESP_LOGD(GATTC_TAG, "ESP_GATTC_DIS_SRVC_CMPL_EVT");
		if (param->dis_srvc_cmpl.status != ESP_GATT_OK){
			ESP_LOGE(GATTC_TAG, "Service discover failed, status %d", param->dis_srvc_cmpl.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "Service discover complete, conn_id %d", param->dis_srvc_cmpl.conn_id);
		esp_ble_gattc_search_service(gattc_if, param->dis_srvc_cmpl.conn_id, &remote_filter_service_uuid);
		break;
	case ESP_GATTC_CFG_MTU_EVT:
		ESP_LOGI(GATTC_TAG, "MTU exchange, status %d, MTU %d", param->cfg_mtu.status, param->cfg_mtu.mtu);
		break;
	case ESP_GATTC_SEARCH_RES_EVT: {
		ESP_LOGD(GATTC_TAG, "ESP_GATTC_SEARCH_RES_EVT");
		ESP_LOGI(GATTC_TAG, "Service search result, conn_id = %x, is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
		ESP_LOGI(GATTC_TAG, "start handle %d, end handle %d, current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
		if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID) {
			ESP_LOGI(GATTC_TAG, "Service found");
			get_server = true;
			gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
			gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
			ESP_LOGI(GATTC_TAG, "UUID16: 0x%x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
			ESP_LOGI(GATTC_TAG, "service_start_handle: 0x%x", p_data->search_res.start_handle);
			ESP_LOGI(GATTC_TAG, "service_end_handle: 0x%x", p_data->search_res.end_handle);
		}
		break;
	}
	case ESP_GATTC_SEARCH_CMPL_EVT:
		ESP_LOGD(GATTC_TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
		if (p_data->search_cmpl.status != ESP_GATT_OK){
			ESP_LOGE(GATTC_TAG, "Service search failed, status %x", p_data->search_cmpl.status);
			break;
		}
		if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
			ESP_LOGI(GATTC_TAG, "Get service information from remote device");
		} else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
			ESP_LOGI(GATTC_TAG, "Get service information from flash");
		} else {
			ESP_LOGI(GATTC_TAG, "Unknown service source");
		}
		ESP_LOGI(GATTC_TAG, "Service search complete. get_server=%d", get_server);
		if (get_server){
			uint16_t count = 0;
			esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
				p_data->search_cmpl.conn_id,
				ESP_GATT_DB_CHARACTERISTIC,
				gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
				gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
				INVALID_HANDLE,
				&count);
			if (status != ESP_GATT_OK){
				ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
				break;
			}

			ESP_LOGI(GATTC_TAG, "esp_ble_gattc_get_attr_count count=%d", count);
			if (count > 0){
				char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
				if (!char_elem_result){
					ESP_LOGE(GATTC_TAG, "gattc no mem");
					break;
				}else{
#if 0
					// Find the descriptor with the given characteristic handle in the gattc cache
					status = esp_ble_gattc_get_char_by_uuid( gattc_if,
						p_data->search_cmpl.conn_id,
						gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
						gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
						remote_filter_char_uuid,
						char_elem_result,
						&count);
#endif
					// Find all the characteristic with the given service in the gattc cache
					status = esp_ble_gattc_get_all_char( gattc_if,
						p_data->search_cmpl.conn_id,
						gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
						gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
						char_elem_result,
						&count,
						0);
					if (status != ESP_GATT_OK){
						ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_char_by_uuid error");
						free(char_elem_result);
						char_elem_result = NULL;
						break;
					}
			
					ESP_LOGI(GATTC_TAG, "esp_ble_gattc_get_attr_count count=%d", count);
					for (int i=0;i<count;i++) {
						ESP_LOGI(GATTC_TAG, "char_elem_result[%d] char_handle=0x%x uuid.len=%d properties=0x%x properties=0x%x",
							i, 
							char_elem_result[i].char_handle,
							char_elem_result[i].uuid.len,
							char_elem_result[i].uuid.uuid.uuid16,
							char_elem_result[i].properties);
						if (char_elem_result[i].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY){
							ESP_LOGI(GATTC_TAG, "char_elem_result[%d].char_handle=0x%x has ESP_GATT_CHAR_PROP_BIT_NOTIFY", i, char_elem_result[i].char_handle);
							gl_profile_tab[PROFILE_A_APP_ID].char_handle = char_elem_result[i].char_handle;
							status = esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[i].char_handle);
							if (status != ESP_GATT_OK){
								ESP_LOGE(GATTC_TAG, "esp_ble_gattc_register_for_notify error");
							}
						}
					}

#if 0
					/*	Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
					if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
						gl_profile_tab[PROFILE_A_APP_ID].char_handle = char_elem_result[0].char_handle;
						esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[0].char_handle);
					}
#endif
				}
				/* free char_elem_result */
				free(char_elem_result);
			}else{
				ESP_LOGE(GATTC_TAG, "no char found");
			}
		}
		 break;
	case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
		ESP_LOGD(GATTC_TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
		ESP_LOGI(GATTC_TAG, "p_data->reg_for_notify.handle=0x%x", p_data->reg_for_notify.handle);
		if (p_data->reg_for_notify.status != ESP_GATT_OK){
			ESP_LOGE(GATTC_TAG, "Notification register failed, status %d", p_data->reg_for_notify.status);
		}else{
			ESP_LOGI(GATTC_TAG, "Notification register successfully");
			uint16_t count = 0;
			uint16_t notify_en = 1;
			esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
				gl_profile_tab[PROFILE_A_APP_ID].conn_id,
				ESP_GATT_DB_DESCRIPTOR,
				gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
				gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
				gl_profile_tab[PROFILE_A_APP_ID].char_handle,
				&count);
			if (ret_status != ESP_GATT_OK){
				ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_attr_count error");
				break;
			}
			if (count > 0){
				descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
				if (!descr_elem_result){
					ESP_LOGE(GATTC_TAG, "malloc error, gattc no mem");
					break;
				}else{
#if 0
					// Find the descriptor with the given characteristic handle in the gattc cache
					ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
						gl_profile_tab[PROFILE_A_APP_ID].conn_id,
						p_data->reg_for_notify.handle,
						notify_descr_uuid,
						descr_elem_result,
						&count);
#endif
					// Find all the descriptor with the given characteristic in the gattc cache
					ret_status = esp_ble_gattc_get_all_descr( gattc_if,
						gl_profile_tab[PROFILE_A_APP_ID].conn_id,
						p_data->reg_for_notify.handle,
						descr_elem_result,
						&count,
						0);

					if (ret_status != ESP_GATT_OK){
						ESP_LOGE(GATTC_TAG, "esp_ble_gattc_get_descr_by_char_handle error");
						free(descr_elem_result);
						descr_elem_result = NULL;
						break;
					}

					ESP_LOGI(GATTC_TAG, "esp_ble_gattc_get_attr_count count=%d", count);
					ESP_LOGI(GATTC_TAG, "ESP_GATT_UUID_CHAR_CLIENT_CONFIG=0x%x", ESP_GATT_UUID_CHAR_CLIENT_CONFIG);
					for (int i=0;i<count;i++) {
						ESP_LOGI(GATTC_TAG, "descr_elem_result[%d] handle=0x%x uuid.len=%d uuid.uuid.uuid16=0x%x",
							i,
							descr_elem_result[i].handle,
							descr_elem_result[i].uuid.len,
							descr_elem_result[i].uuid.uuid.uuid16);
						if (descr_elem_result[i].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[i].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
							ESP_LOGI(GATTC_TAG, "descr_elem_result[%d].handle=0x%x has ESP_GATT_UUID_CHAR_CLIENT_CONFIG", i, descr_elem_result[i].handle);
							ret_status = esp_ble_gattc_write_char_descr( gattc_if,
								gl_profile_tab[PROFILE_A_APP_ID].conn_id,
								descr_elem_result[i].handle,
								sizeof(notify_en),
								(uint8_t *)&notify_en,
								ESP_GATT_WRITE_TYPE_RSP,
								ESP_GATT_AUTH_REQ_NONE);
						}
					}

#if 0
					/* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
					if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
						ret_status = esp_ble_gattc_write_char_descr( gattc_if,
							gl_profile_tab[PROFILE_A_APP_ID].conn_id,
							descr_elem_result[0].handle,
							sizeof(notify_en),
							(uint8_t *)&notify_en,
							ESP_GATT_WRITE_TYPE_RSP,
							ESP_GATT_AUTH_REQ_NONE);
					}
#endif

					if (ret_status != ESP_GATT_OK){
						ESP_LOGE(GATTC_TAG, "esp_ble_gattc_write_char_descr error");
					}

					/* free descr_elem_result */
					free(descr_elem_result);
				}
			}
			else{
				ESP_LOGE(GATTC_TAG, "decsr not found");
			}

		}
		break;
	}
	case ESP_GATTC_NOTIFY_EVT:
		if (p_data->notify.is_notify){
			ESP_LOGI(GATTC_TAG, "Notification received. p_data->notify.value_len=%d", p_data->notify.value_len);
		}else{
			ESP_LOGI(GATTC_TAG, "Indication received.");
		}
		esp_log_buffer_hex(GATTC_TAG, p_data->notify.value, p_data->notify.value_len);
		Notification.NotificationEvent = ESP_GATTC_NOTIFY_EVT;
		Notification.NotificationValue = 0;
		Notification.NotificationLength = p_data->notify.value_len;
		for (int i=0;i<p_data->notify.value_len;i++) {
			Notification.NotificationValue = Notification.NotificationValue << 8;
			Notification.NotificationValue = Notification.NotificationValue | p_data->notify.value[i];
		}
		ESP_LOGI(GATTC_TAG, "NotificationValue=0x%"PRIx32, Notification.NotificationValue);
		xQueueSendFromISR(xQueueEvent, &Notification, NULL);
		break;
	case ESP_GATTC_WRITE_DESCR_EVT:
		ESP_LOGD(GATTC_TAG, "ESP_GATTC_WRITE_DESCR_EVT");
		ESP_LOGI(GATTC_TAG, "p_data->write.handle=0x%x", p_data->write.handle);
		if (p_data->write.status != ESP_GATT_OK){
			ESP_LOGE(GATTC_TAG, "Descriptor write failed, status %x", p_data->write.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "Descriptor write successfully");
#if 0
		uint8_t write_char_data[35];
		for (int i = 0; i < sizeof(write_char_data); ++i)
		{
			write_char_data[i] = i % 256;
		}
		esp_ble_gattc_write_char( gattc_if,
			gl_profile_tab[PROFILE_A_APP_ID].conn_id,
			gl_profile_tab[PROFILE_A_APP_ID].char_handle,
			sizeof(write_char_data),
			write_char_data,
			ESP_GATT_WRITE_TYPE_RSP,
			ESP_GATT_AUTH_REQ_NONE);
#endif
		break;
	case ESP_GATTC_SRVC_CHG_EVT: {
		esp_bd_addr_t bda;
		memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
		ESP_LOGI(GATTC_TAG, "Service change from "ESP_BD_ADDR_STR"", ESP_BD_ADDR_HEX(bda));
		break;
	}
	case ESP_GATTC_WRITE_CHAR_EVT:
		if (p_data->write.status != ESP_GATT_OK){
			ESP_LOGE(GATTC_TAG, "Characteristic write failed, status %x)", p_data->write.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "Characteristic write successfully");
		break;
	case ESP_GATTC_DISCONNECT_EVT:
		connect = false;
		get_server = false;
		ESP_LOGI(GATTC_TAG, "Disconnected, remote "ESP_BD_ADDR_STR", reason 0x%02x",
			ESP_BD_ADDR_HEX(p_data->disconnect.remote_bda), p_data->disconnect.reason);

		// Unregister an application from the GATTC module
		ESP_LOGI(GATTC_TAG, "CleanUp registerd application");
		ESP_ERROR_CHECK(esp_ble_gattc_app_unregister(gattc_if));

		Notification.NotificationEvent = ESP_GATTC_DISCONNECT_EVT;
		xQueueSendFromISR(xQueueEvent, &Notification, NULL);
		break;
	default:
		break;
	}
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
	uint8_t *adv_name = NULL;
	uint8_t adv_name_len = 0;
	switch (event) {
	case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
		ESP_LOGD(GATTC_TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
		//the unit of the duration is second
		uint32_t duration = 30;
		esp_ble_gap_start_scanning(duration);
		break;
	}
	case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
		ESP_LOGD(GATTC_TAG, "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");
		//scan start complete event to indicate scan start successfully or failed
		if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
			ESP_LOGE(GATTC_TAG, "Scanning start failed, status %x", param->scan_start_cmpl.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "Scanning start successfully");

		break;
	case ESP_GAP_BLE_SCAN_RESULT_EVT: {
		ESP_LOGD(GATTC_TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT");
		esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
		switch (scan_result->scan_rst.search_evt) {
		case ESP_GAP_SEARCH_INQ_RES_EVT:
			adv_name = esp_ble_resolve_adv_data_by_type(scan_result->scan_rst.ble_adv,
				scan_result->scan_rst.adv_data_len + scan_result->scan_rst.scan_rsp_len,
				ESP_BLE_AD_TYPE_NAME_CMPL,
				&adv_name_len);
			ESP_LOGD(GATTC_TAG, "Scan result, device "ESP_BD_ADDR_STR", name len %u", ESP_BD_ADDR_HEX(scan_result->scan_rst.bda), adv_name_len);
			ESP_LOG_BUFFER_CHAR(GATTC_TAG, adv_name, adv_name_len);

			if (adv_name != NULL) {
				if (strlen(remote_device_name) == adv_name_len && strncmp((char *)adv_name, remote_device_name, adv_name_len) == 0) {
					ESP_LOGI(GATTC_TAG, "Device found %s", remote_device_name);
					if (connect == false) {
						connect = true;
						ESP_LOGI(GATTC_TAG, "Connect to the remote device");
						esp_ble_gap_stop_scanning();
						esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
						ESP_LOGI(GATTC_TAG, "Connected to the remote device");
					}
				}
			}
			break;
		case ESP_GAP_SEARCH_INQ_CMPL_EVT:
			ESP_LOGD(GATTC_TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT");
			NOTIFICATION_t Notification;
			Notification.NotificationEvent = ESP_GAP_SEARCH_INQ_CMPL_EVT;
			xQueueSendFromISR(xQueueEvent, &Notification, NULL);
			break;
		default:
			break;
		}
		break;
	}

	case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
		if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
			ESP_LOGE(GATTC_TAG, "Scanning stop failed, status %x", param->scan_stop_cmpl.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "Scanning stop successfully");
		break;

	case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
		if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
			ESP_LOGE(GATTC_TAG, "Advertising stop failed, status %x", param->adv_stop_cmpl.status);
			break;
		}
		ESP_LOGI(GATTC_TAG, "Advertising stop successfully");
		break;
	case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
		 ESP_LOGI(GATTC_TAG, "Connection params update, status %d, conn_int %d, latency %d, timeout %d",
			 param->update_conn_params.status,
			 param->update_conn_params.conn_int,
			 param->update_conn_params.latency,
			 param->update_conn_params.timeout);
		break;
	case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
		ESP_LOGI(GATTC_TAG, "Packet length update, status %d, rx %d, tx %d",
			 param->pkt_data_length_cmpl.status,
			 param->pkt_data_length_cmpl.params.rx_len,
			 param->pkt_data_length_cmpl.params.tx_len);
		break;
	default:
		break;
	}
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
	/* If event is register event, store the gattc_if for each profile */
	if (event == ESP_GATTC_REG_EVT) {
		if (param->reg.status == ESP_GATT_OK) {
			gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
		} else {
			ESP_LOGI(GATTC_TAG, "reg app failed, app_id %04x, status %d", param->reg.app_id, param->reg.status);
			return;
		}
	}

	/* If the gattc_if equal to profile A, call profile A cb handler,
	 * so here call each profile's callback */
	do {
		int idx;
		for (idx = 0; idx < PROFILE_NUM; idx++) {
			if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
					gattc_if == gl_profile_tab[idx].gattc_if) {
				if (gl_profile_tab[idx].gattc_cb) {
					gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
				}
			}
		}
	} while (0);
}

void bluetooth(void* pvParameters)
{
	TaskHandle_t taskHandle = (TaskHandle_t)pvParameters;

	if (CONFIG_LED_CONNECTED_GPIO != -1) {
		gpio_reset_pin(CONFIG_LED_CONNECTED_GPIO);
		gpio_set_direction(CONFIG_LED_CONNECTED_GPIO, GPIO_MODE_OUTPUT);
		gpio_set_level(CONFIG_LED_CONNECTED_GPIO, 0);
	}

	if (CONFIG_LED_TRIGGER_GPIO != -1) {
		gpio_reset_pin(CONFIG_LED_TRIGGER_GPIO);
		gpio_set_direction(CONFIG_LED_TRIGGER_GPIO, GPIO_MODE_OUTPUT);
		gpio_set_level(CONFIG_LED_TRIGGER_GPIO, 0);
	}

	// Create Queue
	xQueueEvent = xQueueCreate(10, sizeof(NOTIFICATION_t));
	configASSERT( xQueueEvent );

	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));

	while(1) {
		ESP_LOGI(GATTC_TAG, "Start esp_bt_controller_enable");
		esp_err_t ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
		if (ret) {
			ESP_LOGE(GATTC_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
			break;
		}

		ESP_LOGI(GATTC_TAG, "Start esp_bluedroid_init");
		ret = esp_bluedroid_init();
		if (ret) {
			ESP_LOGE(GATTC_TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
			break;
		}
	
		ESP_LOGI(GATTC_TAG, "Start esp_bluedroid_enable");
		ret = esp_bluedroid_enable();
		if (ret) {
			ESP_LOGE(GATTC_TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
			break;
		}

		//register the	callback function to the gap module
		ESP_LOGI(GATTC_TAG, "Start esp_ble_gap_register_callback");
		ret = esp_ble_gap_register_callback(esp_gap_cb);
		if (ret){
			ESP_LOGE(GATTC_TAG, "%s gap register failed, error code = %x", __func__, ret);
			break;
		}

		//register the callback function to the gattc module
		ESP_LOGI(GATTC_TAG, "Start esp_ble_gattc_register_callback");
		ret = esp_ble_gattc_register_callback(esp_gattc_cb);
		if(ret){
			ESP_LOGE(GATTC_TAG, "%s gattc register failed, error code = %x", __func__, ret);
			break;
		}

		ESP_LOGI(GATTC_TAG, "Start esp_ble_gattc_app_register");
		ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
		if (ret){
			ESP_LOGE(GATTC_TAG, "%s gattc app register failed, error code = %x", __func__, ret);
		}

		ESP_LOGI(GATTC_TAG, "Start esp_ble_gatt_set_local_mtu");
		esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
		if (local_mtu_ret){
			ESP_LOGE(GATTC_TAG, "set local	MTU failed, error code = %x", local_mtu_ret);
		}

		NOTIFICATION_t event;
		while (1) {
			xQueueReceive(xQueueEvent, &event, portMAX_DELAY);
			ESP_LOGI(GATTC_TAG, "event.NotificationEvent=0x%x", event.NotificationEvent);
			if (event.NotificationEvent == ESP_GAP_SEARCH_INQ_CMPL_EVT) { // 0x01
				ESP_LOGW(GATTC_TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT");
				if (CONFIG_LED_CONNECTED_GPIO != -1)
					gpio_set_level(CONFIG_LED_CONNECTED_GPIO, 0);
				break;
			} else if (event.NotificationEvent == ESP_GATTC_DISCONNECT_EVT) { // 0x29
				ESP_LOGW(GATTC_TAG, "ESP_GATTC_DISCONNECT_EVT");
				ESP_LOGW(GATTC_TAG, "Disconnected from the remote device");
				if (CONFIG_LED_CONNECTED_GPIO != -1)
					gpio_set_level(CONFIG_LED_CONNECTED_GPIO, 0);
				break;
			} else if (event.NotificationEvent == ESP_GATTC_OPEN_EVT) { // 0x02
				ESP_LOGW(GATTC_TAG, "ESP_GATTC_OPEN_EVT");
				ESP_LOGW(GATTC_TAG, "Connected to the remote device");
				if (CONFIG_LED_CONNECTED_GPIO != -1)
					gpio_set_level(CONFIG_LED_CONNECTED_GPIO, 1);
			} else if (event.NotificationEvent == ESP_GATTC_NOTIFY_EVT) { // 0x0A
				ESP_LOGW(GATTC_TAG, "ESP_GATTC_NOTIFY_EVT");
				ESP_LOGI(GATTC_TAG, "event.NotificationLength=%d", event.NotificationLength);
				ESP_LOGI(GATTC_TAG, "event.NotificationValue=0x%"PRIx32, event.NotificationValue);
				if (event.NotificationValue != 0x0) continue;
				if (CONFIG_LED_CONNECTED_GPIO != -1)
					gpio_set_level(CONFIG_LED_CONNECTED_GPIO, 0);
				xTaskNotifyGive(taskHandle);
				if (CONFIG_LED_TRIGGER_GPIO != -1) {
					for (int i=0;i<5;i++) {
						gpio_set_level(CONFIG_LED_TRIGGER_GPIO, 1);
						vTaskDelay(2);
						gpio_set_level(CONFIG_LED_TRIGGER_GPIO, 0);
						vTaskDelay(2);
					}
				}
				if (CONFIG_LED_CONNECTED_GPIO != -1)
					gpio_set_level(CONFIG_LED_CONNECTED_GPIO, 1);
			}
		}

		// Disable bluetooth
		ESP_ERROR_CHECK(esp_bluedroid_disable());

		// Deinit and free the resource for bluetooth
		ESP_ERROR_CHECK(esp_bluedroid_deinit());

		// Disable Bluetooth Controller
		ESP_ERROR_CHECK(esp_bt_controller_disable());

	} // end Scan

	// Never reach here
	ESP_LOGI(GATTC_TAG, "All done");
	vTaskDelete(NULL);

}
