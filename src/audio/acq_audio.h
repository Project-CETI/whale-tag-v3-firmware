/*****************************************************************************
 *   @file      acq_audio.h
 *   @brief     audio acquisition code. Note this code just gather audio data
 *              into RAM, but does not perform any analysis, transformation, or
 *              storage of said data.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_ACQ_AUDIO_H
#define CETI_ACQ_AUDIO_H
// libraries
#include <stddef.h>
#include <stdint.h>

#include "config.h"

// definitions
#define AUDIO_SAMPLE_SIZE_LSM (72)

typedef int (*AcqAudioLogCallback)(uint8_t *pData, uint32_t size);

// funcitons
void acq_audio_disable(void);
int acq_audio_init(const AudioConfig p_config[static 1]);
void acq_audio_start(uint8_t *p_buffer, uint16_t buffer_size_blocks, uint16_t block_size_bytes);
void acq_audio_stop(void);
void acq_audio_set_log_callback(AcqAudioLogCallback cb);
void acq_audio_register_block_complete_callback(void (*callback)(uint16_t));

#endif // CETI_ACQ_AUDIO_H
