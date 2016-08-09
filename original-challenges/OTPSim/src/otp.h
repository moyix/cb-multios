/*
 * Copyright (c) 2015 Kaprica Security, Inc.
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

#ifndef OTP_H
#define OTP_H

#include <stdint.h>
#include <string.h>

#define MAX_OTP_LEN 128

typedef struct otp {
    size_t data_len;
    unsigned char data[MAX_OTP_LEN];
    unsigned int seed_value;
    unsigned int seed_idx;
    cgc_uint8_t num_reqs;
} cgc_otp_t;

void cgc_otp_handshake(cgc_otp_t **o);
void cgc_otp_generate_otp(cgc_otp_t *o);
void cgc_otp_extend_session(cgc_otp_t *o);
void cgc_otp_set_seeds(cgc_otp_t *o);
void cgc_otp_verify_otp(cgc_otp_t *o);

#endif
