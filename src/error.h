//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//----------------------------------------------------------------------------
#ifndef CETI_ERROR_H
#define CETI_ERROR_H

#include <stdint.h>

typedef uint32_t CetiStatus;

#define CETI_STATUS_OK 0
#define CETI_ERR(subsystem, error) (((uint32_t)(subsystem) & 0xFFFF) << 16) | ( ((uint32_t)(error) & 0xFFFF))

#endif // CETI_ERROR_H