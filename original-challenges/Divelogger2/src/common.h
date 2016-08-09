/*

Author: Jason Williams <jdw@cromulence.com>

Copyright (c) 2014 Cromulence LLC

Permission is hereby granted, cgc_free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/
#ifndef __COMMON_H__
#define __COMMON_H__

extern "C" {
#include <stdlib.h>
#include <libcgc.h>
#include <stdint.h>
}

#ifdef NULL
#undef NULL
#define NULL (0)
#endif

#include <string.h>

#include "dlqueue.h"
#include "datetime_helper.h"
#include "datetime.h"
#include "diverinfo.h"
#include "dive.h"
#include "commandhandler.h"

void cgc_ReadLine( cgc_String &sLine );
void cgc_ReadUint32( cgc_uint32_t &value );
cgc_uint32_t cgc_StringToInt( const cgc_String &sInStr );

#endif // __COMMON_H___