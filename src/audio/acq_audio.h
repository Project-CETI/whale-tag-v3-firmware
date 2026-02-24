/*****************************************************************************
 *   @file      acq_audio.h
 *   @brief     audio acquisition code. Note this code just gather audio data 
 *              into RAM, but does not perform any analysis, transformation, or
 *              storage of said data.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#ifndef CETI_ACQ_AUDIO_H
#define CETI_ACQ_AUDIO_H
// libraries
#include <stddef.h>
#include <stdint.h>

// definitions
#define AUDIO_SAMPLE_SIZE_LSM (72)
#define AUDIO_BLOCK_SIZE_MAX (UINT16_MAX)
#define AUDIO_BLOCK_SIZE  ((AUDIO_BLOCK_SIZE_MAX) - ((AUDIO_BLOCK_SIZE_MAX) % AUDIO_SAMPLE_SIZE_LSM))

#define AUDIO_BUFFER_SIZE_BLOCKS 32
#define AUDIO_BUFFER_SIZE_BYTES (AUDIO_BUFFER_SIZE_BLOCKS * AUDIO_BLOCK_SIZE)

typedef int (* AcqAudioLogCallback)(uint8_t *pData, uint32_t size);

// funcitons
void acq_audio_disable(void);
int acq_audio_init(void);
void acq_audio_start(void);
void acq_audio_stop(void);
void acq_audio_set_log_callback(AcqAudioLogCallback cb);
void acq_audio_task(void);

#endif // CETI_ACQ_AUDIO_H
