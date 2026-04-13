/*****************************************************************************
 *   @file   acq/acq_ecg.h
 *   @brief  ecg acquisition code. Note this code just gathers ecg data
 *           into RAM, but does not perform any analysis, transformation, or
 *           storage of said data.
 *   @author Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_ACQ_ECG_H
#define CETI_ACQ_ECG_H

#include <stdint.h>

typedef struct {
    uint64_t timestamp_us;
    int32_t value;
    uint8_t lod_p;
    uint8_t lod_n;
} EcgSample;

void acq_ecg_EXTI_Callback(void);
void acq_ecg_init(void);
void acq_ecg_deinit(void);
void acq_ecg_start(void);
void acq_ecg_stop(void);
void acq_ecg_register_sample_callback(void(*callback)(const EcgSample *p_sample));

#endif // CETI_ACQ_ECG_H
