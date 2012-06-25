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
#include <ipxe/socket.h>
#include <ipxe/tcpip.h>
#include <ipxe/in.h>
#include <ipxe/iobuf.h>
#include <ipxe/xfer.h>
#include <ipxe/open.h>
#include <ipxe/uri.h>
#include <ipxe/features.h>
#include <ipxe/nfs.h>
#include <ipxe/oncrpc.h>
#include <ipxe/portmap.h>
#include <ipxe/settings.h>

/** @file
 *
 * Network File System protocol
 *
 */

FEATURE ( FEATURE_PROTOCOL, "NFS", DHCP_EB_FEATURE_NFS, 1 );

#define MOUNT_MNT 1
#define MOUNT_UMNT 3

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
	struct oncrpc_session   mount_session;
	struct oncrpc_session   nfs_session;
	uint16_t                mount_port;
	uint16_t                nfs_port;

	char *                  mountpoint;
	char *                  filename;
	struct nfs_fh           root_fh;

	/** URI being fetched */
	struct uri              *uri;
};

static int nfs_mount ( struct nfs_request *nfs, const char *mountpoint );
static int nfs_umount ( struct nfs_request *nfs, const char *mountpoint );
static int getport_nfs_cb ( struct oncrpc_session *session,
                            struct oncrpc_reply *reply);

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

	free ( nfs->mountpoint );
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

	// portmap_close_session ( &nfs->pm_session, rc );
	// oncrpc_close_session ( &nfs->nfs_session, rc );
	// oncrpc_close_session ( &nfs->mount_session, rc );
	intf_shutdown ( &nfs->xfer, rc );
}

static int getport_mount_cb ( struct oncrpc_session *session,
                              struct oncrpc_reply *reply) {
	struct nfs_request *nfs =
		container_of ( session, struct nfs_request, pm_session );
	int rc = 0;

	if ( reply->accept_state != 0 )
	{
		nfs_done ( nfs, -ENOTSUP );
		return -ENOTSUP;
	}

	nfs->mount_port = oncrpc_iob_get_int ( reply->data );
	DBGC ( nfs, "NFS %p Got mount port (%d)\n", nfs, nfs->mount_port );

	rc = oncrpc_connect_named ( &nfs->mount_session, nfs->mount_port,
	                            nfs->uri->host );
	if ( rc != 0 ) {
		nfs_done ( nfs, rc );
		return rc;
	}

	rc = portmap_getport ( &nfs->pm_session, ONCRPC_NFS, NFS_VERS,
	                       PORTMAP_PROT_TCP, &getport_nfs_cb );
	if ( rc != 0 ) {
		nfs_done ( nfs, rc );
		return rc;
	}

	rc = nfs_mount ( nfs, nfs->mountpoint );
	if ( rc != 0 ) {
		nfs_done ( nfs, rc );
		return rc;
	}

	return 0;
}

static int getport_nfs_cb ( struct oncrpc_session *session,
                            struct oncrpc_reply *reply) {
	struct nfs_request *nfs =
		container_of ( session, struct nfs_request, pm_session );
	int rc = 0;

	if ( reply->accept_state != 0 )
	{
		DBGC ( nfs, "NFS %p GETPORT failed (%d)\n", nfs,
		       reply->accept_state );
		nfs_done ( nfs, -ENOTSUP );
		return -ENOTSUP;
	}

	nfs->nfs_port = oncrpc_iob_get_int ( reply->data );
	DBGC ( nfs, "NFS %p Got NFS port (%d)\n", nfs, nfs->nfs_port );

	rc = oncrpc_connect_named ( &nfs->nfs_session, nfs->nfs_port,
	                            nfs->uri->host );

	if ( rc != 0 )
		nfs_done ( nfs, rc );

	return rc;
}

static int mount_cb ( struct oncrpc_session *session,
                      struct oncrpc_reply *reply) {
	int rc;
	struct nfs_request *nfs =
		container_of ( session, struct nfs_request, mount_session );

	switch ( oncrpc_iob_get_int ( reply->data ) )
	{
	case 0:
		rc = 0;
		break;
	case 1:
		rc = -EPERM;
		break;
	case 2:
		rc = -ENOENT;
		break;
	case 13:
		rc = -EACCES;
		break;
	default:
		rc = -ENOTSUP;
		break;
	}

	if ( rc != 0 ) {
		nfs_done ( nfs, rc );
		return rc;
	}

        nfs_iob_get_fh ( reply->data, &nfs->root_fh );

	if ( rc != 0 )
		nfs_done ( nfs, rc );
	else
		nfs_umount ( nfs, nfs->mountpoint );

	return rc;
}

