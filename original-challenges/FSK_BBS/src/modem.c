/*
 * Copyright (c) 2014 Kaprica Security, Inc.
 *
 * Permission is hereby granted, cgc_free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
/*
 * Implements a simple FSK modem with a carrier of 1900 HZ. Sends and receives
 * 8-bit u-law samples over stdin and stdout.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ecc.h"
#include "buffer.h"
#include "modem.h"
#include "modem_data.h"
#include "frame.h"
#include "sin.h"
#include "comp.h"

#define SAMPLE_RATE 8000
#define SAMPLES_PER_ITER 80 // process samples every 0.01 ms
#define SAMPLES_PER_BIT 20
#define CONVOLUTION_DELAY 9
#define FREQ_1700 ((cgc_int16_t)(1700.0 / 8000.0 * 65536.0))
#define FREQ_2100 ((cgc_int16_t)(2100.0 / 8000.0 * 65536.0))

static cgc_buffer_t g_frames;
static cgc_uint8_t g_frames_data[8192];

// history buffer must support `SAMPLES_PER_ITER + ncoeffs` samples
static void cgc_filter(cgc_int16_t *samples, cgc_int16_t *coeffs, size_t ncoeffs, cgc_int16_t *history)
{
    size_t i, j;
    cgc_memcpy(&history[ncoeffs - 1], samples, SAMPLES_PER_ITER * sizeof(cgc_int16_t));
    for (i = 0; i < SAMPLES_PER_ITER; i++)
    {
        cgc_int16_t sum = 0;
        for (j = 0; j < ncoeffs; j++)
            sum += (history[i + j] * coeffs[ncoeffs - j - 1]) >> 16;
        samples[i] = sum;
    }
    cgc_memmove(&history[0], &history[SAMPLES_PER_ITER], (ncoeffs - 1) * sizeof(cgc_int16_t));
}

static void cgc_bandpass_filter(cgc_int16_t *samples)
{
    static cgc_int16_t history[BANDPASS_COUNT + SAMPLES_PER_ITER];
    cgc_filter(samples, BANDPASS_COEFFS, BANDPASS_COUNT, history);
}

static void cgc_lowpass_filter(cgc_int16_t *samples)
{
    static cgc_int16_t history[LOWPASS_COUNT + SAMPLES_PER_ITER];
    cgc_filter(samples, LOWPASS_COEFFS, LOWPASS_COUNT, history);
}

static void cgc_convolution(cgc_int16_t *samples)
{
    static cgc_int16_t buf[CONVOLUTION_DELAY + SAMPLES_PER_ITER];
    size_t i;

    cgc_memcpy(&buf[CONVOLUTION_DELAY], samples, SAMPLES_PER_ITER * sizeof(cgc_int16_t));
    for (i = 0; i < SAMPLES_PER_ITER; i++)
        samples[i] = (buf[i] * buf[i + CONVOLUTION_DELAY]) >> 16;
    cgc_memmove(&buf[0], &buf[SAMPLES_PER_ITER], CONVOLUTION_DELAY * sizeof(cgc_int16_t));
}

// samples should be u-law encoded
void cgc_modem_decode(cgc_buffer_t *input, cgc_buffer_t *output)
{
    static int phase = -1;
    int max_idx;
    size_t i;
    cgc_int16_t max_mag;
    cgc_int16_t samples[SAMPLES_PER_ITER];

    for (i = 0; i < SAMPLES_PER_ITER; i++)
    {
        cgc_uint8_t byte;
        cgc_buffer_read_bytes(input, &byte, 1);
        samples[i] = cgc_comp_decode(byte) * 4;
    }

    // apply band-pass cgc_filter to remove noise
    cgc_bandpass_filter(samples);

    // apply cgc_convolution with delay line
    cgc_convolution(samples);

    // apply low-pass cgc_filter to remove high-frequency artifacts
    cgc_lowpass_filter(samples);

    // try and find the middle of the phase
    if (phase == -1)
    {
        max_mag = 0;
        max_idx = 0;
        for (i = 0; i < SAMPLES_PER_ITER; i++)
        {
            if (samples[i] > max_mag)
            {
                max_mag = samples[i];
                max_idx = i;
            }
            else if (-samples[i] > max_mag)
            {
                max_mag = -samples[i];
                max_idx = i;
            }
        }
        phase = (max_idx) % (SAMPLES_PER_BIT / 2);
    }

    // sample at baudrate to get output
    for (i = phase; i < SAMPLES_PER_ITER; i += SAMPLES_PER_BIT)
    {
        int avg = 0;
        int j;
        for (j = 0; j < SAMPLES_PER_BIT / 4; j++)
            avg += samples[i + j]; //< 0 ? -1 : 1;
        avg /= SAMPLES_PER_BIT / 4;
        int bit = avg < 0 ? 1 : 0;
        cgc_buffer_write_bit(output, bit);
    }
}

cgc_uint8_t cgc__modem_encode(int bit)
{
    static cgc_int16_t t;

    // adjust frequency based on input
    t += bit == 0 ? FREQ_1700 : FREQ_2100;

    // TODO could precalc companding algorithm
    // XXX THIS MUST BE SHIFT and NOT DIVIDE!! (C vs Python...)
    return cgc_comp_encode(cgc_fastsin(t) >> 2);
}

void cgc_modem_encode(cgc_buffer_t *input, cgc_buffer_t *output)
{
    int bit, i;

    while ((bit = cgc_buffer_read_bit(input)) != -1)
    {
        for (i = 0; i < SAMPLES_PER_BIT; i++)
        {
            cgc_uint8_t sample = cgc__modem_encode(bit);
            cgc_buffer_write_bytes(output, &sample, 1);
        }
    }
}

void cgc_modem_output(cgc_uint8_t *data, size_t count)
{
    cgc_buffer_t pktbuf;
    cgc_buffer_init(&pktbuf, data, count+1);
    cgc_buffer_write_seek(&pktbuf, count * 8);

    while (cgc_buffer_read_remaining(&pktbuf))
    {
        cgc_frame_encode(&pktbuf, &g_frames);
    }
}

void cgc_modem_encode_frame(cgc_buffer_t *output)
{
    size_t i;
    cgc_uint8_t data[FRAME_SIZE / 8 + 1];
    cgc_buffer_t buf;

    if (cgc_buffer_read_remaining(&g_frames) < FRAME_SIZE)
        return;

    cgc_buffer_init(&buf, data, sizeof(data));
    cgc_buffer_write_bits(&buf, 0x13, 5);

    for (i = 0; i < FRAME_SIZE / 8; i++)
    {
        cgc_uint8_t byte;
        cgc_buffer_read_bytes(&g_frames, &byte, 1);
        cgc_buffer_write_bytes(&buf, &byte, 1);
    }
    
    cgc_modem_encode(&buf, output);
}

void cgc_modem_init()
{
    cgc_ecc_init();
    cgc_buffer_init(&g_frames, g_frames_data, sizeof(g_frames_data));
}
   
#define BUF_SIZE 1024
static cgc_uint8_t buf[BUF_SIZE+1];
static size_t buf_read = 0;
static size_t buf_offset = 0;

int cgc_get_byte(cgc_uint8_t *byte) {
    if (buf_read == buf_offset) {
        if (receive(STDIN, &buf, BUF_SIZE, &buf_read) != 0 || buf_read == 0)
            return -1;
        buf_offset = 0;
    }
    *byte = buf[buf_offset];
    buf_offset++;
    return 0;
}

int cgc_ready_to_read(void) {
    cgc_fd_set read_fds;
    int ret;
    int ready_fd;
    struct cgc_timeval tv;

    if (buf_offset < buf_read) {
        return 1;
    }

    FD_ZERO(&read_fds);
    FD_SET(STDIN, &read_fds);

    tv.tv_sec = 0;
    tv.tv_usec = 1;

    ret = cgc_fdwait(STDIN + 1, &read_fds, NULL, &tv, &ready_fd);

    if (ret != 0) {
        _terminate(1);
    }

    if (FD_ISSET(ready_fd, &read_fds))
        return 1;

    return 0;
}

int cgc_send_n_bytes(int fd, size_t n, unsigned char *buf)
{
    if (!n || !buf)
        return -1;

    size_t tx = 0, total_sent = 0;

    while (total_sent < n) {
        if (transmit(fd, buf + total_sent, n - total_sent, &tx) != 0) {
            _terminate(1);
        } else if (tx == 0) {
            break;
        } else {
            total_sent += tx;
        }
    }

    return total_sent;
}

static cgc_uint8_t out_buf[BUF_SIZE+1];
static size_t out_buf_len = 0;

void cgc_flush_output() {
    if (out_buf_len > 0) {
        cgc_send_n_bytes(STDOUT, out_buf_len, out_buf);
        out_buf_len = 0;
    }
}

void cgc_write_byte(cgc_uint8_t byte) {
    out_buf[out_buf_len++] = byte;

    if (out_buf_len >= BUF_SIZE) {
        cgc_flush_output();
    }
}

void cgc_modem_loop(cgc_modem_rx_cb_t cb)
{
#define BIT_1 0
#define BIT_2 1
#define BIT_3 2
#define BIT_4 3
#define BIT_5 4
#define WAIT_FOR_DATA 5
    int state = 0, recvd = 0;
    cgc_uint64_t mark;
    static cgc_uint8_t indata[1024], decdata[512], pktdata[1024], outdata[32768];
    cgc_buffer_t inbuf, decbuf, pktbuf, outbuf;

    cgc_buffer_init(&inbuf, indata, sizeof(indata));
    cgc_buffer_init(&decbuf, decdata, sizeof(decdata));
    cgc_buffer_init(&pktbuf, pktdata, sizeof(pktdata));
    cgc_buffer_init(&outbuf, outdata, sizeof(outdata));

    while (1)
    {
        cgc_uint8_t u_sample;
        size_t bytes = 1;

        if (cgc_ready_to_read() == 0) {
            // transmit(STDERR, "flush!\n", 7, NULL);
            cgc_flush_output();
        }

        if (cgc_get_byte(&u_sample) != 0)
            break;
//        if (receive(STDIN, &u_sample, 1, &bytes) != 0 || bytes != 1)
//            break;
        recvd++;

        cgc_buffer_write_bytes(&inbuf, &u_sample, 1);
        
        // process samples every 0.01 ms
        if (cgc_buffer_read_remaining(&inbuf) / 8 >= SAMPLES_PER_ITER)
        {
            cgc_modem_decode(&inbuf, &decbuf);

            while (state != WAIT_FOR_DATA && cgc_buffer_read_remaining(&decbuf) >= 8)
            {
                cgc_uint8_t bit;
                bit = cgc_buffer_read_bit(&decbuf);

                switch (state)
                {
                case BIT_1:
                    state = BIT_2;
                    mark = cgc_buffer_read_tell(&decbuf);
                    if (bit != 1)
                        goto reset;
                    break;
                case BIT_2:
                    state = BIT_3;
                    if (bit != 0)
                        goto reset;
                    break;
                case BIT_3:
                    state = BIT_4;
                    if (bit != 0)
                        goto reset;
                    break;
                case BIT_4:
                    state = BIT_5;
                    if (bit != 1)
                        goto reset;
                    break;
                case BIT_5:
                    state = WAIT_FOR_DATA;
                    if (bit != 1)
                        goto reset;
                    break;
                }
                
                continue;
reset:
                state = BIT_1;
                cgc_buffer_read_seek(&decbuf, mark);
            }

            if (state == WAIT_FOR_DATA && cgc_buffer_read_remaining(&decbuf) > FRAME_SIZE)
            {
                int result = cgc_frame_decode(&decbuf, &pktbuf);
                if (result == FRAME_FAIL)
                {
                    goto reset;
                }
                else if (result == FRAME_END)
                {
                    // XXX send packet to callback
                    cb(pktdata, cgc_buffer_read_remaining(&pktbuf) / 8);

                    cgc_buffer_init(&pktbuf, pktdata, sizeof(pktdata));
                }

                state = BIT_1;
            }
        }

        if (cgc_buffer_read_remaining(&outbuf) == 0)
            cgc_modem_encode_frame(&outbuf);

        if (cgc_buffer_read_remaining(&outbuf) > 0)
            cgc_buffer_read_bytes(&outbuf, &u_sample, 1);
        else
            u_sample = cgc__modem_encode(1);

        cgc_write_byte(u_sample);
//        transmit(STDOUT, &u_sample, 1, &bytes);
    }
}
