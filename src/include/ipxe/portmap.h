#ifndef _IPXE_PORTMAP_H
#define _IPXE_PORTMAP_H

#include <stdint.h>
#include <ipxe/oncrpc.h>

/** @file
 *
 * SUN ONC RPC protocol.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** PORTMAP default port. */
#define PORTMAP_PORT 111

/** PORTMAP protocol number. */
#define ONCRPC_PORTMAP 100000

/** PORTMAP version. */
#define PORTMAP_VERS 2

/** PORTMAP GETPORT procedure. */
#define PORTMAP_GETPORT 3

/** PORTMAP CALLIR procedure. */
#define PORTMAP_CALLIT 5

#define PORTMAP_PROT_TCP 6
#define PORTMAP_PROT_UDP 17

int portmap_init_session ( struct oncrpc_session *session, uint16_t port,
                            const char *name);
void portmap_close_session ( struct oncrpc_session *session, int rc );
int portmap_getport ( struct oncrpc_session *session, uint32_t prog,
                      uint32_t vers, uint32_t prot, oncrpc_callback_t cb );
int portmap_callit ( struct oncrpc_session *session, uint32_t prog,
                     uint32_t vers, uint32_t proc, struct io_buffer *io_buf,
                     oncrpc_callback_t cb );

#endif /* _IPXE_PORTMAP_H */
