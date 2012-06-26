/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/socket.h>
#include <ipxe/tcpip.h>
#include <ipxe/in.h>
#include <ipxe/iobuf.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/uri.h>
#include <ipxe/features.h>
#include <ipxe/oncrpc.h>
#include <ipxe/oncrpc_iob.h>

/** @file
 *
 * SUN ONC RPC protocol
 *
 */

#define ONCRPC_HEADER_SIZE ( 11 * sizeof ( uint32_t ) )

struct io_buffer *oncrpc_alloc_iob ( const struct oncrpc_session *session,
                                     size_t len ) {
	if ( ! session )
		return NULL;

	struct io_buffer *io_buf;
	size_t header_size;

	header_size = ONCRPC_HEADER_SIZE + session->credential->length +
	              session->verifier->length;

	if ( ! ( io_buf = alloc_iob ( len + header_size ) ) )
		return NULL;

	iob_reserve ( io_buf, header_size );
	return io_buf;
}

size_t oncrpc_strlen ( const char *str )
{
	size_t len;

	len = sizeof ( uint32_t ) + strlen ( str );
	while ( len % 4 )
		++len;

	return len;
}

size_t oncrpc_iob_add_string ( struct io_buffer *io_buf, const char *val ) {
	const char *s;

	oncrpc_iob_add_int ( io_buf, strlen ( val ) );

	for ( s = val; *s != '\0'; ++s )
		* ( char * ) iob_put ( io_buf, sizeof ( *s ) ) = *s;

	while ( ( s++ - val ) % 4 != 0 )
		* ( char * ) iob_put ( io_buf, sizeof ( *s ) ) = '\0';

	return ( ( s - val ) - 1 + sizeof ( uint32_t ) );
}

size_t oncrpc_iob_add_intarray ( struct io_buffer *io_buf, size_t size,
                                 const uint32_t *array ) {
	size_t i;

	oncrpc_iob_add_int ( io_buf, size );

	for ( i = 0; i < size; ++i )
		oncrpc_iob_add_int ( io_buf, array[i] );

	return ( ( size + 1 ) * sizeof ( uint32_t ) );
}

size_t oncrpc_iob_add_cred ( struct io_buffer *io_buf,
                             struct oncrpc_cred *cred ) {
	if ( ! io_buf || ! cred )
		return 0;

	size_t s = 0;

	s += oncrpc_iob_add_int ( io_buf, cred->flavor );
	s += oncrpc_iob_add_int ( io_buf, cred->length );

	struct oncrpc_cred_sys *syscred = ( void * ) cred;
	switch ( cred->flavor ) {
		case ONCRPC_AUTH_NONE:
			break;

		case ONCRPC_AUTH_SYS:
			s += oncrpc_iob_add_int ( io_buf, syscred->stamp );
			s += oncrpc_iob_add_string ( io_buf,
			                             syscred->hostname );
			s += oncrpc_iob_add_int ( io_buf, syscred->uid );
			s += oncrpc_iob_add_int ( io_buf, syscred->gid );
			s += oncrpc_iob_add_intarray ( io_buf,
			                               syscred->aux_gid_len,
			                               syscred->aux_gid );
			break;
	}

	return s;
}

size_t oncrpc_iob_get_cred ( struct io_buffer *io_buf,
                             struct oncrpc_cred *cred ) {
	cred->flavor = oncrpc_iob_get_int ( io_buf );
	cred->length = oncrpc_iob_get_int ( io_buf );

	return ( 2 * sizeof ( uint32_t ) );
}
