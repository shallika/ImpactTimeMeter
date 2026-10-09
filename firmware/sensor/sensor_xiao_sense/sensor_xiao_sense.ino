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

// Optional Serial debugging (0 = disabled for bare-metal RF timing & clean zero-dependency linking)
#define ENABLE_SERIAL_DEBUG       0

#if ENABLE_SERIAL_DEBUG
  #if defined(USE_TINYUSB)
    #include <Adafruit_TinyUSB.h>
  #endif
#endif

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

static uint8_t rx_packet[32] __attribute__((aligned(4)));
static uint8_t tx_packet[20] __attribute__((aligned(4)));
static volatile uint32_t last_sync_time_us = 0;
static uint32_t green_led_off_time = 0;
static uint32_t red_led_off_time = 0;

void setup_radio_receiver() {
    NRF_RADIO->POWER = 0;
    delay(2);
    NRF_RADIO->POWER = 1;
    NRF_RADIO->MODE = 1; // 2 Mbps
    NRF_RADIO->FREQUENCY = 50; // 2450 MHz
    NRF_RADIO->TXPOWER = 0x04; // +4 dBm
    NRF_RADIO->PCNF0 = 0; // LFLEN = 0 (no dynamic length byte in packet header)
    NRF_RADIO->PCNF1 = (32 << 0) | (12 << 8) | (3 << 16) | (0 << 24); // MAXLEN = 32, STATLEN = 12 (Beacon len), BALEN = 3, ENDIAN = Little
    NRF_RADIO->BASE0 = 0xE7E7E700;
    NRF_RADIO->PREFIX0 = 0xE7;
    NRF_RADIO->TXADDRESS = 0;
    NRF_RADIO->RXADDRESSES = 1;
    NRF_RADIO->CRCCNF = 2; // 2 byte CRC, Include Address in CRC calculation
    NRF_RADIO->CRCPOLY = 0x11021;
    NRF_RADIO->CRCINIT = 0xFFFF;
    NRF_RADIO->SHORTS = (1 << 0) | (1 << 1); // READY_START | END_DISABLE
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

    // 1. Ensure radio is completely Disabled first
    NRF_RADIO->TASKS_DISABLE = 1;
    uint32_t start_us = micros();
    while (NRF_RADIO->STATE != 0 && (micros() - start_us) < 1000);

    // 2. Clear flags and set TX packet parameters
    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->EVENTS_END = 0;
    NRF_RADIO->EVENTS_DISABLED = 0;

    NRF_RADIO->PCNF1 = (20 << 0) | (20 << 8) | (3 << 16) | (0 << 24); // MAXLEN = 20, STATLEN = 20 for 20-byte reply
    NRF_RADIO->PACKETPTR = (uint32_t)tx_packet;

    // 3. Start TX
    NRF_RADIO->TASKS_TXEN = 1;

    // 4. Wait for packet transmission to complete on air (EVENTS_END)
    start_us = micros();
    while (!NRF_RADIO->EVENTS_END && (micros() - start_us) < 2000);

    // 5. Turn off TX radio cleanly
    NRF_RADIO->TASKS_DISABLE = 1;
    start_us = micros();
    while (NRF_RADIO->STATE != 0 && (micros() - start_us) < 1000);

    // 6. Restore RX configuration for Beacon receiving
    NRF_RADIO->EVENTS_READY = 0;
    NRF_RADIO->EVENTS_END = 0;
    NRF_RADIO->EVENTS_DISABLED = 0;
    NRF_RADIO->PCNF1 = (32 << 0) | (12 << 8) | (3 << 16) | (0 << 24); // Restore MAXLEN = 32, STATLEN = 12
    NRF_RADIO->PACKETPTR = (uint32_t)rx_packet;
    NRF_RADIO->TASKS_RXEN = 1;
}

void set_green_led(bool state_on) {
    digitalWrite(SENSOR_LED_GREEN, state_on ? LOW : HIGH);
}

