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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "asl6.h"
#include "libcgc.h"

#define MAX_SIZE (sizeof(cgc_uint8_t) * 1024 * 32)
int cgc_fdprintf(int fd, const char *fmt, ...);

static int cgc_read_exactly(int fd, cgc_uint8_t *buf, size_t n)
{

  size_t rx;
  while (n > 0) {
    if (receive(fd, buf, n, &rx) != 0)
      return -1;
    n -= rx;
    buf += n;
  }

  return 0;
}

int main(void)
{
  unsigned size = 0;
  size_t read = 0;
  cgc_uint8_t buf[MAX_SIZE];
  cgc_memset(buf, 0, sizeof(buf));

  if (receive(STDIN, &size, sizeof(size), &read) != 0)
    cgc_exit(1);

  if (read != 4)
    cgc_exit(1);

  if (size > MAX_SIZE) {
    printf("too big\n");
    cgc_exit(1);
  }

  if (cgc_read_exactly(STDIN, buf, size) != 0)
    cgc_exit(1);

  cgc_element *e = cgc_decode(buf, (unsigned) buf + size);
  if (e == NULL)
    cgc_exit(1);

  cgc_pprint(e);
}
