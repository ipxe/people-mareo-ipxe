#ifndef _IPXE_ONCRPC_H
#define _IPXE_ONCRPC_H

#include <stdint.h>
#include <ipxe/interface.h>
#include <ipxe/iobuf.h>
#include <ipxe/refcnt.h>

/** @file
 *
 * SUN ONC RPC protocol.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** ONC RCP Version */
#define ONCRPC_VERS 2

/** ONC RPC Null Authentication */
#define ONCRPC_AUTH_NONE 0

/** ONC RPC System Authentication (also called UNIX Authentication) */
#define ONCRPC_AUTH_SYS 1

struct oncrpc_cred {
	uint32_t               flavor;
	uint32_t               length;
};

struct oncrpc_cred_sys {
	struct oncrpc_cred     credential;
	uint32_t               stamp;
	char                   *hostname;
	uint32_t               uid;
	uint32_t               gid;
	uint32_t               aux_gid_len;
	uint32_t               aux_gid[16];
};

struct oncrpc_session {
	struct refcnt           refcnt;
	struct interface        intf;
	struct list_head        pending_reply;
	struct list_head        pending_call;
	struct oncrpc_cred      *credential;
	struct oncrpc_cred      *verifier;
	uint32_t                rpc_id;
	uint32_t                prog_name;
	uint32_t                prog_vers;
};

struct oncrpc_reply
{
	struct oncrpc_cred      *verifier;
	uint32_t                rpc_id;
	uint32_t                reply_state;
	uint32_t                accept_state;
	uint32_t                frame_size;
	struct io_buffer        *data;
};

typedef int ( *oncrpc_callback_t ) ( struct oncrpc_session *session,
                                     struct oncrpc_reply *reply );
struct oncrpc_pending_reply {
	struct list_head       list;
	oncrpc_callback_t      callback;
	uint32_t               rpc_id;
};

struct oncrpc_pending_call {
	struct list_head       list;
	struct io_buffer       *data;
};

extern struct oncrpc_cred oncrpc_auth_none;

void oncrpc_init_cred_sys ( struct oncrpc_cred_sys *auth_sys, uint32_t uid,
                            uint32_t gid, char *hostname );
void oncrpc_init_session ( struct oncrpc_session *session,
                           struct oncrpc_cred *credential,
                           struct oncrpc_cred *verifier, uint32_t prog_name,
                           uint32_t prog_vers );
int oncrpc_connect_named (struct oncrpc_session *session, uint16_t port,
                          const char *name );
void oncrpc_close_session ( struct oncrpc_session *session, int rc );

int oncrpc_call_iob ( struct oncrpc_session *session, uint32_t proc_name,
                      struct io_buffer *io_buf, oncrpc_callback_t cb );

#endif /* _IPXE_ONCRPC_H */
