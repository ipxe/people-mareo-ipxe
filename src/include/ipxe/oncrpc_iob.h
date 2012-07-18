#ifndef _IPXE_ONCRPC_IOB_H
#define _IPXE_ONCRPC_IOB_H

#include <stdint.h>
#include <string.h>
#include <ipxe/iobuf.h>
#include <ipxe/refcnt.h>
#include <ipxe/oncrpc.h>

/** @file
 *
 * SUN ONC RPC protocol.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** Enusure that size is a multiple of four */
#define oncrpc_align( size ) ( ( ( ( (size) - 1 ) >> 2 ) + 1 ) << 2 )

#define oncrpc_iob_get_int( buf ) \
( { \
	uint32_t *_val; \
	_val = (buf)->data; \
	iob_pull ( (buf), sizeof ( uint32_t ) ); \
	ntohl ( *_val ); \
} )

#define oncrpc_iob_get_int64( buf ) \
( { \
	uint64_t *_val; \
	_val = (buf)->data; \
	iob_pull ( (buf), sizeof ( uint64_t ) ); \
	ntohll ( *_val ); \
} )

/**
 * Calculate the length of a string, including padding bytes.
 *
 * @v str               String
 * @ret size            Length of the string
 */
#define oncrpc_strlen( str ) ( oncrpc_align ( strlen ( str ) ) + \
                               sizeof ( uint32_t ) )

struct io_buffer * oncrpc_alloc_iob ( const struct oncrpc_session *session,
                                      size_t size );

size_t oncrpc_iob_add_string ( struct io_buffer *io_buf, const char *val );
size_t oncrpc_iob_add_intarray ( struct io_buffer *io_buf, size_t length,
                                 const uint32_t *array );
size_t oncrpc_iob_add_cred ( struct io_buffer *io_buf,
                             struct oncrpc_cred *cred );
size_t oncrpc_iob_get_cred ( struct io_buffer *io_buf,
                             struct oncrpc_cred *cred );

/**
 * Add a 32 bits integer to the end of an I/O buffer
 *
 * @v io_buf            I/O buffer
 * @v val               Integer
 * @ret size            Size of the data written
 */
static inline size_t oncrpc_iob_add_int ( struct io_buffer *io_buf,
                                          uint32_t val ) {
	* ( uint32_t * ) iob_put ( io_buf, sizeof ( val ) ) = htonl ( val );
	return ( sizeof ( val) );
}

/**
 * Add a 64 bits integer to the end of an I/O buffer
 *
 * @v io_buf            I/O buffer
 * @v val               Integer
 * @ret size            Size of the data written
 */
static inline size_t oncrpc_iob_add_int64 ( struct io_buffer *io_buf,
                                            uint64_t val ) {
	* ( uint64_t * ) iob_put ( io_buf, sizeof ( val ) ) = htonll ( val );
	return ( sizeof ( val) );
}

#endif /* _IPXE_ONCRPC_IOB_H */
