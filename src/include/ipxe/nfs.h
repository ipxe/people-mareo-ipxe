#ifndef _IPXE_NFS_H
#define _IPXE_NFS_H

#include <stdint.h>
#include <ipxe/oncrpc.h>

/** @file
 *
 * Network File System protocol.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

/** NFS protocol number. */
#define ONCRPC_NFS 100003

/** NFS protocol version. */
#define NFS_VERS 3

#define NFS3_OK             0
#define NFS3ERR_PERM        1
#define NFS3ERR_NOENT       2
#define NFS3ERR_IO          5
#define NFS3ERR_NXIO        6
#define NFS3ERR_ACCES       13
#define NFS3ERR_EXIST       17
#define NFS3ERR_XDEV        18
#define NFS3ERR_NODEV       19
#define NFS3ERR_NOTDIR      20
#define NFS3ERR_ISDIR       21
#define NFS3ERR_INVAL       22
#define NFS3ERR_FBIG        27
#define NFS3ERR_NOSPC       28
#define NFS3ERR_ROFS        30
#define NFS3ERR_MLINK       31
#define NFS3ERR_NAMETOOLONG 63
#define NFS3ERR_NOTEMPTY    66
#define NFS3ERR_DQUOT       69
#define NFS3ERR_STALE       70
#define NFS3ERR_REMOTE      71
#define NFS3ERR_BADHANDLE   10001
#define NFS3ERR_NOT_SYNC    10002
#define NFS3ERR_BAD_COOKIE  10003
#define NFS3ERR_NOTSUPP     10004
#define NFS3ERR_TOOSMALL    10005
#define NFS3ERR_SERVERFAULT 10006
#define NFS3ERR_BADTYPE     10007
#define NFS3ERR_JUKEBOX     10008

/**
 * A NFS File Handle.
 *
 */
struct nfs_fh {
	uint8_t               fh[64];
	size_t                size;
};

struct nfs_lookup_reply {
	uint32_t             status;
	struct nfs_fh        fh;
};

struct nfs_read_reply {
	uint32_t             status;
	uint64_t             filesize;
	uint32_t             count;
	uint32_t             eof;
	uint32_t             data_len;
	void                 *data;
};

size_t nfs_iob_get_fh ( struct io_buffer *io_buf, struct nfs_fh *fh );
size_t nfs_iob_add_fh ( struct io_buffer *io_buf, const struct nfs_fh *fh );

void nfs_init_session ( struct oncrpc_session *session );

int nfs_lookup ( struct interface *intf, struct oncrpc_session *session,
                 const struct nfs_fh *fh, const char *filename );
int nfs_read ( struct interface *intf, struct oncrpc_session *session,
               const struct nfs_fh *fh, uint64_t offset, uint32_t count );

int nfs_get_lookup_reply ( struct nfs_lookup_reply *lookup_reply,
                           struct oncrpc_reply *reply );
int nfs_get_read_reply ( struct nfs_read_reply *read_reply,
                         struct oncrpc_reply *reply );

#endif /* _IPXE_NFS_H */
