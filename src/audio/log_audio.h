/*****************************************************************************
 *   @file      audio/log_audio.h
 *   @brief     Audio sample processing and logging code
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#ifndef CETI_LOG_AUDIO_H
#define CETI_LOG_AUDIO_H

#include <stdint.h>

#include "config.h"
#include "acq_audio.h"

#define AUDIO_LOG_BUFFER_SIZE_BLOCKS 32
#define AUDIO_LOG_BLOCK_SIZE_MAX (UINT16_MAX)
#define AUDIO_LOG_BLOCK_SIZE ((AUDIO_LOG_BLOCK_SIZE_MAX) - ((AUDIO_LOG_BLOCK_SIZE_MAX) % AUDIO_SAMPLE_SIZE_LSM))

void log_audio_init(const AudioConfig *settings);
void log_audio_disable(void);
void log_audio_block_complete_callback(uint16_t block_index);
void log_audio_task(void);

extern uint8_t *log_audio_buffer;

#endif // CETI_LOG_AUDIO_H
