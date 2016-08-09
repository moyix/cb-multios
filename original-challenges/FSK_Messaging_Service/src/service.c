/*

Author: Jason Williams <jdw@cromulence.com>

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
#include <libcgc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <prng.h>

#include "common.h"
#include "rx.h"
#include "baseband.h"
#include "packet.h"

#define BYTE_RECEIVE_MAX	(SAMPLE_FS*4)	// 4 seconds

void cgc_init_prng( void )
{
	cgc_uint32_t randomData[8];
	size_t random_bytes_populated;

	if ( cgc_random( (void*)randomData, sizeof(cgc_uint32_t)*8, &random_bytes_populated ) != 0 )
	{
		cgc_printf( "[[ERROR RANDOM FAILED]]\n" );
		_terminate(1);
	}

	cgc_seed_prng_array( randomData, 8 );
}

int __attribute__((fastcall)) main(int secret_page_i, char *unused[]) 
{
	void *secret_page = (void *)secret_page_i;

	cgc_tBasebandState oBaseband;

	cgc_uint32_t ts = ((cgc_uint32_t*)secret_page)[0] + ((cgc_uint32_t*)secret_page)[1] + ((cgc_uint32_t*)secret_page)[2] + ((cgc_uint32_t*)secret_page)[3];
	ts &= 0x7FFFF;

	ts += 1452975630;

	cgc_printf( "[[RECEIVER STARTED -- TIMESTAMP: $d]]\n", ts );

	// Initialize random number generator
	cgc_init_prng();

	// Initialize baseband
	cgc_init_baseband( &oBaseband );

	// Initialize packet handler
	cgc_init_packet_handler( );

	// Begin receiing packets
	for ( cgc_uint32_t byteCount = 0; byteCount < BYTE_RECEIVE_MAX; )
	{
		cgc_uint8_t rx_sample[256];
		size_t rx_count;

        	if ( receive( STDIN, rx_sample, 256, &rx_count) != 0) 
		{
			// Failed ??
			cgc_printf( "[[CONNECTION CLOSED EARLY]]\n" );
            		_terminate(1);
        	}

		if ( rx_count < 1 )
		{
			// Failed ??
			cgc_printf( "[[RECEIVE ERROR]]\n" );
            		_terminate(1);
		}

		for ( cgc_uint32_t i = 0; i < rx_count; i++ )
		{
			if ( byteCount >= BYTE_RECEIVE_MAX )
				break;

			cgc_uint8_t sample_result = cgc_receive_sample( rx_sample[i] );

			cgc_process_sample( &oBaseband, sample_result );

			byteCount++;
		}
	}

	// Display any received packets
	cgc_display_packets();

	// Destroy the packet handler
	cgc_destroy_packet_handler();

	return 0;
}
