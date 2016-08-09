/*

Author: Joe Rogers <joe@cromulence.com>

Copyright (c) 2015 Cromulence LLC

Permission is hereby granted, free of charge, to any person obtaining a copy
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
#ifndef FS_H
#define FS_H
#include <libcgc.h>
#include "stdint.h"

#define PERMS_READ (0x4)
#define PERMS_WRITE (0x2)

#define MAX_FILES (16)
#define MAX_FILE_SIZE (4096)
typedef struct _filesystem {
	char Filename[32];
	char Data[MAX_FILE_SIZE];
	char Owner[32];
	char Group[32];
	cgc_uint16_t Perms;
	cgc_uint16_t Size;
} cgc_Filesystem, *cgc_pFilesystem;

typedef struct _file {
	cgc_pFilesystem fp;
	char *CurrPosition;
	cgc_uint8_t mode;
} cgc_FILE, *cgc_pFILE;

void cgc_InitFilesystem(void);
void cgc_ListFiles(void);
cgc_pFILE cgc_fopen(char *Filename, char *Mode, cgc_uint8_t Suid);
char *cgc_fgets(char *str, cgc_uint32_t size, cgc_pFILE stream);
cgc_uint8_t cgc_fclose(cgc_pFILE stream);
size_t cgc_fread(void *restrict ptr, size_t size, size_t nitems, cgc_FILE *restrict stream);
size_t cgc_fwrite(const void *restrict ptr, size_t size, size_t nitems, cgc_FILE *restrict stream);
cgc_uint8_t cgc_Dump(char *filename);

#endif
