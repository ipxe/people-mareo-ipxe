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

/** Set most significant bit to 1. */
#define SET_LAST_FRAME( x ) ( (x) | 1 << 31 )
#define GET_FRAME_SIZE( x ) ( (x) & ~( 1 << 31 ) )

#define ONCRPC_HEADER_SIZE ( 11 * sizeof ( uint32_t ) )

#define ONCRPC_CALL 0
#define ONCRPC_REPLY 1

struct oncrpc_cred oncrpc_auth_none = {
	.flavor = ONCRPC_AUTH_NONE,
	.length = 0
};

static int oncrpc_deliver ( struct oncrpc_session *session,
                            struct io_buffer *io_buf,
                            struct xfer_metadata *meta );
static void oncrpc_window_changed ( struct oncrpc_session *session );

static struct interface_operation oncrpc_intf_operations[] = {
	INTF_OP ( intf_close, struct oncrpc_session *, oncrpc_close_session ),
	INTF_OP ( xfer_deliver, struct oncrpc_session *, oncrpc_deliver ),
	INTF_OP ( xfer_window_changed, struct oncrpc_session *,
	          oncrpc_window_changed ),
};

static struct interface_descriptor oncrpc_intf_desc =
	INTF_DESC ( struct oncrpc_session, intf, oncrpc_intf_operations );

static int oncrpc_deliver ( struct oncrpc_session *session,
                            struct io_buffer *io_buf,
                            struct xfer_metadata *meta __unused ) {

	DBGC ( session, "ONCRPC %p Got frame (len=%d)\n", session,
	       (unsigned int) iob_len ( io_buf ) );

	int                      rc;
	struct oncrpc_reply      reply;
	struct oncrpc_cred       verifier;
	oncrpc_callback_t        callback = NULL;

	reply.frame_size   = GET_FRAME_SIZE ( oncrpc_iob_get_int ( io_buf ) );
	reply.rpc_id       = oncrpc_iob_get_int ( io_buf );

	if ( oncrpc_iob_get_int ( io_buf ) != ONCRPC_REPLY )
		return -ENOTSUP;

	reply.reply_state  = oncrpc_iob_get_int ( io_buf );

	if ( reply.reply_state == 0 )
	{
		verifier.flavor = oncrpc_iob_get_int ( io_buf );
		verifier.length = oncrpc_iob_get_int ( io_buf );
		reply.verifier  = &verifier;
		iob_pull ( io_buf, verifier.length );
	}

	reply.accept_state = oncrpc_iob_get_int ( io_buf );
	reply.data         = io_buf;

	struct oncrpc_pending_reply *p;
	struct oncrpc_pending_reply *tmp;

	list_for_each_entry_safe ( p, tmp, &session->pending_reply, list ) {
		if ( reply.rpc_id != p->rpc_id )
			continue;

		callback = p->callback;
		list_del ( &p->list );
		free ( p );
	}

	if ( callback == NULL )
		rc = 0;
	else
		rc = callback ( session, &reply );

	free_iob ( io_buf );
	return rc;
}

static void oncrpc_window_changed ( struct oncrpc_session *session ) {
	int rc;
	struct oncrpc_pending_call *p;
	struct oncrpc_pending_call *tmp;

	list_for_each_entry_safe ( p, tmp, &session->pending_call, list ) {
		if ( xfer_window ( &session->intf ) < iob_len ( p->data ) )
			continue;

		rc = xfer_deliver_iob ( &session->intf,
                                        iob_disown ( p->data ) );
		list_del ( &p->list );
		free ( p );
	}
}

void oncrpc_init_cred_sys ( struct oncrpc_cred_sys *auth_sys, uint32_t uid,
                            uint32_t gid, char *hostname ) {
	auth_sys->hostname    = hostname;
	auth_sys->uid         = uid;
	auth_sys->gid         = gid;
	auth_sys->aux_gid_len = 0;
	auth_sys->stamp       = 0;

	auth_sys->credential.flavor = ONCRPC_AUTH_SYS;
	auth_sys->credential.length = 16 + oncrpc_strlen ( hostname );

}

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

	INIT_LIST_HEAD ( &session->pending_call );
	INIT_LIST_HEAD ( &session->pending_reply );

	intf_init ( &session->intf, &oncrpc_intf_desc, NULL );
}

int oncrpc_connect_named ( struct oncrpc_session *session, uint16_t port,
                           const char *name ) {
	if ( ! session || ! name )
		return -EINVAL;

	struct sockaddr_tcpip peer;
	struct sockaddr_tcpip local;

	memset ( &peer, 0, sizeof ( peer ) );
	memset ( &peer, 0, sizeof ( local ) );
	peer.st_port = htons ( port );
	local.st_port = htons ( rand() % 1024 );

	return xfer_open_named_socket ( &session->intf, SOCK_STREAM,
	                                ( struct sockaddr * ) &peer, name,
                                        ( struct sockaddr * ) &local );
}

void oncrpc_close_session ( struct oncrpc_session *session, int rc ) {
	if ( ! session )
		return;

	struct oncrpc_pending_reply *pr;
	struct oncrpc_pending_reply *tr;

	list_for_each_entry_safe ( pr, tr, &session->pending_reply, list ) {
		list_del ( &pr->list );
		free ( pr );
	}

	struct oncrpc_pending_call *pc;
	struct oncrpc_pending_call *tc;

	list_for_each_entry_safe ( pc, tc, &session->pending_call, list ) {
		free_iob ( pc->data );
		list_del ( &pc->list );
		free ( pc );
	}

	intf_shutdown ( &session->intf, rc );
}

int oncrpc_call_iob ( struct oncrpc_session *session, uint32_t proc_name,
                      struct io_buffer *io_buf, oncrpc_callback_t cb ) {
	if ( ! session )
		return -EINVAL;

	if ( ! io_buf )
		return -EINVAL;

	int rc = 0;
	void *tail;
	size_t header_size, frame_size;
	struct oncrpc_pending_reply *pending_reply;
	struct oncrpc_pending_call *pending_call;

	header_size = ONCRPC_HEADER_SIZE + session->credential->length +
	              session->verifier->length;

	if ( header_size > iob_headroom ( io_buf ) )
		return -EINVAL;

	pending_reply = malloc ( sizeof ( struct oncrpc_pending_reply ) );
	if ( ! pending_reply )
		return -ENOBUFS;

	frame_size = iob_len ( io_buf ) + header_size - sizeof ( uint32_t );

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

	if ( xfer_window ( &session->intf ) < iob_len ( io_buf ) ) {
		pending_call = malloc ( sizeof ( struct oncrpc_pending_call ) );
		if ( ! pending_call ) {
			free ( pending_reply );
			return -ENOBUFS;
		}

		INIT_LIST_HEAD ( &pending_call->list );
		pending_call->data = io_buf;
		list_add ( &pending_call->list, &session->pending_call );
		rc = 0;
	} else {
		rc = xfer_deliver_iob ( &session->intf, iob_disown ( io_buf ) );
		if ( rc != 0  ) {
			free ( pending_reply );
			return rc;
		}
	}

	INIT_LIST_HEAD ( &pending_reply->list );
	pending_reply->callback = cb;
	pending_reply->rpc_id = session->rpc_id;
	list_add ( &pending_reply->list, &session->pending_reply );

	return rc;
}
