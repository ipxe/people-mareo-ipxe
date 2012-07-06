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

struct portmap_getport_reply {
	uint32_t        port;
};

int portmap_init_session ( struct oncrpc_session *session, uint16_t port,
                           const char *name);
int portmap_getport ( struct oncrpc_session *session, uint32_t prog,
                      uint32_t vers, uint32_t prot, oncrpc_callback_t cb );
int portmap_get_getport_reply ( struct portmap_getport_reply *getport_reply,
                                 struct oncrpc_reply *reply );


#endif /* _IPXE_PORTMAP_H */
