/**
 * @file sensor_xiao_sense.ino
 * @brief Sensor Node Firmware for Seeed Studio XIAO nRF52840 Sense
 * Compatible with Seeed nRF52 mbed core and Adafruit nRF52 core.
 */
#include <Arduino.h>

#if defined(__has_include)
  #if __has_include(<nrf.h>)
    #include <nrf.h>
  #endif
  #if __has_include(<nrf_sdm.h>)
    #include <nrf_sdm.h>
  #endif
#endif

#ifndef NRF_RADIO
  #define NRF_RADIO ((NRF_RADIO_Type *) 0x40001000UL)
#endif
#ifndef NRF_TIMER1
  #define NRF_TIMER1 ((NRF_TIMER_Type *) 0x40009000UL)
#endif
#ifndef NRF_PPI
  #define NRF_PPI ((NRF_PPI_Type *) 0x4001F000UL)
#endif

#define SENSOR_ID                 1
#define SYNC_MAGIC_BYTE           0x59

#if defined(PIN_LED_GREEN)
  #define SENSOR_LED_GREEN PIN_LED_GREEN
#elif defined(LED_GREEN)
  #define SENSOR_LED_GREEN LED_GREEN
#else
  #define SENSOR_LED_GREEN 13
#endif

#if defined(PIN_LED_RED)
  #define SENSOR_LED_RED PIN_LED_RED
#elif defined(LED_RED)
  #define SENSOR_LED_RED LED_RED
#else
  #define SENSOR_LED_RED 11
#endif

static uint8_t rx_packet[32];
static uint8_t tx_packet[20];
static volatile uint32_t last_sync_time_us = 0;
static uint32_t green_led_off_time = 0;

void setup_radio_receiver() {
    NRF_RADIO->POWER = 0;
    delay(2);
    NRF_RADIO->POWER = 1;
    NRF_RADIO->MODE = 1; // 2 Mbps
    NRF_RADIO->FREQUENCY = 50; // 2450 MHz
    NRF_RADIO->TXPOWER = 0x04; // +4 dBm
    NRF_RADIO->PCNF0 = (8 << 0);
    NRF_RADIO->PCNF1 = (32 << 0) | (3 << 16) | (0 << 24);
    NRF_RADIO->BASE0 = 0xE7E7E700;
    NRF_RADIO->PREFIX0 = 0xE7;
    NRF_RADIO->TXADDRESS = 0;
    NRF_RADIO->RXADDRESSES = 1;
    NRF_RADIO->CRCCNF = 2;
    NRF_RADIO->CRCPOLY = 0x11021;
    NRF_RADIO->CRCINIT = 0xFFFF;
    NRF_RADIO->SHORTS = (1 << 0) | (1 << 1);
    NRF_RADIO->PACKETPTR = (uint32_t)rx_packet;
    NRF_RADIO->TASKS_RXEN = 1;
}

void setup_timer_and_ppi() {
    NRF_TIMER1->TASKS_STOP = 1;
    NRF_TIMER1->MODE = 0;
    NRF_TIMER1->BITMODE = 3;
    NRF_TIMER1->PRESCALER = 4; // 1 MHz (1 us per tick)
    NRF_TIMER1->TASKS_CLEAR = 1;
    NRF_TIMER1->TASKS_START = 1;

    NRF_PPI->CH[0].EEP = (uint32_t)&NRF_RADIO->EVENTS_ADDRESS;
    NRF_PPI->CH[0].TEP = (uint32_t)&NRF_TIMER1->TASKS_CLEAR;
    NRF_PPI->CHENSET = (1 << 0);
}

void send_tdma_reply(int8_t rssi, uint32_t elapsed_us) {
    tx_packet[0] = SENSOR_ID;
    uint32_t imp1_time = 1250;
    uint16_t imp1_peak = 1840;
    memcpy(&tx_packet[1], &imp1_time, 4);
    memcpy(&tx_packet[5], &imp1_peak, 2);
    memcpy(&tx_packet[13], &elapsed_us, 4);
    tx_packet[17] = 22;
    tx_packet[18] = (uint8_t)rssi;
    tx_packet[19] = 0x01;

    NRF_RADIO->PACKETPTR = (uint32_t)tx_packet;
    NRF_RADIO->TASKS_TXEN = 1;
    while (!NRF_RADIO->EVENTS_DISABLED);
    NRF_RADIO->EVENTS_DISABLED = 0;
    NRF_RADIO->PACKETPTR = (uint32_t)rx_packet;
    NRF_RADIO->TASKS_RXEN = 1;
}

void setup() {
    Serial.begin(115200);
    #if defined(SOFTDEVICE_PRESENT)
      sd_softdevice_disable();
    #endif
    pinMode(SENSOR_LED_GREEN, OUTPUT);
    pinMode(SENSOR_LED_RED, OUTPUT);
    digitalWrite(SENSOR_LED_GREEN, HIGH);
    digitalWrite(SENSOR_LED_RED, HIGH);
    setup_timer_and_ppi();
    setup_radio_receiver();
}

void loop() {
    if (NRF_RADIO->EVENTS_CRCOK) {
        NRF_RADIO->EVENTS_CRCOK = 0;
        if (rx_packet[0] == SYNC_MAGIC_BYTE) {
            digitalWrite(SENSOR_LED_GREEN, LOW);
            green_led_off_time = millis() + 100;
            uint32_t now_us = NRF_TIMER1->COUNTER;
            uint32_t elapsed_us = (last_sync_time_us > 0) ? (now_us - last_sync_time_us) : 1000000;
            last_sync_time_us = now_us;
            int8_t rssi = -1 * (int8_t)NRF_RADIO->RSSISAMPLE;
            while (NRF_TIMER1->COUNTER < SENSOR_ID * 1000);
            send_tdma_reply(rssi, elapsed_us);
        } else {
            NRF_RADIO->TASKS_RXEN = 1;
        }
    }
    if (green_led_off_time > 0 && millis() >= green_led_off_time) {
        digitalWrite(SENSOR_LED_GREEN, HIGH);
        green_led_off_time = 0;
    }
}
