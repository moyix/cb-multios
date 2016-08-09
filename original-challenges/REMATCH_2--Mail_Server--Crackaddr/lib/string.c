/*

Copyright (c) 2015 Cromulence LLC

Authors: Cromulence <cgc@cromulence.com>

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
#include <string.h>
#include <stdint.h>

size_t cgc_strlen( const char *str )
{
	size_t len = 0;
	while ( *str++ != '\0' )
		len++;

	return len;
}

void cgc_bzero(void *s, size_t n) {
        while (n) {
                ((char *)s)[--n] = '\0';
        }
        ((char *)s)[n] = '\0';
}

void *cgc_memset( void *ptr, int value, size_t num )
{
	void *ptr_temp = ptr;
	cgc_uint8_t set_value_byte = (cgc_uint8_t)value;
	cgc_uint32_t set_value_dword = (set_value_byte << 24) | (set_value_byte << 16) | (set_value_byte << 8) | set_value_byte;

	while ( num >= 4 )
	{
		*((cgc_uint32_t*)ptr) = set_value_dword;	
		ptr+=4;
		num-=4;	
	}

	while ( num > 0 )
	{
		*((cgc_uint8_t*)ptr++) = set_value_byte;	
		num--;
	}

	return (ptr_temp);
}

char *cgc_strchr(char *s, int c) {
	cgc_uint32_t i;

	if (!s) {
		return(NULL);
	}
	for (i = 0; i < cgc_strlen(s); i++) {
		if (s[i] == c) {
			return(s+i);
		}
	}

	return(NULL);

}

char *StrtokNext = NULL;
char *cgc_strtok(char *str, char *sep) {
	cgc_uint32_t i, j;
	cgc_uint32_t str_len;
	char *tok;

	if (!sep) {
		return(NULL);
	}

	if (!str) {
		if (!StrtokNext) {
			return(NULL);
		} else {
			str = StrtokNext;
		}
	}

	// deal with any leading sep chars
	while (cgc_strchr(sep, *str) && *str != '\0') {
		str++;
	}
	if (*str == '\0') {
		StrtokNext = NULL;
		return(NULL);
	}

	str_len = cgc_strlen(str);
	for (i = 0; i < str_len; i++) {
		if (cgc_strchr(sep, str[i])) {
			// found a sep character
			str[i] = '\0';
			// see if there are any subsequent tokens
			for (j = i+1; j < str_len; j++) {
				if (cgc_strchr(sep, str[j])) {
					// found one
					str[j] = '\0';
				} else {
					// no more tokens
					StrtokNext = str+j;
					return(str);
				}
			}
			if (j == str_len) {
				StrtokNext = NULL;
			}
			return(str);
		}
	}

	// made it to the end of the string without any new tokens
	StrtokNext = NULL;
	return(str);
}

int cgc_strcmp(const char *s1, const char *s2) {
	while (*s1 != '\0' && *s1 == *s2) {
		s1++; s2++;
	}
	return *s1 - *s2;
}

int cgc_strncmp(const char *s1, const char *s2, size_t n) {
	while (*s1 != '\0' && *s1 == *s2 && n > 0) {
		s1++; s2++; n--;
	}
	if (n == 0) {
		return 0;
	}
	else {
		return *s1 - *s2;
	}
}

char *cgc_strcat(char *restrict s1, const char *restrict s2) {
	char *dest = s1;
	while(*dest != '\0') dest++;
	while (*s2 != '\0') {
		*dest++ = *s2++;
	}
	*dest = '\0';
	return s1;
}

char *cgc_strstr(char *s1, char *s2) {
	if ((s1 == NULL) || (s2 == NULL)) {
		return NULL;
	}
	int s2len = cgc_strlen(s2);
	while(*s1 != '\0') {
		if (cgc_strncmp(s1, s2, s2len) == 0) {
			return s1;
		}
		s1++;
	}
	return NULL;
}


char *cgc_rindex(char *source, char match) {
	int length = cgc_strlen(source);
	for(int i=length; i>=0; i--) {
		if (source[i] == match) {
			return (source + i);
		}
	}
	return NULL;
}

