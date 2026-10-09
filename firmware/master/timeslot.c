/**
 * @file timeslot.c
 * @brief Nordic SoftDevice Timeslot API multiplexer for Impact Time Meter
 */
#include "timeslot.h"
#include "nrf.h"
#include "nrf_soc.h"
#include "nrf_log.h"
#include "app_timer.h"
#include <string.h>

#define SYNC_MAGIC_BYTE           0x59
#define BEACON_PACKET_LEN         12
#define SENSOR_REPLY_LEN          20

static timeslot_data_handler_t   m_data_handler = NULL;
static uint16_t                  m_epoch_seq = 0;
static uint8_t                   m_tx_buf[BEACON_PACKET_LEN];
static uint8_t                   m_rx_buf[SENSOR_REPLY_LEN + 4];
static volatile bool             m_in_timeslot = false;

APP_TIMER_DEF(m_timeslot_timer_id);

static nrf_radio_signal_callback_return_param_t * radio_callback(uint8_t signal_type);

static nrf_radio_request_t m_timeslot_req_earliest = {
    .request_type = NRF_RADIO_REQ_TYPE_EARLIEST,
    .params.earliest = {
        .hfclk       = NRF_RADIO_HFCLK_CFG_XTAL_GUARANTEED,
        .priority    = NRF_RADIO_PRIORITY_HIGH,
        .length_us   = TIMESLOT_LENGTH_US,
        .timeout_us  = 1000000
    }
};

static nrf_radio_signal_callback_return_param_t m_rsc_return_param;

static void configure_radio_proprietary(void) {
    NRF_RADIO->POWER = 1;
    NRF_RADIO->MODE = (RADIO_MODE_MODE_Nrf_2Mbit << RADIO_MODE_MODE_Pos);
    NRF_RADIO->FREQUENCY = 50;
    NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_Pos4dBm << RADIO_TXPOWER_TXPOWER_Pos);
    NRF_RADIO->PCNF0 = (0 << RADIO_PCNF0_S0LEN_Pos) | (0 << RADIO_PCNF0_LFLEN_Pos) | (0 << RADIO_PCNF0_S1LEN_Pos);
    NRF_RADIO->PCNF1 = (BEACON_PACKET_LEN << RADIO_PCNF1_MAXLEN_Pos) | (BEACON_PACKET_LEN << RADIO_PCNF1_STATLEN_Pos) | (3 << RADIO_PCNF1_BALEN_Pos) | (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) | (RADIO_PCNF1_WHITEEN_Disabled << RADIO_PCNF1_WHITEEN_Pos);
    NRF_RADIO->BASE0 = 0xE7E7E700;
    NRF_RADIO->PREFIX0 = 0xE7;
    NRF_RADIO->TXADDRESS = 0;
    NRF_RADIO->RXADDRESSES = 1;
    NRF_RADIO->CRCCNF = (RADIO_CRCCNF_LEN_Two << RADIO_CRCCNF_LEN_Pos) | (RADIO_CRCCNF_SKIPADDR_Include << RADIO_CRCCNF_SKIPADDR_Pos);
    NRF_RADIO->CRCPOLY = 0x11021;
    NRF_RADIO->CRCINIT = 0xFFFF;
    NRF_RADIO->SHORTS = RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_END_DISABLE_Msk;
}

static void prepare_sync_packet(void) {
    m_tx_buf[0] = SYNC_MAGIC_BYTE;
    m_tx_buf[1] = (uint8_t)(m_epoch_seq & 0xFF);
    m_tx_buf[2] = (uint8_t)((m_epoch_seq >> 8) & 0xFF);
    m_tx_buf[3] = 0x00;
    NRF_TIMER0->TASKS_CAPTURE[1] = 1;
    uint32_t current_time_us = NRF_TIMER0->CC[1];
    memcpy(&m_tx_buf[4], &current_time_us, sizeof(uint32_t));
    m_tx_buf[8] = 0xAA;
    m_tx_buf[9] = 0x55;
    m_tx_buf[10] = 0xAA;
    m_tx_buf[11] = 0x55;
    m_epoch_seq++;
}

static nrf_radio_signal_callback_return_param_t * radio_callback(uint8_t signal_type) {
    switch (signal_type) {
        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_START:
            m_in_timeslot = true;
            configure_radio_proprietary();
            NRF_TIMER0->TASKS_STOP = 1;
            NRF_TIMER0->TASKS_CLEAR = 1;
            NRF_TIMER0->PRESCALER = 4;
            NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
            NRF_TIMER0->EVENTS_COMPARE[0] = 0;
            NRF_TIMER0->CC[0] = (TIMESLOT_LENGTH_US - 500);
            NRF_TIMER0->INTENSET = TIMER_INTENSET_COMPARE0_Msk;
            NRF_TIMER0->TASKS_START = 1;
            prepare_sync_packet();
            NRF_RADIO->PACKETPTR = (uint32_t)m_tx_buf;
            NRF_RADIO->TASKS_TXEN = 1;
            m_rsc_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE;
            break;

        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_RADIO:
            if (NRF_RADIO->EVENTS_DISABLED) {
                NRF_RADIO->EVENTS_DISABLED = 0;
                if (NRF_RADIO->STATE == RADIO_STATE_STATE_Disabled) {
                    NRF_RADIO->PCNF1 = (SENSOR_REPLY_LEN << RADIO_PCNF1_MAXLEN_Pos) | (SENSOR_REPLY_LEN << RADIO_PCNF1_STATLEN_Pos) | (3 << RADIO_PCNF1_BALEN_Pos) | (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) | (RADIO_PCNF1_WHITEEN_Disabled << RADIO_PCNF1_WHITEEN_Pos);
                    NRF_RADIO->PACKETPTR = (uint32_t)m_rx_buf;
                    NRF_RADIO->TASKS_RXEN = 1;
                }
            }
            if (NRF_RADIO->EVENTS_CRCOK) {
                NRF_RADIO->EVENTS_CRCOK = 0;
                if (m_rx_buf[0] >= 1 && m_rx_buf[0] <= 4 && m_data_handler != NULL) {
                    sensor_telemetry_t telemetry;
                    memcpy(&telemetry, m_rx_buf, sizeof(sensor_telemetry_t));
                    telemetry.rssi = (int8_t)(-1 * (int8_t)NRF_RADIO->RSSISAMPLE);
                    m_data_handler(&telemetry);
                }
                NRF_RADIO->TASKS_RXEN = 1;
            }
            m_rsc_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE;
            break;

        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_TIMER0:
            NRF_TIMER0->EVENTS_COMPARE[0] = 0;
            NRF_RADIO->TASKS_DISABLE = 1;
            m_in_timeslot = false;
            m_rsc_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_END;
            break;

        default:
            m_rsc_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE;
            break;
    }
    return &m_rsc_return_param;
}

static void timeslot_timer_timeout_handler(void * p_context) {
    sd_radio_request(&m_timeslot_req_earliest);
}

uint32_t timeslot_init(timeslot_data_handler_t data_handler) {
    m_data_handler = data_handler;
    uint32_t err_code = sd_radio_session_open(radio_callback);
    if (err_code != NRF_SUCCESS) return err_code;
    err_code = app_timer_create(&m_timeslot_timer_id, APP_TIMER_MODE_REPEATED, timeslot_timer_timeout_handler);
    if (err_code != NRF_SUCCESS) return err_code;
    return app_timer_start(m_timeslot_timer_id, APP_TIMER_TICKS(TIMESLOT_INTERVAL_MS), NULL);
}

void timeslot_trigger_immediate_sync(void) {
    sd_radio_request(&m_timeslot_req_earliest);
}
