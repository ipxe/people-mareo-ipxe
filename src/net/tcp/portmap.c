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
#include <ipxe/portmap.h>

/** @file
 *
 * PORTMAPPER protocol.
 *
 */

static struct oncrpc_cred oncrpc_auth_none = {
	.flavor = ONCRPC_AUTH_NONE,
	.length = 0
};

struct portmap_mapping {
	uint32_t                prog;
	uint32_t                vers;
	uint32_t                prot;
	uint32_t                port;
} __packed;

void portmap_init_session ( struct oncrpc_session *session ) {
	if ( ! session )
		return;

	session->credential = &oncrpc_auth_none;
	session->verifier = &oncrpc_auth_none;
	session->rpc_id = currticks() << 16;
	session->prog_name = ONCRPC_PORTMAP;
	session->prog_vers = PORTMAP_VERS;
}

int portmap_getport ( struct interface *intf, struct oncrpc_session *session,
                      uint32_t prog, uint32_t vers, uint32_t prot ) {
	if ( ! intf )
		return -EINVAL;
	if ( ! session )
		return -EINVAL;

	struct io_buffer call_buf;
	struct portmap_mapping query = {
		.prog = htonl ( prog ),
		.vers = htonl ( vers ),
		.prot = htonl ( prot ),
		.port = htonl ( 0 )
	};

	iob_populate ( &call_buf, &query, sizeof ( query ), sizeof ( query ) );

	return oncrpc_call_iob ( intf, session, PORTMAP_GETPORT, &call_buf );
}
