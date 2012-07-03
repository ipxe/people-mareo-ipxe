/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <byteswap.h>
#include <ipxe/time.h>
#include <ipxe/iobuf.h>
#include <ipxe/open.h>
#include <ipxe/features.h>
#include <ipxe/oncrpc.h>
#include <ipxe/oncrpc_iob.h>
#include <ipxe/nfs.h>
#include <ipxe/mount.h>

/** @file
 *
 * NFS MOUNT protocol
 *
 */

#define MOUNT_MNT 1
#define MOUNT_UMNT 3

int mount_get_mnt_reply ( struct mount_mnt_reply *mnt_reply,
                          struct oncrpc_reply *reply ) {
	if (  ! mnt_reply || ! reply )
		return -EINVAL;

	mnt_reply->status = oncrpc_iob_get_int ( reply->data );

	switch ( mnt_reply->status )
	{
	case MNT3_OK:
		nfs_iob_get_fh ( reply->data, &mnt_reply->fh );
		return 0;
	case MNT3ERR_NOENT:
		return -ENOENT;
	case MNT3ERR_IO:
		return -EIO;
	case MNT3ERR_ACCES:
		return -EACCES;
	case MNT3ERR_NOTDIR:
		return -ENOTDIR;
	case MNT3ERR_NAMETOOLONG:
		return -ENAMETOOLONG;
	default:
		return -ENOTSUP;
	}
}

int mount_mnt ( struct oncrpc_session *session, const char *mountpoint,
                oncrpc_callback_t cb) {
	struct io_buffer *io_buf;

	io_buf = oncrpc_alloc_iob ( session, oncrpc_strlen ( mountpoint ) );
	if ( ! io_buf )
		return -ENOBUFS;

	oncrpc_iob_add_string ( io_buf, mountpoint );
	return  oncrpc_call_iob ( session, MOUNT_MNT, io_buf, cb );
}

int mount_umnt ( struct oncrpc_session *session, const char *mountpoint,
                oncrpc_callback_t cb) {
	struct io_buffer *io_buf;

	io_buf = oncrpc_alloc_iob ( session, oncrpc_strlen ( mountpoint ) );
	if ( ! io_buf )
		return -ENOBUFS;

	oncrpc_iob_add_string ( io_buf, mountpoint );
	return oncrpc_call_iob ( session, MOUNT_UMNT, io_buf, cb );
}
