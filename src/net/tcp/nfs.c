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

/** @file
 *
 * Network File System protocol
 *
 */

FEATURE ( FEATURE_PROTOCOL, "NFS", DHCP_EB_FEATURE_NFS, 1 );

/**
 * A NFS request
 *
 */
struct nfs_request {
	/** Reference counter */
	struct refcnt refcnt;
	/** Data transfer interface */
	struct interface xfer;

	/** URI being fetched */
	struct uri *uri;
	/** NFS channel interface */
	struct interface channel;
};

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
	if ( ! rc )
		rc = -ECONNRESET;
	DBGC ( nfs, "NFS %p completed (%s)\n", nfs, strerror ( rc ) );

	/* Close transfer interfaces */
	intf_shutdown ( &nfs->channel, rc );
	intf_shutdown ( &nfs->xfer, rc );
}

/*****************************************************************************
 *
 * NFS channel
 *
 */

/** NFS channel interface operations */
static struct interface_operation nfs_channel_operations[] = {
	INTF_OP ( intf_close, struct nfs_request *, nfs_done ),
};

/** NFS channel interface descriptor */
static struct interface_descriptor nfs_channel_desc =
	INTF_DESC ( struct nfs_request, channel, nfs_channel_operations );

/*****************************************************************************
 *
 * Data transfer interface
 *
 */

 /** NFS data transfer interface operations */
static struct interface_operation nfs_xfer_operations[] = {
	INTF_OP ( intf_close, struct nfs_request *, nfs_done ),
};

static struct interface_descriptor nfs_xfer_desc =
	INTF_DESC_PASSTHRU ( struct nfs_request, xfer, nfs_xfer_operations,
	                     channel );

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
	struct sockaddr_tcpip server;
	int rc;

	/* Sanity checks */
	if ( ! uri->path )
		return -EINVAL;
	if ( ! uri->host )
		return -EINVAL;

	nfs = zalloc ( sizeof ( *nfs ) );
	if ( ! nfs )
		return -ENOMEM;
	ref_init ( &nfs->refcnt, nfs_free );
	intf_init ( &nfs->xfer, &nfs_xfer_desc, &nfs->refcnt );
	intf_init ( &nfs->channel, &nfs_channel_desc, &nfs->refcnt );
	nfs->uri = uri_get ( uri );

	DBGC ( nfs, "NFS %p fetching %s\n", nfs, nfs->uri->path );

	/* Open control connection */
	memset ( &server, 0, sizeof ( server ) );
	server.st_port = htons ( uri_port ( uri, NFS_PORT ) );
	if ( ( rc = xfer_open_named_socket ( &nfs->channel, SOCK_STREAM,
	                                     ( struct sockaddr * ) &server,
	                                     uri->host, NULL ) ) != 0 )
		goto err;

	/* Attach to parent interface, mortalise self, and return */
	intf_plug_plug ( &nfs->xfer, xfer );
	ref_put ( &nfs->refcnt );
	return 0;

err:
	DBGC ( nfs, "NFS %p could not create request: %s\n",
	       nfs, strerror ( rc ) );
	nfs_done ( nfs, rc );
	ref_put ( &nfs->refcnt );
	return rc;
}

/** NFS URI opener */
struct uri_opener nfs_uri_opener __uri_opener = {
	.scheme	= "nfs",
	.open	= nfs_open,
};
