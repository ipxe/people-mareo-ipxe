#ifndef _IPXE_ONCRPC_IOB_H
#define _IPXE_ONCRPC_IOB_H

#include <stdint.h>
#include <ipxe/iobuf.h>
#include <ipxe/refcnt.h>
#include <ipxe/oncrpc.h>

/** @file
 *
 * SUN ONC RPC protocol.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#define oncrpc_iob_get_int( buf ) \
( { \
	uint32_t _val; \
	oncrpc_iob_get_val ( (buf), &_val, sizeof ( _val ) ); \
	ntohl ( _val ); \
} )

#define oncrpc_iob_get_int64( buf ) \
( { \
	uint64_t _val; \
	oncrpc_iob_get_val ( (buf), &_val, sizeof ( _val ) ); \
	ntohll ( _val ); \
} )

struct io_buffer * oncrpc_alloc_iob ( const struct oncrpc_session *session,
                                      size_t len );

size_t oncrpc_strlen ( const char *str );
size_t oncrpc_iob_add_string ( struct io_buffer *io_buf, const char *val );

size_t oncrpc_iob_add_intarray ( struct io_buffer *io_buf, size_t size,
                                 const uint32_t *array );
size_t oncrpc_iob_add_cred ( struct io_buffer *io_buf,
                             struct oncrpc_cred *cred );
size_t oncrpc_iob_get_cred ( struct io_buffer *io_buf,
                             struct oncrpc_cred *cred );

static inline size_t oncrpc_iob_add_int ( struct io_buffer *io_buf,
                                          uint32_t val ) {
	* ( uint32_t * ) iob_put ( io_buf, sizeof ( val ) ) = htonl ( val );
	return ( sizeof ( val) );
}

static inline size_t oncrpc_iob_add_int64 ( struct io_buffer *io_buf,
                                            uint64_t val ) {
	* ( uint64_t * ) iob_put ( io_buf, sizeof ( val ) ) = htonll ( val );
	return ( sizeof ( val) );
}

static inline size_t oncrpc_iob_get_val ( struct io_buffer *io_buf,
                                          void *val, size_t size ) {
	memcpy (val, io_buf->data, size );
	iob_pull ( io_buf, size );
	return size;
}

static inline size_t oncrpc_iob_add_val ( struct io_buffer *io_buf,
                                          void *val, size_t size ) {
	memcpy ( iob_put ( io_buf, size ), val, size );
	return size;
}


#endif /* _IPXE_ONCRPC_IOB_H */