void set_red_led(bool state_on) {
    digitalWrite(SENSOR_LED_RED, state_on ? LOW : HIGH);
}

void setup() {
#if ENABLE_SERIAL_DEBUG
    Serial.begin(115200);
#endif
    #if defined(SOFTDEVICE_PRESENT)
    uint8_t sd_enabled = 0;
    if (sd_softdevice_is_enabled(&sd_enabled) == 0 && sd_enabled) {
        sd_softdevice_disable();
    }
    #endif

    // Ensure 32 MHz External Crystal Oscillator (HFXO) is running for high-precision RF
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0);

    pinMode(SENSOR_LED_GREEN, OUTPUT);
    pinMode(SENSOR_LED_RED, OUTPUT);
    set_green_led(false);
    set_red_led(false);

    // 3 quick power-on test blinks
    for (int i = 0; i < 3; i++) {
        set_green_led(true);
        delay(100);
        set_green_led(false);
        delay(100);
    }

    setup_timer_and_ppi();
    setup_radio_receiver();
}

static inline uint32_t read_timer1_us() {
    NRF_TIMER1->TASKS_CAPTURE[1] = 1;
    return NRF_TIMER1->CC[1];
}

void loop() {
    if (NRF_RADIO->EVENTS_CRCOK) {
        NRF_RADIO->EVENTS_CRCOK = 0;
        NRF_RADIO->EVENTS_END = 0;
        NRF_RADIO->EVENTS_ADDRESS = 0;
        NRF_RADIO->EVENTS_DISABLED = 0;
        if (rx_packet[0] == SYNC_MAGIC_BYTE) {
            set_green_led(true);
            green_led_off_time = millis() + 100;
            uint32_t now_us = read_timer1_us();
            uint32_t elapsed_us = (last_sync_time_us > 0) ? (now_us - last_sync_time_us) : 1000000;
            last_sync_time_us = now_us;
            int8_t rssi = -1 * (int8_t)NRF_RADIO->RSSISAMPLE;
            
            // TDMA slot delay: wait 800 us after beacon end before sending reply
            delayMicroseconds(800 * SENSOR_ID);
            send_tdma_reply(rssi, elapsed_us);
        } else {
            NRF_RADIO->PACKETPTR = (uint32_t)rx_packet;
            NRF_RADIO->TASKS_RXEN = 1;
        }
    } else if (NRF_RADIO->EVENTS_CRCERROR) {
        NRF_RADIO->EVENTS_CRCERROR = 0;
        NRF_RADIO->EVENTS_END = 0;
        NRF_RADIO->EVENTS_ADDRESS = 0;
        NRF_RADIO->EVENTS_DISABLED = 0;
        set_red_led(true);
        red_led_off_time = millis() + 80;
        NRF_RADIO->PACKETPTR = (uint32_t)rx_packet;
        NRF_RADIO->TASKS_RXEN = 1;
    } else if (NRF_RADIO->EVENTS_END) {
        NRF_RADIO->EVENTS_END = 0;
        NRF_RADIO->EVENTS_ADDRESS = 0;
        NRF_RADIO->EVENTS_DISABLED = 0;
        NRF_RADIO->PACKETPTR = (uint32_t)rx_packet;
        NRF_RADIO->TASKS_RXEN = 1;
    } else if (NRF_RADIO->STATE == 0) { // Radio Disabled state
        NRF_RADIO->EVENTS_DISABLED = 0;
        NRF_RADIO->PACKETPTR = (uint32_t)rx_packet;
        NRF_RADIO->TASKS_RXEN = 1;
    }

    if (green_led_off_time > 0 && millis() >= green_led_off_time) {
        set_green_led(false);
        green_led_off_time = 0;
    }
    if (red_led_off_time > 0 && millis() >= red_led_off_time) {
        set_red_led(false);
        red_led_off_time = 0;
    }
}