static int umnt_cb ( struct oncrpc_session *session,
                    struct oncrpc_reply *reply __unused) {
	struct nfs_request *nfs =
		container_of ( session, struct nfs_request, mount_session );

	nfs_done ( nfs, 0 );
	return 0;
}

static int nfs_mount ( struct nfs_request *nfs, const char *mountpoint )
{
	struct io_buffer *io_buf;

	if ( ! ( io_buf = oncrpc_alloc_iob ( &nfs->mount_session,
	                                     oncrpc_strlen ( mountpoint ) ) ) )
		return -ENOBUFS;

	oncrpc_iob_add_string ( io_buf, mountpoint );
	return  oncrpc_call_iob ( &nfs->mount_session, MOUNT_MNT, io_buf,
	                          &mount_cb );
}

static int nfs_umount ( struct nfs_request *nfs, const char *mountpoint )
{
	struct io_buffer *io_buf;


	if ( ! ( io_buf = oncrpc_alloc_iob ( &nfs->mount_session,
	                                     oncrpc_strlen ( mountpoint ) ) ) )
		return -ENOBUFS;

	oncrpc_iob_add_string ( io_buf, mountpoint );
	return oncrpc_call_iob ( &nfs->mount_session, MOUNT_UMNT, io_buf,
	                         &umnt_cb );
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
	struct nfs_request       *nfs;
	struct oncrpc_cred_sys   *auth_sys;

	/* Sanity checks */
	if ( ! uri->path || ! uri->host )
		return -EINVAL;

	nfs = zalloc ( sizeof ( *nfs ) );
	if ( ! nfs )
		return -ENOMEM;

	auth_sys = malloc ( sizeof ( *auth_sys) );
	if ( ! auth_sys )
	{
		free ( nfs );
		return -ENOMEM;
	}

	fetch_string_setting_copy ( NULL, &hostname_setting,
	                            &auth_sys->hostname );
	if ( auth_sys->hostname == NULL )
		auth_sys->hostname = strdup ( "iPXE1" );

	if ( auth_sys->hostname == NULL )
	{
		free ( nfs );
		free ( auth_sys );
		return -ENOMEM;
	}

	oncrpc_init_cred_sys ( auth_sys, 0, 0, auth_sys->hostname );

	nfs->mountpoint = strdup ( uri->path );
	if ( ! nfs->mountpoint )
	{
		free ( nfs );
		free ( auth_sys->hostname );
		return -ENOMEM;
	}

	ref_init ( &nfs->refcnt, nfs_free );
	intf_init ( &nfs->xfer, &nfs_xfer_desc, &nfs->refcnt );
	nfs->uri = uri_get ( uri );

	portmap_init_session ( &nfs->pm_session, uri_port ( uri, 0 ),
	                       uri->host );
	oncrpc_init_session ( &nfs->mount_session, &auth_sys->credential,
	                      &oncrpc_auth_none, ONCRPC_MOUNT, MOUNT_VERS );
	oncrpc_init_session ( &nfs->nfs_session, &oncrpc_auth_none,
	                      &oncrpc_auth_none, ONCRPC_NFS, NFS_VERS );

	nfs->filename   = basename ( nfs->mountpoint );
	nfs->mountpoint = dirname ( nfs->mountpoint );

	portmap_getport ( &nfs->pm_session, ONCRPC_MOUNT, MOUNT_VERS,
	                  PORTMAP_PROT_TCP, &getport_mount_cb );

	/* Attach to parent interface, mortalise self, and return */
	intf_plug_plug ( &nfs->xfer, xfer );
	ref_put ( &nfs->refcnt );

	return 0;
}

/** NFS URI opener */
struct uri_opener nfs_uri_opener __uri_opener = {
	.scheme	= "nfs",
	.open	= nfs_open,
};
