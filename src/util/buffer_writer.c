#include "buffer_writer.h"

extern FX_MEDIA sdio_disk;

/********* Private ***********************************************************/
__attribute__((no_instrument_function)) static UINT priv__flush(BufferWriter *w) {
    if (0 == w->cursor) {
        return FX_SUCCESS;
    }

    UINT fx_result = fx_file_write(&w->fp, w->buffer, w->cursor);
    if (FX_SUCCESS != fx_result) {
        return fx_result;
    }
    w->cursor = 0;
    return FX_SUCCESS;
}

/********* PUblic ************************************************************/
__attribute__((no_instrument_function))
UINT
buffer_writer_open(BufferWriter *w, char *filename) {
    return fx_file_open(&sdio_disk, &w->fp, filename, FX_OPEN_FOR_WRITE);
}

__attribute__((no_instrument_function))
UINT
buffer_writer_write(BufferWriter *w, uint8_t *p_bytes, size_t len) {
    UINT w_result = FX_SUCCESS;
    size_t remaining_capacity = w->capacity - w->cursor;
    while (len > remaining_capacity) {
        if (0 == w->cursor) {
            // no reason to move into own smaller buffer, just directly write
            return fx_file_write(&w->fp, p_bytes, len);
        }

        /* fill buffer and flush */
        memcpy(&w->buffer[w->cursor], p_bytes, remaining_capacity);
        w->cursor = w->capacity;
        w_result = priv__flush(w);
        if (FX_SUCCESS != w_result) {
            return w_result;
        }
        p_bytes += remaining_capacity;
        len -= remaining_capacity;
        remaining_capacity = w->capacity;

        if (len == 0) {
            return FX_SUCCESS;
        }
    }

    /* store data into buffer */
    memcpy(&w->buffer[w->cursor], p_bytes, len);
    w->cursor += len;

    /* write out data if we have enough */
    if (w->cursor >= w->threshold) {
        return priv__flush(w);
    }
    return FX_SUCCESS;
}

__attribute__((no_instrument_function))
UINT
buffer_writer_close(BufferWriter *w) {
    UINT fx_result = priv__flush(w);
    if (FX_SUCCESS != fx_result) {
        return fx_result;
    }
    return fx_file_close(&w->fp);
}

/// @brief flushes any buffered data to SD card
/// @param w pointer to buffer writer
/// @return
__attribute__((no_instrument_function))
UINT
buffer_writer_flush(BufferWriter *w) {
    /* write any data in internal buffer */
    UINT fx_result = priv__flush(w);
    if (FX_SUCCESS != fx_result) {
        return fx_result;
    }

    /* ensure SD card write flushes */
    return fx_media_flush(&sdio_disk);
}
