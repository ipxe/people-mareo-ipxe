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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
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
#include <ipxe/timer.h>
#include <ipxe/oncrpc.h>
#include <ipxe/oncrpc_iob.h>
#include <ipxe/portmap.h>

/** @file
 *
 * PORTMAPPER protocol.
 *
 */

/** PORTMAP GETPORT procedure. */
#define PORTMAP_GETPORT 3

/**
 * Send a GETPORT request
 *
 * @v intf              Interface to send the request on
 * @v session           ONC RPC session
 * @v prog              ONC RPC program number
 * @v vers              ONC RPC rogram version number
 * @v proto             Protocol (TCP or UDP)
 * @ret rc              Return status code
 */
int portmap_getport ( struct interface *intf, struct oncrpc_session *session,
                      uint32_t prog, uint32_t vers, uint32_t proto ) {
	int rc;
	struct io_buffer *call_buf;

	if ( ! session )
		return -EINVAL;

	call_buf = oncrpc_alloc_iob ( session, 4 * sizeof ( uint32_t ) );

	if ( ! call_buf )
		return -ENOBUFS;

	oncrpc_iob_add_int ( call_buf, prog );
	oncrpc_iob_add_int ( call_buf, vers );
	oncrpc_iob_add_int ( call_buf, proto );
	oncrpc_iob_add_int ( call_buf, 0 );

	rc = oncrpc_call_iob ( intf, session, PORTMAP_GETPORT, call_buf );

	if ( rc != 0 )
		free_iob ( call_buf );

	return rc;
}

/**
 * Parse a GETPORT reply
 *
 * @v getport_reply     A structure where the data will be saved
 * @v reply             The ONC RPC reply to get data from
 * @ret rc              Return status code
 */
int portmap_get_getport_reply ( struct portmap_getport_reply *getport_reply,
                                struct oncrpc_reply *reply ) {
	if ( ! getport_reply || ! reply )
		return -EINVAL;

	getport_reply->port = oncrpc_iob_get_int ( reply->data );
	if ( getport_reply == 0 || getport_reply->port >= 65536 )
		return -EINVAL;

	return 0;
}
