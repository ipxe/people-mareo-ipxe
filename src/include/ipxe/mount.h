#ifndef _IPXE_MOUNT_H
#define _IPXE_MOUNT_H

#include <ipxe/nfs.h>

/** @file
 *
 * NFS MOUNT protocol.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** NFS MOUNT protocol number. */
#define ONCRPC_MOUNT 100005
/** NFS MOUNT protocol version. */
#define MOUNT_VERS   3


/** No error. */
#define MNT3_OK                 0
/** Not owner. */
#define MNT3ERR_PERM            1
/** No such file or directory. */
#define MNT3ERR_NOENT           2
/** I/O error. */
#define MNT3ERR_IO              5
/** Permission denied. */
#define MNT3ERR_ACCES           13
/** Not a directory. */
#define MNT3ERR_NOTDIR          20
/** Invalid argument. */
#define MNT3ERR_INVAL           22
/** Filename too long. */
#define MNT3ERR_NAMETOOLONG     63
/** Operation not supported. */
#define MNT3ERR_NOTSUPP         10004
/** A failure on the server. */
#define MNT3ERR_SERVERFAULT     10006

struct mount_mnt_reply {
	uint32_t        status;
	struct nfs_fh   fh;
};

int mount_init_session ( struct oncrpc_session *session, uint16_t port,
                       const char *name );

int mount_mnt ( struct oncrpc_session *session, const char *mountpoint,
                oncrpc_callback_t cb);
int mount_umnt ( struct oncrpc_session *session, const char *mountpoint,
                oncrpc_callback_t cb);

int mount_get_mnt_reply ( struct mount_mnt_reply *mnt_reply,
                          struct oncrpc_reply *reply );

#endif /* _IPXE_MOUNT_H */
