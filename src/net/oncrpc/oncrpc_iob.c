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

/**
 * Allocate an I/O buffer suited for use by the ONC RPC layer
 *
 * @v session            ONC RPC session
 * @v size               Size of the buffer
 *
 * This function ensure that the I/O buffer has suffisant headroom for an ONC
 * RPC header, if the credential or the verifier field of the ONC RPC
 * session changes, previously allocated buffers might not be suited for use
 * anymore.
 */
struct io_buffer *oncrpc_alloc_iob ( const struct oncrpc_session *session,
                                     size_t size ) {
	struct io_buffer        *io_buf;
	size_t                  header_size;

	if ( ! session )
		return NULL;

	header_size = ONCRPC_HEADER_SIZE + session->credential->length +
	              session->verifier->length;

	if ( ! ( io_buf = alloc_iob ( size + header_size ) ) )
		return NULL;

	iob_reserve ( io_buf, header_size );
	return io_buf;
}

/**
 * Add a string to the end of an I/O buffer
 *
 * @v io_buf            I/O buffer
 * @v val               String
 * @ret size            Size of the data written
 *
 * In the ONC RPC protocol, every data is four byte paded, we add padding when
 * necessary by using oncrpc_align()
 */
size_t oncrpc_iob_add_string ( struct io_buffer *io_buf, const char *val ) {
	size_t                  len;
	size_t                  padding;

	len     = strlen ( val );
	padding = oncrpc_align ( len ) - len;

	oncrpc_iob_add_int ( io_buf, len );

	strcpy ( iob_put ( io_buf, len ), val );
	memset ( iob_put ( io_buf, padding ), 0, padding );

	return len + padding + sizeof ( uint32_t );
}

/**
 * Add an int array to the end of an I/O buffer
 *
 * @v io_buf            I/O buffer
 * @v length            Length od the array
 * @v val               Int array
 * @ret size            Size of the data written
 */
size_t oncrpc_iob_add_intarray ( struct io_buffer *io_buf, size_t length,
                                 const uint32_t *array ) {
	size_t                  i;

	oncrpc_iob_add_int ( io_buf, length );

	for ( i = 0; i < length; ++i )
		oncrpc_iob_add_int ( io_buf, array[i] );

	return ( ( length + 1 ) * sizeof ( uint32_t ) );
}

/**
 * Add credential information to the end of an I/O buffer
 *
 * @v io_buf            I/O buffer
 * @v cred              Credential information
 * @ret size            Size of the data written
 */
size_t oncrpc_iob_add_cred ( struct io_buffer *io_buf,
                             struct oncrpc_cred *cred ) {
	struct oncrpc_cred_sys  *syscred;
	size_t                  s;

	if ( ! io_buf || ! cred )
		return 0;

	s  = oncrpc_iob_add_int ( io_buf, cred->flavor );
	s += oncrpc_iob_add_int ( io_buf, cred->length );

	switch ( cred->flavor ) {
	case ONCRPC_AUTH_NONE:
		break;

	case ONCRPC_AUTH_SYS:
		syscred = container_of ( cred, struct oncrpc_cred_sys,
		                         credential );
		s += oncrpc_iob_add_int ( io_buf, syscred->stamp );
		s += oncrpc_iob_add_string ( io_buf,syscred->hostname );
		s += oncrpc_iob_add_int ( io_buf, syscred->uid );
		s += oncrpc_iob_add_int ( io_buf, syscred->gid );
		s += oncrpc_iob_add_intarray ( io_buf, syscred->aux_gid_len,
		                               syscred->aux_gid );
		break;
	}

	return s;
}

/**
 * Get credential information from the beginning of an I/O buffer
 *
 * @v io_buf            I/O buffer
 * @v cred              Struct where the information will be saved
 * @ret size            Size of the data read
 */
size_t oncrpc_iob_get_cred ( struct io_buffer *io_buf,
                             struct oncrpc_cred *cred ) {
	if ( cred == NULL )
		return * ( uint32_t * ) io_buf->data;

	cred->flavor = oncrpc_iob_get_int ( io_buf );
	cred->length = oncrpc_iob_get_int ( io_buf );

	iob_pull ( io_buf, cred->length );

	return ( 2 * sizeof ( uint32_t ) + cred->length );
}
