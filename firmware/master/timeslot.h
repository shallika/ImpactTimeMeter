#ifndef TIMESLOT_H__
#define TIMESLOT_H__
#include <stdint.h>
#include <stdbool.h>
#include "nrf_soc.h"

#define TIMESLOT_LENGTH_US        8000
#define TIMESLOT_INTERVAL_MS      1000

typedef struct {
    uint8_t  sensor_id;
    uint32_t impact1_time_us;
    uint16_t impact1_peak;
    uint32_t impact2_time_us;
    uint16_t impact2_peak;
    uint32_t elapsed_prev_sync_us;
    uint8_t  noise_floor;
    int8_t   rssi;
    uint8_t  status;
} __attribute__((packed)) sensor_telemetry_t;

typedef void (*timeslot_data_handler_t)(const sensor_telemetry_t *p_data);

uint32_t timeslot_init(timeslot_data_handler_t data_handler);
uint32_t timeslot_request_next(void);
void timeslot_trigger_immediate_sync(void);
void timeslot_on_soc_evt(uint32_t evt_id);
#endif
