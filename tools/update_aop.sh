#! /bin/bash

HEADER_PATH=src/satellite

# Make request
AOP_TABLE=$(curl -s -X 'POST' \
  'https://environmental-monitoring.groupcls.com/telemetry/api/v1/retrieve-kineis-aop?format=text' \
  -H 'accept: application/octet-stream' \
  -d '')

# Format text result
FORMATTED_AOP_TABLE=$(echo "$AOP_TABLE" | sed 's/\t/, /g;'\
's/\([0-9]*\.[0-9]*\)/\1f/g;'\
's/[0]*\([0-9],\)/\1/g;'\
's/[^,]*, \(\([^,]*, \)\{4\}\)/0x\1{/;'\
's/\({\([^,]*, \)\{5\}[^,]*\)/\1}/;'\
's/\(.*\)\r/    \{\1\},/'\
 )

# Generate Header
cat << EOF > $HEADER_PATH/aop.h
/*****************************************************************************
 *   @file      aop.h
 *   @brief     This is a generated header that grabs the latest aop file from
 *              CLS's API
 *   @project   Project CETI
 *   @date      $(date)
 *   @authors   Michael Salino-Hugg, [TODO: Add other contributors here]
 *****************************************************************************/
#ifndef CETI_AOP_H
#define CETI_AOP_H
#include "previpass.h"
#include <stdint.h>

__attribute__((section(".tag_aop_flash")))
const struct {
  uint64_t timestamp_s;
  uint16_t table_count;
  uint16_t reserve_0A;
  uint16_t reserve_0C;
  uint16_t reserve_0E;
  struct AopSatelliteEntry_t aopTable[(8*1024 - 2*sizeof(uint64_t))/sizeof(struct AopSatelliteEntry_t)];
} nv_aop_data =  {
  .timestamp_s = $(date +%s),
  .table_count = $(echo "$FORMATTED_AOP_TABLE" | wc -l ),
  .aopTable = {
$(echo "$FORMATTED_AOP_TABLE")
  },
};
#endif // CETI_AOP_H
EOF