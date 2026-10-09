/**
 * @file ble_impact_service.c
 * @brief Custom BLE GATT service implementation for Impact Time Meter
 */
#include "ble_impact_service.h"
#include "nordic_common.h"
#include "nrf_log.h"
#include <string.h>

static void on_connect(ble_impact_service_t * p_service, ble_evt_t const * p_ble_evt) {
    p_service->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    NRF_LOG_INFO("[BLE] Connected! Conn Handle: %d", p_service->conn_handle);
}

static void on_disconnect(ble_impact_service_t * p_service, ble_evt_t const * p_ble_evt) {
    UNUSED_PARAMETER(p_ble_evt);
    p_service->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_service->is_notification_enabled = false;
    NRF_LOG_INFO("[BLE] Disconnected");
}

static void on_write(ble_impact_service_t * p_service, ble_evt_t const * p_ble_evt) {
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;
    if (p_evt_write->handle == p_service->ping_handles.value_handle) {
        if (p_service->ping_handler != NULL) {
            p_service->ping_handler(p_service, p_evt_write->data, p_evt_write->len);
        }
        ble_gatts_value_t gatts_value;
        memset(&gatts_value, 0, sizeof(gatts_value));
        gatts_value.len     = p_evt_write->len;
        gatts_value.offset  = 0;
        gatts_value.p_value = (uint8_t *)p_evt_write->data;
        sd_ble_gatts_value_set(p_service->conn_handle, p_service->ping_handles.value_handle, &gatts_value);
    } else if (p_evt_write->handle == p_service->config_handles.value_handle) {
        if (p_service->config_handler != NULL) {
            p_service->config_handler(p_service, p_evt_write->data, p_evt_write->len);
        }
    } else if (p_evt_write->handle == p_service->telemetry_handles.cccd_handle && p_evt_write->len == 2) {
        p_service->is_notification_enabled = ble_srv_is_notification_enabled(p_evt_write->data);
        NRF_LOG_INFO("[BLE] Telemetry notifications %s", p_service->is_notification_enabled ? "ENABLED" : "DISABLED");
    }
}

void ble_impact_service_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context) {
    ble_impact_service_t * p_service = (ble_impact_service_t *)p_context;
    if (p_service == NULL || p_ble_evt == NULL) return;
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_service, p_ble_evt);
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_service, p_ble_evt);
            break;
        case BLE_GATTS_EVT_WRITE:
            on_write(p_service, p_ble_evt);
            break;
        default:
            break;
    }
}

uint32_t ble_impact_service_init(ble_impact_service_t * p_service,
                                 ble_impact_ping_handler_t ping_handler,
                                 ble_impact_config_handler_t config_handler) {
    uint32_t              err_code;
    ble_uuid_t            ble_uuid;
    p_service->conn_handle = BLE_CONN_HANDLE_INVALID;
    p_service->is_notification_enabled = false;
    p_service->ping_handler = ping_handler;
    p_service->config_handler = config_handler;

    p_service->uuid_type = BLE_UUID_TYPE_BLE;
    ble_uuid.type = BLE_UUID_TYPE_BLE;
    ble_uuid.uuid = BLE_UUID_IMPACT_SERVICE;

    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_service->service_handle);
    VERIFY_SUCCESS(err_code);

    ble_add_char_params_t add_char_params;
    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = BLE_UUID_TELEMETRY_CHAR;
    add_char_params.uuid_type         = BLE_UUID_TYPE_BLE;
    add_char_params.char_props.notify = 1;
    add_char_params.char_props.read   = 1;
    add_char_params.max_len           = 64;
    add_char_params.is_var_len        = true;
    add_char_params.cccd_write_access = SEC_OPEN;
    add_char_params.read_access       = SEC_OPEN;
    err_code = characteristic_add(p_service->service_handle, &add_char_params, &p_service->telemetry_handles);
    VERIFY_SUCCESS(err_code);

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = BLE_UUID_PING_CHAR;
    add_char_params.uuid_type         = BLE_UUID_TYPE_BLE;
    add_char_params.char_props.read   = 1;
    add_char_params.char_props.write  = 1;
    add_char_params.char_props.write_wo_resp = 1;
    add_char_params.max_len           = 32;
    add_char_params.is_var_len        = true;
    add_char_params.read_access       = SEC_OPEN;
    add_char_params.write_access      = SEC_OPEN;
    err_code = characteristic_add(p_service->service_handle, &add_char_params, &p_service->ping_handles);
    VERIFY_SUCCESS(err_code);

    memset(&add_char_params, 0, sizeof(add_char_params));
    add_char_params.uuid              = BLE_UUID_CONFIG_CHAR;
    add_char_params.uuid_type         = BLE_UUID_TYPE_BLE;
    add_char_params.char_props.read   = 1;
    add_char_params.char_props.write  = 1;
    add_char_params.max_len           = 32;
    add_char_params.is_var_len        = true;
    add_char_params.read_access       = SEC_OPEN;
    add_char_params.write_access      = SEC_OPEN;
    err_code = characteristic_add(p_service->service_handle, &add_char_params, &p_service->config_handles);
    VERIFY_SUCCESS(err_code);

    return NRF_SUCCESS;
}

uint32_t ble_impact_service_send_telemetry(ble_impact_service_t * p_service, 
                                          const uint8_t * p_data, 
                                          uint16_t len) {
    if (p_service->conn_handle == BLE_CONN_HANDLE_INVALID || !p_service->is_notification_enabled) {
        return NRF_ERROR_INVALID_STATE;
    }
    ble_gatts_hvx_params_t hvx_params;
    memset(&hvx_params, 0, sizeof(hvx_params));
    hvx_params.handle = p_service->telemetry_handles.value_handle;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
    hvx_params.offset = 0;
    hvx_params.p_len  = &len;
    hvx_params.p_data = (uint8_t *)p_data;
    return sd_ble_gatts_hvx(p_service->conn_handle, &hvx_params);
}
