/**
 * @file main.c
 * @brief Impact Time Meter - Master Unit Firmware
 * Target: Nordic nRF52840 DK (PCA10056)
 */
#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "app_error.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_srv_common.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "app_timer.h"
#include "bsp_btn_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "ble_impact_service.h"
#include "timeslot.h"

#define DEVICE_NAME                     "ImpactMaster"
#define APP_BLE_OBSERVER_PRIO           3
#define APP_BLE_CONN_CFG_TAG            1

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(50, UNIT_1_25_MS)
#define SLAVE_LATENCY                   0
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)

NRF_BLE_GATT_DEF(m_gatt);
NRF_BLE_QWR_DEF(m_qwr);
BLE_ADVERTISING_DEF(m_advertising);

static ble_impact_service_t m_impact_service;
static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

static void on_sensor_telemetry_received(const sensor_telemetry_t * p_data) {
    NRF_LOG_INFO("[TIMESLOT] S%d Telemetry: Impact1=%u us (pk:%u) | Elapsed=%u us | RSSI=%d dBm",
                 p_data->sensor_id,
                 p_data->impact1_time_us,
                 p_data->impact1_peak,
                 p_data->elapsed_prev_sync_us,
                 p_data->rssi);

    if (m_conn_handle != BLE_CONN_HANDLE_INVALID) {
        ble_impact_service_send_telemetry(&m_impact_service, 
                                          (const uint8_t *)p_data, 
                                          sizeof(sensor_telemetry_t));
    }
}

static void on_ble_ping_received(ble_impact_service_t * p_service, const uint8_t * p_data, uint16_t len) {
    bsp_board_led_invert(1);
    NRF_LOG_INFO("[BLE] Ping packet received from Android Phone (%d bytes)", len);
}

static void on_ble_config_received(ble_impact_service_t * p_service, const uint8_t * p_data, uint16_t len) {
    NRF_LOG_INFO("[BLE] Sensor config update received from Android Phone");
}

static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context) {
    ret_code_t err_code = NRF_SUCCESS;
    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("BLE Connected");
            bsp_board_led_on(0);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("BLE Disconnected");
            bsp_board_led_off(0);
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;
        case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
            ble_gap_phys_t const phys = {
                .tx_phys = BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS,
                .rx_phys = BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;
        default:
            break;
    }
}

static void ble_stack_init(void) {
    ret_code_t err_code;
    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
    NRF_SDH_BLE_OBSERVER(m_impact_observer, APP_BLE_OBSERVER_PRIO, ble_impact_service_on_ble_evt, &m_impact_service);
}

static void gap_params_init(void) {
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)DEVICE_NAME, strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);
    memset(&gap_conn_params, 0, sizeof(gap_conn_params));
    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;
    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

static void advertising_init(void) {
    ret_code_t             err_code;
    ble_advertising_init_t init;
    memset(&init, 0, sizeof(init));
    init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance      = true;
    init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.config.ble_adv_fast_enabled     = true;
    init.config.ble_adv_fast_interval    = 64;
    init.config.ble_adv_fast_timeout     = 0;
    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);
    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

int main(void) {
    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();
    APP_ERROR_CHECK(app_timer_init());
    bsp_board_init(BSP_INIT_LEDS);
    APP_ERROR_CHECK(nrf_pwr_mgmt_init());

    NRF_LOG_INFO("=============================================");
    NRF_LOG_INFO(" Impact Time Meter: Master Unit (nRF52840)");
    NRF_LOG_INFO(" Mode: BLE GATT + SoftDevice Timeslot RF");
    NRF_LOG_INFO("=============================================");

    ble_stack_init();
    gap_params_init();
    APP_ERROR_CHECK(nrf_ble_gatt_init(&m_gatt, NULL));
    APP_ERROR_CHECK(nrf_ble_qwr_init(&m_qwr, &(nrf_ble_qwr_init_t){.error_handler = NULL}));

    APP_ERROR_CHECK(ble_impact_service_init(&m_impact_service, 
                                           on_ble_ping_received, 
                                           on_ble_config_received));

    APP_ERROR_CHECK(timeslot_init(on_sensor_telemetry_received));
    NRF_LOG_INFO("Timeslot API initialized successfully!");

    advertising_init();
    APP_ERROR_CHECK(ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST));
    NRF_LOG_INFO("Advertising as '%s'...", DEVICE_NAME);

    for (;;) {
        NRF_LOG_FLUSH();
        nrf_pwr_mgmt_run();
    }
}
