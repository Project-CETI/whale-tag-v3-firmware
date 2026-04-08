#include <stdint.h>
#include "stm32u5xx.h"

#include <app_filex.h> // filex

#ifdef BENCHMARK
extern FX_MEDIA sdio_disk;
static FX_FILE fp = {};

typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    uint32_t func_addr;
    uint8_t event;
} ProfilingEvent;

#define PROFILING_BUFFER_LEN (6 * 1024)  // ~144 KB per buffer, 288 KB total

static ProfilingEvent s_buffers[2][PROFILING_BUFFER_LEN];
static volatile uint32_t s_trace_count = 0;
static volatile int s_active_buf = 0;
static volatile int s_profiling_enabled = 0;
static volatile uint32_t s_dropped_events = 0;

// Flush state (written by flush, read by hooks)
static volatile int s_flush_buf = -1;  // buffer index being flushed, -1 if idle
static uint32_t s_flush_count = 0;

__attribute__((no_instrument_function))
static inline uint32_t profile_get_cycle_count(void) {
    return DWT->CYCCNT;
}

__attribute__((no_instrument_function))
static inline void profile_record_event(void *func, uint8_t event) {
    if (!s_profiling_enabled) {
        return;
    }

    // Atomically claim a slot by disabling interrupts around the increment.
    // This prevents ISRs from racing on s_trace_count.
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint32_t idx = s_trace_count;
    if (idx < PROFILING_BUFFER_LEN) {
        s_trace_count = idx + 1;

        s_buffers[s_active_buf][idx] = (ProfilingEvent) {
            .timestamp = profile_get_cycle_count(),
            .func_addr = (uint32_t)func,
            .event = event,
        };
    } else {
        s_dropped_events++;
    }
    __set_PRIMASK(primask);
}

__attribute__((no_instrument_function))
void __cyg_profile_func_enter(void *func, void *caller) {
    profile_record_event(func, 0);
}

__attribute__((no_instrument_function))
void __cyg_profile_func_exit(void *func, void *caller) {
    profile_record_event(func, 1);
}

__attribute__((no_instrument_function))
void profile_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    UINT fx_create_result = fx_file_create(&sdio_disk, "profile.bin");
    if (fx_create_result != FX_SUCCESS) {
        return;
    }

    UINT fx_open_result = fx_file_open(&sdio_disk, &fp, "profile.bin", FX_OPEN_FOR_WRITE);
    if (FX_SUCCESS != fx_open_result) {
        return;
    }

    metadata_log_file_creation("profile.bin", DATA_TYPE_BENCHMARK, DATA_FORMAT_BIN, 0);

    s_profiling_enabled = 1;
}

__attribute__((no_instrument_function))
void profile_flush(void) {
    if (0 == s_trace_count) {
        return;
    }

    // Swap buffers atomically — hooks immediately start writing to the new buffer
    // while we flush the old one. No events are lost.
    __disable_irq();
    int buf_to_flush = s_active_buf;
    uint32_t count = s_trace_count;
    s_active_buf ^= 1;
    s_trace_count = 0;
    s_flush_buf = buf_to_flush;
    __enable_irq();

    // Write the completed buffer to SD (hooks are still running into the other buffer)
    fx_file_write(&fp, s_buffers[buf_to_flush], count * sizeof(ProfilingEvent));
    fx_media_flush(&sdio_disk);

    __disable_irq();
    s_flush_buf = -1;
    __enable_irq();
}

__attribute__((no_instrument_function))
void profile_pause(void) {
    s_profiling_enabled = 0;
}

__attribute__((no_instrument_function))
void profile_continue(void) {
    s_profiling_enabled = 1;
}

__attribute__((no_instrument_function))
uint32_t profile_get_dropped_count(void) {
    return s_dropped_events;
}
#endif
