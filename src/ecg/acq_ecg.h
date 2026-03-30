/*****************************************************************************
 *   @file   acq/acq_ecg.h
 *   @brief  ecg acquisition code. Note this code just gathers ecg data
 *           into RAM, but does not perform any analysis, transformation, or
 *           storage of said data.
 *   @author Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#ifndef CETI_ACQ_ECG_H
#define CETI_ACQ_ECG_H

void acq_ecg_EXTI_Callback(void);
void acq_ecg_enable(void);
void acq_ecg_update(void);
void acq_ecg_init(void);
void acq_ecg_start(void);

#endif // CETI_ACQ_ECG_H
