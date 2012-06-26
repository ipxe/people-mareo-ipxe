#ifndef _IPXE_MOUNT_H
#define _IPXE_MOUNT_H

/** @file
 *
 * NFS MOUNT protocol.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#define ONCRPC_MOUNT 100005
#define MOUNT_VERS 3


int mount_mnt ( struct oncrpc_session *session, const char *mountpoint,
                oncrpc_callback_t cb);
int mount_umnt ( struct oncrpc_session *session, const char *mountpoint,
                oncrpc_callback_t cb);

#endif /* _IPXE_MOUNT_H */
