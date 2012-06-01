#ifndef _IPXE_ONCRPC_H
#define _IPXE_ONCRPC_H

#include <stdint.h>
#include <ipxe/interface.h>
#include <ipxe/iobuf.h>

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
#define ONCRPC_AUTH_SHORT 2

struct oncrpc_cred {
	uint32_t               flavor;
	uint32_t               length;
} __packed;

struct oncrpc_cred_sys {
	struct oncrpc_cred     credential;
	uint32_t               stamp;
	char                   *hostname;
	uint32_t               uid;
	uint32_t               gid;
	uint32_t               aux_gid[10];
	char                   *short_id;
};

struct oncrpc_session {
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
};

int oncrpc_call_iob ( struct interface *intf, struct oncrpc_session *session,
                      uint32_t proc_name, struct io_buffer *io_buf );

#endif /* _IPXE_ONCRPC_H */
