#ifndef BLE_IMPACT_SERVICE_H__
#define BLE_IMPACT_SERVICE_H__
#include <stdint.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_srv_common.h"

#define BLE_UUID_IMPACT_SERVICE_BASE   {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, \
                                        0x00, 0x10, 0x00, 0x00, 0x20, 0x18, 0x00, 0x00}
#define BLE_UUID_IMPACT_SERVICE        0x1820
#define BLE_UUID_TELEMETRY_CHAR        0x2A90
#define BLE_UUID_PING_CHAR             0x2A91
#define BLE_UUID_CONFIG_CHAR           0x2A92

typedef struct ble_impact_service_s ble_impact_service_t;
typedef void (*ble_impact_ping_handler_t)(ble_impact_service_t * p_service, const uint8_t * p_data, uint16_t len);
typedef void (*ble_impact_config_handler_t)(ble_impact_service_t * p_service, const uint8_t * p_data, uint16_t len);

struct ble_impact_service_s {
    uint16_t                    service_handle;
    ble_gatts_char_handles_t    telemetry_handles;
    ble_gatts_char_handles_t    ping_handles;
    ble_gatts_char_handles_t    config_handles;
    uint8_t                     uuid_type;
    uint16_t                    conn_handle;
    bool                        is_notification_enabled;
    ble_impact_ping_handler_t   ping_handler;
    ble_impact_config_handler_t config_handler;
};

uint32_t ble_impact_service_init(ble_impact_service_t * p_service,
                                 ble_impact_ping_handler_t ping_handler,
                                 ble_impact_config_handler_t config_handler);
void ble_impact_service_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context);
uint32_t ble_impact_service_send_telemetry(ble_impact_service_t * p_service, const uint8_t * p_data, uint16_t len);
#endif
