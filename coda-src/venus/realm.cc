/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 2002 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
}
#endif

#include <rvmlib.h>
#include "rec_dllist.h"
#include "realm.h"
#include "realmdb.h"
#include "comm.h"
#include "parse_realms.h"

/* MUST be called from within a transaction */
Realm::Realm(const char *realm_name)
{
    int len = strlen(realm_name) + 1;

    RVMLIB_REC_OBJECT(name);
    name = (char *)rvmlib_rec_malloc(len); 
    CODA_ASSERT(name);
    rvmlib_set_range(name, len);
    strcpy(name, realm_name);

    RVMLIB_REC_OBJECT(rec_refcount);
    rec_refcount = 0;

    rec_list_head_init(&realms);

    rootservers = NULL;
    refcount = 1;
}

/* MUST be called from within a transaction */
Realm::~Realm(void)
{
    VenusFid Fid;
    fsobj *f;

    CODA_ASSERT(!rec_refcount && refcount <= 1);

    rec_list_del(&realms);
    if (rootservers) {
	eprint("Removing realm '%s'", name);

	coda_freeaddrinfo(rootservers);
	rootservers = NULL;
    }
    rvmlib_rec_free(name); 

    Realm *localrealm = REALMDB->GetRealm(LOCALREALM);

    /* kill the fake object that represents our mountlink */
    Fid.Realm = localrealm->Id();
    Fid.Volume = FakeRootVolumeId;
    Fid.Vnode = 0xfffffffc;
    Fid.Unique = Id();

    localrealm->PutRef();

    f = FSDB->Find(&Fid);
    if (f) f->Kill();
}

/* MAY be called from within a transaction */
void Realm::ResetTransient(void)
{
    rootservers = NULL;
    refcount = 0;

    if (rvmlib_in_transaction() && !rec_refcount)
	delete this;
}

/* MUST be called from within a transaction */
void Realm::Rec_PutRef(void)
{
    CODA_ASSERT(rec_refcount);
    RVMLIB_REC_OBJECT(rec_refcount);
    rec_refcount--;
    if (!refcount && !rec_refcount)
	delete this;
}

/* MAY be called from within a transaction */
void Realm::PutRef(void)
{
    CODA_ASSERT(refcount);
    refcount--;
    /*
     * Only destroy the object if we happen to be in a transaction,
     * otherwise we'll destroy ourselves later during ResetTransient,
     * or when a reference is regained and then dropped in a transaction.
     */
    if (rvmlib_in_transaction() && !refcount && !rec_refcount)
	delete this;
}

/* Get a connection to any server (as root). */
/* MUST NOT be called from within a transaction */
int Realm::GetAdmConn(connent **cpp)
{
    struct coda_addrinfo *p;
    int code = 0;
    int tryagain = 0;
    int unknown = !rootservers;

    LOG(100, ("GetAdmConn: \n"));

    if (STREQ(name, LOCALREALM))
	return ETIMEDOUT;

    *cpp = 0;

retry:
    if (!rootservers)
	GetRealmServers(name, "codasrv", &rootservers);
    else {
	coda_reorder_addrs(&rootservers);
	/* our cached addresses might be stale, re-resolve if we can't reach
	 * any of the servers */
	tryagain = 1;
    }

    if (!rootservers)
	return ETIMEDOUT;

    /* Get a connection to any custodian. */
    for (p = rootservers; p; p = p->ai_next) {
	struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
	srvent *s;
	s = ::GetServer(&sin->sin_addr, Id());
	code = s->GetConn(cpp, V_UID);
	switch(code) {
	case ERETRY:
	    tryagain = 1;
	case ETIMEDOUT:
	    continue;

	case 0:
	case EINTR:
	    /* We might have discovered a new realm */
	    if (unknown) {
		Realm *localrealm;
		VenusFid Fid;
		fsobj *f;

		eprint("Resolved realm '%s'", name);

		localrealm = REALMDB->GetRealm(LOCALREALM);
		Fid.Realm = LocalRealm->Id();
		Fid.Volume = FakeRootVolumeId;
		Fid.Vnode = 1;
		Fid.Unique = 1;
		localrealm->PutRef();

		f = FSDB->Find(&Fid);
		if (f) {
		    Recov_BeginTrans();
		    f->Kill();
		    Recov_EndTrans(MAXFP);
		}
	    }
	    return code;

	default:
	    if (code < 0)
		eprint("GetAdmConn: bogus code (%d)", code);
	    return code;
	}
    }
    if (tryagain) {
	coda_freeaddrinfo(rootservers);
	rootservers = NULL;
	tryagain = 0;
	goto retry;
    }
    return ETIMEDOUT;
}

void Realm::print(FILE *f)
{
    struct coda_addrinfo *p;

    fprintf(f, "%08x realm '%s', refcount %d/%d\n", (unsigned int)Id(), Name(),
	    refcount, rec_refcount);
    for (p = rootservers; p; p = p->ai_next) {
	struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
	fprintf(f, "\t%s\n", inet_ntoa(sin->sin_addr)); 
    }
}
