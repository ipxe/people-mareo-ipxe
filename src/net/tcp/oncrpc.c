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
#include <ipxe/oncrpc.h>
#include <ipxe/oncrpc_iob.h>
#include <ipxe/init.h>
#include <ipxe/settings.h>

/** @file
 *
 * SUN ONC RPC protocol
 *
 */

/** Set most significant bit to 1. */
#define SET_LAST_FRAME( x ) ( (x) | 1 << 31 )
#define GET_FRAME_SIZE( x ) ( (x) & ~( 1 << 31 ) )

#define ONCRPC_CALL     0
#define ONCRPC_REPLY    1

/** AUTH NONE authentication flavor */
struct oncrpc_cred oncrpc_auth_none = {
	.flavor = ONCRPC_AUTH_NONE,
	.length = 0
};

#define DHCP_EB_UID DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 0xc2 )
#define DHCP_EB_GID DHCP_ENCAP_OPT ( DHCP_EB_ENCAP, 0xc3 )

struct setting uid_setting __setting ( SETTING_AUTH ) = {
	.name        = "uid",
	.description = "UID",
	.tag         = DHCP_EB_UID,
	.type        = &setting_type_uint32
};

struct setting gid_setting __setting ( SETTING_AUTH ) = {
	.name        = "gid",
	.description = "GID",
	.tag         = DHCP_EB_GID,
	.type        = &setting_type_uint32
};

/**
 * Initialize an ONC RPC AUTH SYS credential structure
 *
 * @v auth_sys          The structure to initialize
 *
 * The hostname field is filled with the value of the hostname setting, if the
 * hostname setting is empty, "iPXE" is used instead.
 */
int oncrpc_init_cred_sys ( struct oncrpc_cred_sys *auth_sys ) {
	if ( ! auth_sys )
		return -EINVAL;

	fetch_string_setting_copy ( NULL, &hostname_setting,
	                            &auth_sys->hostname );
	if ( ! auth_sys->hostname )
		auth_sys->hostname = strdup ( "iPXE" );

	if ( ! auth_sys->hostname )
		return -ENOMEM;

	auth_sys->uid         = fetch_uintz_setting ( NULL, &uid_setting );
	auth_sys->gid         = fetch_uintz_setting ( NULL, &uid_setting );
	auth_sys->aux_gid_len = 0;
	auth_sys->stamp       = 0;

	auth_sys->credential.flavor = ONCRPC_AUTH_SYS;
	auth_sys->credential.length = 16 +
	                              oncrpc_strlen ( auth_sys->hostname );

	return 0;
}

/**
 * Prepare an ONC RPC session structure to be used by the ONC RPC layer
 *
 * @v session           ONC RPC session
 * @v credential        Credential structure pointer
 * @v verifier          Verifier structure pointer
 * @v prog_name         ONC RPC program number
 * @v prog_vers         ONC RPC program version number
 */
void oncrpc_init_session ( struct oncrpc_session *session,
                           struct oncrpc_cred *credential,
                           struct oncrpc_cred *verifier, uint32_t prog_name,
                           uint32_t prog_vers ) {
	if ( ! session )
		return;

	session->rpc_id     = rand();
	session->credential = credential;
	session->verifier   = verifier;
	session->prog_name  = prog_name;
	session->prog_vers  = prog_vers;
}

/**
 * Parse an I/O buffer to extract an ONC RPC REPLY
 * @v session	        ONC RPC session
 * @v reply             Reply structure where data will be saved
 * @v io_buf            I/O buffer
 */
int oncrpc_get_reply ( struct oncrpc_session *session __unused,
                       struct oncrpc_reply *reply, struct io_buffer *io_buf ) {
	if ( ! reply || ! io_buf )
		return -EINVAL;

	reply->frame_size = GET_FRAME_SIZE ( oncrpc_iob_get_int ( io_buf ) );
	reply->rpc_id     = oncrpc_iob_get_int ( io_buf );

	/* iPXE has no support for ONC RPC call */
	if ( oncrpc_iob_get_int ( io_buf ) != ONCRPC_REPLY )
		return -EPROTO;

	reply->reply_state = oncrpc_iob_get_int ( io_buf );

	if ( reply->reply_state == 0 )
	{
		/* verifier.flavor */
		oncrpc_iob_get_int ( io_buf );
		/* verifier.length */
		iob_pull ( io_buf, oncrpc_iob_get_int ( io_buf ));

		/* We don't use the verifier in iPXE, let it be an empty
		   verifier. */
		reply->verifier = &oncrpc_auth_none;
	}

	reply->accept_state = oncrpc_iob_get_int ( io_buf );
	reply->data         = io_buf;

	return 0;
}

/**
 * Send an ONC RPC call
 *
 * @v intf              Interface to send the request on
 * @v session           ONC RPC session
 * @v proc_name         ONC RPC procedure number
 * @v io_buf            I/O buffer
 * */
int oncrpc_call_iob ( struct interface *intf, struct oncrpc_session *session,
                      uint32_t proc_name, struct io_buffer *io_buf ) {
	if ( ! session || ! io_buf)
		return -EINVAL;

	void    *tail;
	size_t  header_size, frame_size;

	header_size = ONCRPC_HEADER_SIZE + session->credential->length +
	              session->verifier->length;

	if ( header_size > iob_headroom ( io_buf ) )
		return -EINVAL;

	frame_size = iob_len ( io_buf ) + header_size - sizeof ( uint32_t );

	if ( xfer_window ( intf ) < iob_len ( io_buf ) + header_size )
		return -EAGAIN;

	tail = io_buf->tail;
	io_buf->tail = iob_push ( io_buf, header_size );
	oncrpc_iob_add_int ( io_buf, SET_LAST_FRAME ( frame_size ) );
	oncrpc_iob_add_int ( io_buf, ++session->rpc_id );
	oncrpc_iob_add_int ( io_buf, ONCRPC_CALL );
	oncrpc_iob_add_int ( io_buf, ONCRPC_VERS );
	oncrpc_iob_add_int ( io_buf, session->prog_name );
	oncrpc_iob_add_int ( io_buf, session->prog_vers );
	oncrpc_iob_add_int ( io_buf, proc_name );
	oncrpc_iob_add_cred ( io_buf, session->credential );
	oncrpc_iob_add_cred ( io_buf, session->verifier );
	io_buf->tail = tail;

	return xfer_deliver_iob ( intf, iob_disown ( io_buf ) );
}
