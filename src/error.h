//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//----------------------------------------------------------------------------
#ifndef CETI_ERROR_H
#define CETI_ERROR_H

#include <stdint.h>

typedef uint32_t CetiStatus;

typedef enum {
    ERR_SUBSYS_NONE,
    ERR_SUBSYS_ERROR_QUEUE,
    ERR_SUBSYS_LOG_MISSION,
    ERR_SUBSYS_SYSLOG,
    ERR_SUBSYS_MISSION,
    ERR_SUBSYS_AUDIO,
    ERR_SUBSYS_LOG_AUDIO,
    ERR_SUBSYS_GPS,
    ERR_SUBSYS_LOG_GPS,
    ERR_SUBSYS_ARGOS,
    ERR_SUBSYS_BMS,
    ERR_SUBSYS_LOG_BMS,
    ERR_SUBSYS_PRESSURE,
    ERR_SUBSYS_LOG_PRESSURE,
    ERR_SUBSYS_IMU,
    ERR_SUBSYS_LOG_IMU,
    ERR_SUBSYS_ECG,
    ERR_SUBSYS_LOG_ECG,
    ERR_SUBSYS_LED,
} ErrorSubsystem;

typedef enum {
    ERR_TYPE_DEFAULT,
    ERR_TYPE_FILEX,
} ErrorType;

typedef enum {
    ERR_NONE,
    ERR_BUFFER_OVERFLOW,
    ERR_OUTDATED_AOP_TABLE,
} DefaultErrorCode;

#define CETI_STATUS_OK 0
#define CETI_ERROR(subsystem, error_type, error_code) ((((CetiStatus)(subsystem) & 0xFF) << 24) | (((CetiStatus)(error_type) & 0xFF) << 16) | (((CetiStatus)(error_code) & 0xFFFF)))
#define CETI_ERR_SUBSYSTEM(status) ((status >> 24) & 0xFF)
#define CETI_ERR_CODE_TYPE(status) ((status >> 16) & 0xFF)
#define CETI_ERR_CODE(status) ((status) & 0xFFFF)

int error_queue_init(void);
void error_queue_push(CetiStatus error);
void error_queue_flush(void);
void error_queue_task(void);
void error_queue_close(void);

#endif // CETI_ERROR_H
