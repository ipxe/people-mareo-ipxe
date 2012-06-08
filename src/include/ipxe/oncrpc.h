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

#define oncrpc_iob_add_int( buf, val ) oncrpc_iob_add_val ( (buf), \
                                                            htonl ( val ) )

#define oncrpc_iob_get_int( buf ) \
( { \
	uint32_t val; \
	oncrpc_iob_get_val ( (buf), &val, sizeof ( val ) ); \
	ntohl ( val ); \
} )


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
	uint32_t               aux_gid_len;
	uint32_t               aux_gid[16];
};

struct oncrpc_session {
	struct interface        intf;
	struct oncrpc_pending   *pending;
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
	struct io_buffer        *data;
};

typedef int ( *oncrpc_callback_t ) ( struct oncrpc_session *session,
                                     struct oncrpc_reply *reply );
struct oncrpc_pending {
	struct list_head       list;
	oncrpc_callback_t      callback;
	uint32_t               rpc_id;
};

static inline size_t oncrpc_iob_add_val ( struct io_buffer *io_buf,
                                          uint32_t val ) {
	memcpy ( iob_put ( io_buf, sizeof ( val ) ), &val, sizeof ( val ) );
	return ( sizeof ( val) );
}

static inline size_t oncrpc_iob_get_val ( struct io_buffer *io_buf,
                                          void *val, size_t size ) {
	memcpy ( iob_pull ( io_buf, size ), val, size );
	return size;
}

void oncrpc_init_session ( struct oncrpc_session *session,
                           struct oncrpc_cred *credential,
                           struct oncrpc_cred *verifier, uint32_t prog_name,
                           uint32_t prog_vers );
void oncrpc_close_session ( struct oncrpc_session *session, int rc );

size_t oncrpc_iob_add_string ( struct io_buffer *io_buf, const char *val ) ;
size_t oncrpc_iob_add_intarray ( struct io_buffer *io_buf, size_t size,
                               const uint32_t *array );
int oncrpc_call_iob ( struct oncrpc_session *session, uint32_t proc_name,
                      struct io_buffer *io_buf, oncrpc_callback_t cb );

#endif /* _IPXE_ONCRPC_H */
