#ifndef _IPXE_PORTMAPC_H
#define _IPXE_PORTMAP_H

#include <stdint.h>
#include <ipxe/interface.h>
#include <ipxe/oncrpc.h>

/** @file
 *
 * SUN ONC RPC protocol.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** PORTMAP protocol number. */
#define ONCRPC_PORTMAP 100000

/** PORTMAP version. */
#define PORTMAP_VERS 2

/** PORTMAP GETPORT procedure. */
#define PORTMAP_GETPORT 3

void portmap_init_session ( struct oncrpc_session *session );
int portmap_getport ( struct interface *intf, struct oncrpc_session *session,
                      uint32_t prog, uint32_t vers, uint32_t prot );


#endif /* _IPXE_ONCRPC_H */
