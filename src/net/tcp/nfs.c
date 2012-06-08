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
#include <ipxe/socket.h>
#include <ipxe/tcpip.h>
#include <ipxe/in.h>
#include <ipxe/iobuf.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/uri.h>
#include <ipxe/features.h>
#include <ipxe/nfs.h>
#include <ipxe/portmap.h>

/** @file
 *
 * Network File System protocol
 *
 */

FEATURE ( FEATURE_PROTOCOL, "NFS", DHCP_EB_FEATURE_NFS, 1 );

#define MOUNT_MNT 1

/**
 * A NFS File handle.
 *
 */
struct nfs_fh {
	uint8_t               fh[64];
	size_t                size;
};

/**
 * A NFS request
 *
 */
struct nfs_request {
	/** Reference counter */
	struct refcnt           refcnt;
	/** Data transfer interface */
	struct interface        xfer;

	struct oncrpc_session   pm_session;
	struct oncrpc_session   nfs_session;
	uint16_t                nfs_port;

	struct nfs_fh           root_fh;

	/** URI being fetched */
	struct uri              *uri;
};

size_t nfs_iob_get_fh ( struct io_buffer *io_buf, struct nfs_fh *fh ) {
	fh->size = oncrpc_iob_get_int ( io_buf );
	oncrpc_iob_get_val ( io_buf, &fh->fh, fh->size );
	iob_pull ( io_buf, (4 - (fh->size % 4)) % 4 );

	return ( fh->size + (4 - (fh->size % 4)) % 4 );
}

/**
 * Free NFS request
 *
 * @v refcnt		Reference counter
 */
static void nfs_free ( struct refcnt *refcnt ) {
	struct nfs_request *nfs =
		container_of ( refcnt, struct nfs_request, refcnt );

	DBGC ( nfs, "NFS %p freed\n", nfs );

	uri_put ( nfs->uri );
	free ( nfs );
}

/**
 * Mark NFS operation as complete
 *
 * @v nfs		NFS request
 * @v rc		Return status code
 */
static void nfs_done ( struct nfs_request *nfs, int rc ) {
	DBGC ( nfs, "NFS %p completed (%s)\n", nfs, strerror ( rc ) );

	intf_shutdown ( &nfs->xfer, rc );
	portmap_close_session ( &nfs->pm_session, rc );
	oncrpc_close_session ( &nfs->nfs_session, rc );
}

static int getport_cb ( struct oncrpc_session *session,
                        struct oncrpc_reply *reply) {
	struct nfs_request *nfs =
		container_of ( session, struct nfs_request, pm_session );

	nfs->nfs_port = oncrpc_iob_get_int ( reply->data );

	return 0;
}

static int mount_cb ( struct oncrpc_session *session,
                      struct oncrpc_reply *reply) {
	int rc = 0;
	struct nfs_request *nfs =
		container_of ( session, struct nfs_request, pm_session );

	// Ignore port information sent by PORTMAP.
	(void) oncrpc_iob_get_int ( reply->data );


	nfs_iob_get_fh ( reply->data, &nfs->root_fh );

	if ( ( nfs->nfs_port = uri_port ( nfs->uri, 0 ) ) == 0 ) {
		rc = portmap_getport ( &nfs->pm_session, ONCRPC_NFS, NFS_VERS,
		                       PORTMAP_PROT_TCP, &getport_cb );
	}

	if ( rc != 0 )
		nfs_done ( nfs, rc );


	return rc;
}

static int nfs_mount ( struct nfs_request *nfs, const char *mountpoint )
{
	int rc;
	struct io_buffer *io_buf;


	if ( ! ( io_buf = alloc_iob ( strlen ( mountpoint ) + 3 ) ) )
		return -ENOBUFS;

	oncrpc_iob_add_string ( io_buf, mountpoint );

	rc = portmap_callit ( &nfs->pm_session, ONCRPC_MOUNT, MOUNT_VERS,
	                      MOUNT_MNT, io_buf, &mount_cb );

	if ( rc != 0 )
		nfs_done ( nfs, rc );

	return rc;
}

static struct interface_operation nfs_xfer_operations[] = {
	INTF_OP ( intf_close, struct nfs_request *, nfs_done ),
};

/** NFS data transfer interface descriptor */
static struct interface_descriptor nfs_xfer_desc =
	INTF_DESC ( struct nfs_request, xfer, nfs_xfer_operations );

/*****************************************************************************
 *
 * URI opener
 *
 */

/**
 * Initiate a NFS connection
 *
 * @v xfer		Data transfer interface
 * @v uri		Uniform Resource Identifier
 * @ret rc		Return status code
 */
static int nfs_open ( struct interface *xfer, struct uri *uri ) {
	struct nfs_request *nfs;

	/* Sanity checks */
	if ( ! uri->path || ! uri->host )
		return -EINVAL;

	nfs = zalloc ( sizeof ( *nfs ) );
	if ( ! nfs )
		return -ENOMEM;

	ref_init ( &nfs->refcnt, nfs_free );
	intf_init ( &nfs->xfer, &nfs_xfer_desc, &nfs->refcnt );
	nfs->uri = uri_get ( uri );

	portmap_init_session ( &nfs->pm_session );

	/* Attach to parent interface, mortalise self, and return */
	intf_plug_plug ( &nfs->xfer, xfer );
	ref_put ( &nfs->refcnt );

	nfs_mount ( nfs, uri->path );

	return 0;
}

/** NFS URI opener */
struct uri_opener nfs_uri_opener __uri_opener = {
	.scheme	= "nfs",
	.open	= nfs_open,
};
