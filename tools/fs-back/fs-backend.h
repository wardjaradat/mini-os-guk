/*
 * Copyright (c) 2009 Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, California 95054, U.S.A. All rights reserved.
 * 
 * U.S. Government Rights - Commercial software. Government users are
 * subject to the Sun Microsystems, Inc. standard license agreement and
 * applicable provisions of the FAR and its supplements.
 * 
 * Use is subject to license terms.
 * 
 * This distribution may include materials developed by third parties.
 * 
 * Parts of the product may be derived from Berkeley BSD systems,
 * licensed from the University of California. UNIX is a registered
 * trademark in the U.S.  and in other countries, exclusively licensed
 * through X/Open Company, Ltd.
 * 
 * Sun, Sun Microsystems, the Sun logo and Java are trademarks or
 * registered trademarks of Sun Microsystems, Inc. in the U.S. and other
 * countries.
 * 
 * This product is covered and controlled by U.S. Export Control laws and
 * may be subject to the export or import laws in other
 * countries. Nuclear, missile, chemical biological weapons or nuclear
 * maritime end uses or end users, whether direct or indirect, are
 * strictly prohibited. Export or reexport to countries subject to
 * U.S. embargo or to entities identified on U.S. export exclusion lists,
 * including, but not limited to, the denied persons and specially
 * designated nationals lists is strictly prohibited.
 * 
 */
/*
 * xenbus interface for Guest VM microkernel fs support
 *
 * Author: Grzegorz Milos, Sun Microsystems, Inc., summer intern 2007.
 *         Mick Jordan, Sun Microsystems, Inc.
 * 
 */

#ifndef __LIB_FS_BACKEND__
#define __LIB_FS_BACKEND__

#include <aio.h>
#include <xs.h>
#include <xen/grant_table.h>
#include <xen/event_channel.h>
#include <xen/io/ring.h>
#include "fsif.h"

#define ROOT_NODE           "backend/vfs"
#define EXPORTS_SUBNODE     "exports"
#define EXPORTS_NODE        ROOT_NODE"/"EXPORTS_SUBNODE
#define WATCH_NODE          EXPORTS_NODE"/requests"

#define TRACE_OPS 1
#define TRACE_OPS_NOISY 2
#define TRACE_RING 3

struct fs_export
{
    int export_id;
    char *export_path;
    char *name;
    struct fs_export *next; 
};

struct fs_request
{
    int active;
    void *page;                         /* Pointer to mapped grant */
    struct fsif_request req_shadow;
    struct aiocb aiocb; 
};


struct mount
{
    struct fs_export *export;
    int dom_id;
    char *frontend;
    int mount_id;                     /* = backend id */
    grant_ref_t gref;
    evtchn_port_t remote_evtchn;
    int evth;                         /* Handle to the event channel */
    evtchn_port_t local_evtchn;
    int gnth;
    struct fsif_back_ring ring;
    int nr_entries;
    struct fs_request *requests;
    unsigned short *freelist;
};


/* Handle to XenStore driver */
extern struct xs_handle *xsh;

bool xenbus_create_request_node(void);
int xenbus_register_export(struct fs_export *export);
int xenbus_get_watch_fd(void);
void xenbus_read_mount_request(struct mount *mount);
void xenbus_write_backend_node(struct mount *mount);
void xenbus_write_backend_ready(struct mount *mount);

/* File operations, implemented in fs-ops.c */
struct fs_op
{
    int type;       /* Type of request (from fsif.h) this handlers 
                       are responsible for */
    void (*dispatch_handler)(struct mount *mount, struct fsif_request *req);
    void (*response_handler)(struct mount *mount, struct fs_request *req);
};

/* This NULL terminated array of all file requests handlers */
extern struct fs_op *fsops[];

static inline void add_id_to_freelist(unsigned int id,unsigned short* freelist)
{
    freelist[id] = freelist[0];
    freelist[0]  = id;
}

static inline unsigned short get_id_from_freelist(unsigned short* freelist)
{
    unsigned int id = freelist[0];
    freelist[0] = freelist[id];
    return id;
}

#endif /* __LIB_FS_BACKEND__ */
