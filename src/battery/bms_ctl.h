/*****************************************************************************
 *   @file      battery/bms_ctl.h
 *   @brief     High level runtime BMS control
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_BMS_CTL_H
#define CETI_BMS_CTL_H

#include "error.h"

int bms_ctl_verify(void);
int bms_ctl_program_nonvolatile_memory(void);
int bms_ctl_temporary_overwrite_nv_values(void);
int bms_ctl_get_cycles(uint16_t *p_cycles);
int bms_ctl_reset_FETs(void);
int bms_ctl_disable_FETs(void);
CetiStatus bms_disable_FETs(void);

#endif // CETI_BMS_CTL_H