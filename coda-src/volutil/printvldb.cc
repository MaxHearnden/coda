#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /usr/rvb/XX/src/coda-src/volutil/RCS/printvldb.cc,v 4.1 1997/01/08 21:52:26 rvb Exp $";
#endif /*_BLURB_*/






/******************************************/
/* Print out vldb, copied from vol/vldb.c */
/******************************************/

/* NOTE: since the vldb is a hash table that contains two entries for each
 * volume (namely hashed by name and hashed by volid in ascii), each volume
 * is printed out twice!
 */

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <stdio.h>
#include <sys/file.h>
#include <netinet/in.h>

#ifdef __MACH__
#include <sysent.h>
#include <libc.h>
#else	/* __linux__ || __BSD44__ */
#include <unistd.h>
#include <stdlib.h>
#endif
    
#include <lwp.h>
#include <lock.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include <vnode.h>
#include <volume.h>
#include <vldb.h>

#define LEFT(i)		2 * (i);
#define RIGHT(i) 	2*(i) + 1;
#define VID(lp) 	ntohl((lp)->volumeId[(lp)->volumeType])
#define UNIQUE(vid)	((vid) & 0xffffff)	/* strip hostid bits */
    
void heapify(struct vldb a[], int i, int size)
{
    int l, r, largest;
    struct vldb tmp;
    
    l = LEFT(i);
    r = RIGHT(i);

    largest = ((l <= size) && VID(&a[l]) > VID(&a[i])) ?  l : i;

    if ((r <=size) && VID(&a[r]) > VID(&a[largest]))
	largest = r;
	
    if (largest != i) {
	bcopy(&a[i], &tmp, sizeof(struct vldb));
	bcopy(&a[largest], &a[i], sizeof(struct vldb));
	bcopy(&tmp, &a[largest], sizeof(struct vldb));
	heapify(a, largest, size);
    }
}

void heapsort(struct vldb a[], int length)
{
    int i, size = length;
    struct vldb tmp;
    
    for (i = length / 2; i >= 1; i--)
	heapify(a, i, size);

    for (i = length; i >= 2; i--) {
	bcopy(&a[i], &tmp, sizeof(struct vldb));
	bcopy(&a[1], &a[i], sizeof(struct vldb));
	bcopy(&tmp, &a[1], sizeof(struct vldb));
	heapify(a, 1, --size);
    }
}
    
void main(int argc, char **argv)
{
    if (argc > 1) printf("Usage: %s\n", argv[0]); /* code to supress warnings */
    
    struct vldb buffer[8];
    
    int VLDB_fd = open(VLDB_PATH, O_RDONLY, 0);
    if (VLDB_fd == -1) 
	exit(-1);

    int size = 8;			/* Current size of VLDB array */
    int nentries = 0;			/* Number of valid records in VLDB */
    struct vldb *VLDB = (struct vldb *)malloc(size * sizeof(struct vldb));
    register int i;
    
    for (;;) {
        int n;
	register nRecords=0;
	n = read(VLDB_fd, (char *)buffer, sizeof(buffer));
	if (n < 0) {
	    printf("VLDBPrint: read failed for VLDB\n");
	    exit(-1);
	}
	if (n==0)
	    break;

	nRecords = (n>>LOG_VLDBSIZE);

	for (i = 0; i < nRecords; i++) {
	    register struct vldb *vldp = &buffer[i];

/* There are two entries in the VLDB for each volume, one is keyed on the
   volume name, and the other is keyed on the volume id in alphanumeric form.
   I feel we should only print out the entry with the volume name. */
	    
	    if ((VID(vldp) != 0) && (VID(vldp) != atoi(vldp->key))) {
		bcopy(vldp, &VLDB[nentries++], sizeof(struct vldb));
		if (nentries == size) {
		    struct vldb *tmp;

		    size *= 2;
		    tmp = (struct vldb *)malloc(size * sizeof(struct vldb));
		    bcopy(VLDB, tmp, nentries * sizeof(struct vldb));
		    free(VLDB);
		    VLDB = tmp;
		}
	    }
	}
    }

    heapsort(VLDB, nentries);
    for (i = 0; i<nentries; i++) {
	register struct vldb *vldp = &VLDB[i];
	printf("VID =%x, key = %s, type = %x, servers = (%x", 
	       VID(vldp), vldp->key, vldp->volumeType, vldp->serverNumber[0]);
	    
	for (byte j = 1; j < vldp->nServers; j++)
	    printf(",%x", vldp->serverNumber[j]);
	printf(")\n");
    }	
}    
     
