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

/** @file
 *
 * SUN ONC RPC protocol
 *
 */

/** Set most significant bit to 1. */
#define SET_LAST_FRAME( x ) ( (x) |= 1 << 31 )

#define ONCRPC_HEADER_MAX_SIZE ( sizeof ( struct oncrpc_query_header ) + 512 )

#define ONCRPC_CALL 0
#define ONCRPC_REPLY 1

struct oncrpc_query_header {
	uint32_t                frame_size;
	uint32_t                rpc_id;
	uint32_t                message_type;
	uint32_t                rpc_vers;
	uint32_t                prog_name;
	uint32_t                prog_vers;
	uint32_t                proc_name;
} __packed;

static int iob_add_cred ( struct io_buffer *io_buf, struct oncrpc_cred *cred ) {
	if ( ! io_buf)
		return -EINVAL;
	if ( ! cred )
		return -EINVAL;

	struct oncrpc_cred *buf_cred = io_buf->tail;
	iob_put ( io_buf,  sizeof ( struct oncrpc_cred ) );
	buf_cred->flavor = htonl ( cred->flavor );
	buf_cred->length = htonl ( cred->length );

	struct oncrpc_cred_sys *syscred = ( void * ) cred;

	switch ( cred->flavor ) {
		case ONCRPC_AUTH_NONE:
			break;

		case ONCRPC_AUTH_SYS:
			return -ENOTSUP;

		case ONCRPC_AUTH_SHORT:
			memcpy ( io_buf->tail, syscred->short_id,
			         cred->length );
			iob_put ( io_buf, cred->length );
			break;
	}

	return 0;
}

int oncrpc_call_iob ( struct interface *intf, struct oncrpc_session *session,
                       uint32_t proc_name, struct io_buffer *io_buf ) {
	if ( ! intf )
		return -EINVAL;
	if ( ! session )
		return -EINVAL;

	int rc;
	uint32_t frame_size;
	struct io_buffer *call_buf = alloc_iob ( ONCRPC_HEADER_MAX_SIZE +
	                                         iob_len ( io_buf ) );

	if ( ! call_buf )
		return -ENOBUFS;

	iob_put ( call_buf, sizeof ( struct oncrpc_query_header ) );
	iob_add_cred ( call_buf, session->credential );
	iob_add_cred ( call_buf, session->verifier );

	struct oncrpc_query_header *header = ( void * ) call_buf->data;

	frame_size = iob_len ( call_buf ) + iob_len ( io_buf ) -
                     sizeof ( frame_size );
	SET_LAST_FRAME ( frame_size );

	header->frame_size   = htonl ( frame_size );
	header->rpc_id       = htonl ( session->rpc_id++ );
	header->message_type = htonl ( ONCRPC_CALL );
	header->rpc_vers     = htonl ( ONCRPC_VERS );
	header->prog_name    = htonl ( session->prog_name );
	header->prog_vers    = htonl ( session->prog_vers );
	header->proc_name    = htonl ( proc_name );

	memcpy ( call_buf->tail, io_buf->data, iob_len (io_buf));
	iob_put ( call_buf, iob_len (io_buf) );

	if ( ( rc = xfer_deliver_iob ( intf, call_buf ) ) != 0 )
		free_iob ( call_buf );

	free_iob ( io_buf );
	return rc;
}
