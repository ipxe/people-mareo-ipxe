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
#include <ipxe/timer.h>
#include <ipxe/oncrpc.h>
#include <ipxe/oncrpc_iob.h>
#include <ipxe/portmap.h>

/** @file
 *
 * PORTMAPPER protocol.
 *
 */

static struct oncrpc_cred_sys oncrpc_auth_sys __unused = {
	.credential = { ONCRPC_AUTH_SYS, 20 + 8 },
	.stamp = 0,
	.hostname = "atlas",
	.uid = 0,
	.gid = 0,
	.aux_gid_len = 0,
	.aux_gid = { 0 }
};

int portmap_init_session ( struct oncrpc_session *session, uint16_t port,
                            const char *name) {
	if ( ! session )
		return -EINVAL;

	if ( ! port )
		port = PORTMAP_PORT;

	oncrpc_init_session ( session, &oncrpc_auth_none,
                              &oncrpc_auth_none, ONCRPC_PORTMAP,
                              PORTMAP_VERS );

	return oncrpc_connect_named ( session, port, name );
}

void portmap_close_session ( struct oncrpc_session *session, int rc ) {
	oncrpc_close_session ( session, rc );
}

int portmap_getport ( struct oncrpc_session *session, uint32_t prog,
                      uint32_t vers, uint32_t prot, oncrpc_callback_t cb ) {
	int rc;
	struct io_buffer *call_buf;

	if ( ! session )
		return -EINVAL;

	call_buf = oncrpc_alloc_iob ( session, 4 * sizeof ( uint32_t ) );

	if ( ! call_buf )
		return -ENOBUFS;

	oncrpc_iob_add_int ( call_buf, prog );
	oncrpc_iob_add_int ( call_buf, vers );
	oncrpc_iob_add_int ( call_buf, prot );
	oncrpc_iob_add_int ( call_buf, 0 );

	rc = oncrpc_call_iob ( session, PORTMAP_GETPORT, call_buf, cb );

	if ( rc != 0 )
		free_iob ( call_buf );

	return rc;
}

int portmap_callit ( struct oncrpc_session *session, uint32_t prog,
                     uint32_t vers, uint32_t proc, struct io_buffer *io_buf,
                     oncrpc_callback_t cb ) {
	int rc;
	struct io_buffer *call_buf;

	if ( ! session )
		return -EINVAL;

	call_buf = oncrpc_alloc_iob ( session, 4 * sizeof ( uint32_t ) +
	           iob_len ( io_buf ) );
	if ( ! call_buf )
		return -ENOBUFS;

	oncrpc_iob_add_int ( call_buf, prog );
	oncrpc_iob_add_int ( call_buf, vers );
	oncrpc_iob_add_int ( call_buf, proc );
	oncrpc_iob_add_int ( call_buf, iob_len ( io_buf ) );
	memcpy ( iob_put ( call_buf, iob_len ( io_buf ) ), io_buf->data,
	         iob_len ( io_buf ) );

	rc = oncrpc_call_iob ( session, PORTMAP_CALLIT, call_buf, cb );

	if ( rc != 0 )
		free_iob ( call_buf );
	else
		free_iob ( io_buf );

	return rc;
}
