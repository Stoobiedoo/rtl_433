/** @file
    Fuzz harness driving every decoder with arbitrary bitbuffers.

    Copyright (C) 2026 rtl_433 contributors

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Build with:

        cmake -B build -DENABLE_FUZZING=ON -DCMAKE_C_COMPILER=clang
        cmake --build build
        ./build/tests/fuzz-decoder -max_len=600 corpus/

    Decoders parse data straight off the air, so every bitbuffer they see is
    attacker influenced. This feeds them arbitrary ones under ASan/UBSan.

    The input is: two bytes selecting the decoder, then a sequence of
    length-prefixed rows filling the bitbuffer.
*/

#include "bitbuffer.h"
#include "data.h"
#include "r_device.h"
#include "rtl_433_devices.h"

#include <stdint.h>
#include <string.h>

static r_device const *const fuzz_devices[] = {
#define DECL(name) &name,
        DEVICES
#undef DECL
};

#define FUZZ_NUM_DEVICES (sizeof(fuzz_devices) / sizeof(fuzz_devices[0]))

static void fuzz_output(r_device *decoder, data_t *data)
{
    (void)decoder;
    data_free(data);
}

static void fuzz_log(r_device *decoder, int level, data_t *data)
{
    (void)decoder;
    (void)level;
    data_free(data);
}

int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size);

int LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    if (size < 3) {
        return 0;
    }

    unsigned sel = ((unsigned)data[0] << 8 | data[1]) % FUZZ_NUM_DEVICES;
    data += 2;
    size -= 2;

    bitbuffer_t bits = {0};

    size_t pos = 0;
    while (pos < size && bits.num_rows < BITBUF_ROWS) {
        unsigned nbytes = data[pos++];
        if (nbytes > BITBUF_COLS) {
            nbytes = BITBUF_COLS;
        }
        if (pos + nbytes > size) {
            nbytes = (unsigned)(size - pos);
        }
        if (nbytes == 0) {
            break;
        }
        unsigned row = bits.num_rows++;
        memcpy(bits.bb[row], data + pos, nbytes);
        bits.bits_per_row[row] = (uint16_t)(nbytes * 8);
        pos += nbytes;
    }
    if (bits.num_rows == 0) {
        return 0;
    }
    bits.free_row = bits.num_rows;

    r_device dev = *fuzz_devices[sel];
    if (dev.create_fn || !dev.decode_fn) {
        return 0; // needs args, e.g. the flex decoder
    }
    dev.output_fn   = fuzz_output;
    dev.log_fn      = fuzz_log;
    dev.verbose     = 0;
    dev.verbose_bits = 0;
    dev.decode_ctx  = NULL;
    dev.output_ctx  = NULL;

    dev.decode_fn(&dev, &bits);

    return 0;
}
