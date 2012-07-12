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
#include <ipxe/nfs.h>
#include <ipxe/oncrpc.h>
#include <ipxe/oncrpc_iob.h>
#include <ipxe/portmap.h>
#include <ipxe/mount.h>
#include <ipxe/settings.h>

/** @file
 *
 * Network File System protocol
 *
 */

#define NFS_LOOKUP      3
#define NFS_READ        6

size_t nfs_iob_get_fh ( struct io_buffer *io_buf, struct nfs_fh *fh ) {
	fh->size = oncrpc_iob_get_int ( io_buf );
	oncrpc_iob_get_val ( io_buf, &fh->fh, fh->size );

	return fh->size + sizeof ( uint32_t );
}

size_t nfs_iob_add_fh ( struct io_buffer *io_buf, const struct nfs_fh *fh ) {
	size_t s = 0;
	s += oncrpc_iob_add_int ( io_buf, fh->size );
	s += oncrpc_iob_add_val ( io_buf, &fh->fh, fh->size );

	return s;
}

void nfs_init_session ( struct oncrpc_session *session ) {
	oncrpc_init_session ( session, &oncrpc_auth_none,
                              &oncrpc_auth_none, ONCRPC_NFS,
                              NFS_VERS );
}

int nfs_lookup ( struct interface *intf, struct oncrpc_session *session,
                 const struct nfs_fh *fh, const char *filename ) {
	int              rc;
	struct io_buffer *io_buf;

	io_buf = oncrpc_alloc_iob ( session, oncrpc_strlen ( filename ) +
	                                     fh->size + sizeof ( uint32_t ) );
	if ( ! io_buf )
		return -ENOBUFS;

	nfs_iob_add_fh ( io_buf, fh );
	oncrpc_iob_add_string ( io_buf, filename );

	rc =  oncrpc_call_iob ( intf, session, NFS_LOOKUP, io_buf );
	if ( rc != 0 )
		free_iob ( io_buf );

	return rc;
}

int nfs_read ( struct interface *intf, struct oncrpc_session *session,
               const struct nfs_fh *fh, uint64_t offset, uint32_t count ) {
	int              rc;
	struct io_buffer *io_buf;

	io_buf = oncrpc_alloc_iob ( session,  fh->size + sizeof ( uint64_t ) +
	                                      sizeof ( uint64_t ) );
	if ( ! io_buf )
		return -ENOBUFS;

	nfs_iob_add_fh ( io_buf, fh );
	oncrpc_iob_add_int64 ( io_buf, offset );
	oncrpc_iob_add_int ( io_buf, count );

	rc = oncrpc_call_iob ( intf, session, NFS_READ, io_buf );
	if ( rc != 0 )
		free_iob ( io_buf );

	return rc;
}

int nfs_get_lookup_reply ( struct nfs_lookup_reply *lookup_reply,
                           struct oncrpc_reply *reply ) {
	if ( ! lookup_reply || ! reply )
		return -EINVAL;

	lookup_reply->status = oncrpc_iob_get_int ( reply->data );
	switch ( lookup_reply->status )
	{
	case NFS3_OK:
		break;
	case NFS3ERR_PERM:
		return -EPERM;
	case NFS3ERR_NOENT:
		return -ENOENT;
	case NFS3ERR_IO:
		return -EIO;
	case NFS3ERR_ACCES:
		return -EACCES;
	case NFS3ERR_NOTDIR:
		return -ENOTDIR;
	case NFS3ERR_NAMETOOLONG:
		return -ENAMETOOLONG;
	case NFS3ERR_STALE:
	case NFS3ERR_BADHANDLE:
	case NFS3ERR_SERVERFAULT:
	default:
		return -EPROTO;
	}

	nfs_iob_get_fh ( reply->data, &lookup_reply->fh );

	return 0;
}

int nfs_get_read_reply ( struct nfs_read_reply *read_reply,
                         struct oncrpc_reply *reply ) {
	if ( ! read_reply || ! reply )
		return -EINVAL;

	read_reply->status = oncrpc_iob_get_int ( reply->data );
	switch ( read_reply->status )
	{
	case NFS3_OK:
		 break;
	case NFS3ERR_PERM:
		return -EPERM;
	case NFS3ERR_NOENT:
		return -ENOENT;
	case NFS3ERR_IO:
		return -EIO;
	case NFS3ERR_NXIO:
		return -ENXIO;
	case NFS3ERR_ACCES:
		return -EACCES;
	case NFS3ERR_INVAL:
		return -EINVAL;
	case NFS3ERR_STALE:
	case NFS3ERR_BADHANDLE:
	case NFS3ERR_SERVERFAULT:
	default:
		return -EPROTO;
	}

	if ( oncrpc_iob_get_int ( reply->data ) == 1 )
	{
		iob_pull ( reply->data, 5 * sizeof ( uint32_t ));
		read_reply->filesize = oncrpc_iob_get_int64 ( reply->data );
		iob_pull ( reply->data, 7 * sizeof ( uint64_t ));
	}

	read_reply->count    = oncrpc_iob_get_int ( reply->data );
	read_reply->eof      = oncrpc_iob_get_int ( reply->data );
	read_reply->data_len = oncrpc_iob_get_int ( reply->data );
	read_reply->data     = reply->data->data;

	if ( read_reply->count != read_reply->data_len )
		return -EPROTO;

	return 0;
}
