#include <app_filex.h>
#include <main.h>

#include <stdint.h>


typedef uint16_t PerfEventId;

#define PERF_EVENT_TYPE_START (0)
#define PERF_EVENT_TYPE_END   (!0)
#define PERF_EVENT_QUEUE_LENGTH (512)
#define PERF_FILENAME "perf.bin"

typedef struct {
    uint32_t system_clock_us;
    uint16_t event_type;
    PerfEventId id;
} PerfEvent;

extern FX_MEDIA sdio_disk;
extern TIM_HandleTypeDef uS_htim;

static PerfEvent s_event_queue[PERF_EVENT_QUEUE_LENGTH];
static uint16_t s_event_queue_write_position = 0;
static uint16_t s_event_queue_read_position = 0;

void perf_start_event(PerfEventId id) {
    s_event_queue[s_event_queue_write_position].system_clock_us = uS_htim.Instance->CNT;
    s_event_queue[s_event_queue_write_position].event_type = PERF_EVENT_TYPE_START;
    s_event_queue[s_event_queue_write_position].id = id;
    s_event_queue_write_position =  (s_event_queue_write_position + 1) % PERF_EVENT_QUEUE_LENGTH;
}

void perf_end_event(PerfEventId id) {
    s_event_queue[s_event_queue_write_position].system_clock_us = uS_htim.Instance->CNT;
    s_event_queue[s_event_queue_write_position].event_type = PERF_EVENT_TYPE_START;
    s_event_queue[s_event_queue_write_position].id = id;
    s_event_queue_write_position =  (s_event_queue_write_position + 1) % PERF_EVENT_QUEUE_LENGTH;
}

void perf_init(void) {
    UINT fx_result = fx_file_create(&sdio_disk, PERF_FILENAME);
}

void perf_event_task(void) {
    // write perf report to SD card
    uint16_t w_half = s_event_queue_write_position / (PERF_EVENT_QUEUE_LENGTH / 2);
    uint16_t r_half = s_event_queue_read_position / (PERF_EVENT_QUEUE_LENGTH / 2);
    if (w_half == r_half) {
        return;
    }
    
    {  // write r_half to sd card
        FX_FILE perf_file = {};
        uint8_t * p_data = (uint8_t *)&s_event_queue[s_event_queue_read_position];
        uint16_t data_size = sizeof(s_event_queue) / 2;

        UINT fx_result = fx_file_open(&sdio_disk, &perf_file, PERF_FILENAME, FX_OPEN_FOR_WRITE);
        if (FX_SUCCESS == fx_result) { fx_result = fx_file_seek(&perf_file, -1); };
        // if (FX_SUCCESS == fx_result) { fx_result = fx_file_write_notify_set(&perf_file, audio_SDWriteComplete); };
        if (FX_SUCCESS == fx_result) { fx_result = fx_file_write(&perf_file, p_data, data_size); };
        fx_file_close(&perf_file);
    }

    // advance read position
    s_event_queue_read_position = (r_half ^ 1) * (PERF_EVENT_QUEUE_LENGTH / 2);
}