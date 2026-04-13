/*****************************************************************************
 *   @file      util/buffer_writer.c
 *   @brief     wrapper around a file to buffer the output.
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef __CETI_BUFFER_WRITER_H__
#define __CETI_BUFFER_WRITER_H__

#include <app_filex.h>

typedef struct {
    uint8_t * buffer;
    size_t cursor;
    size_t threshold;
    size_t capacity;
    FX_FILE fp;
} BufferWriter;

UINT buffer_writer_open(BufferWriter *w, char * filename);
UINT buffer_writer_write(BufferWriter *w, uint8_t *p_bytes, size_t len);
UINT buffer_writer_close(BufferWriter *w);
UINT buffer_writer_flush(BufferWriter *w);

#endif // __CETI_BUFFER_WRITER_H__
