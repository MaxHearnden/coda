/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifdef __CYGWIN32__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winbase.h>

#include "coda_string.h"
#include "coda.h"
#include "util.h"

#ifdef __cplusplus
}
#endif

#include "nt_util.h"

/*
 * NT specific routines 
 *
 */

// Mounts ...

// Parameters for nd_do_mounts ... both for direct call
// and via CreateThread.
static char drive;
static int  mount;

int
wcslen (PWCHAR wstr)
{
    int len = 0;
    while (*wstr++) len++;
    return len;
}


// Can't use "normal" parameters due ot the fact that this will 
// be called to start a new thread.  Use the static it for mount
// communication.
static DWORD
nt_do_mounts (void *junk)
{
    HANDLE h;
    OW_PSEUDO_MOUNT_INFO info;
    DWORD nBytesReturned;
    CHAR outBuf[4096];
    DWORD d;
    int n;
    int ctlcode = OW_FSCTL_DISMOUNT_PSEUDO;
    
    WCHAR link[20] = L"\\??\\X:";  
    
    // Parameters ...
    link[4] = (short)drive;
    if (mount)
	ctlcode = OW_FSCTL_MOUNT_PSEUDO;
    
    d = DefineDosDevice(DDD_RAW_TARGET_PATH, "codadev", "\\Device\\codadev");
    if ( d == 0 ) {
	if (mount) {
	    eprint ("DDD failed, mount failed.  Killing venus.");
	    kill(getpid(), SIGKILL);
	    exit(1);
	} else {
	    eprint ("DDD failed, umount failed.");
	    return 1;
	}
    }

    h = CreateFile ("\\\\.\\codadev", GENERIC_READ | GENERIC_WRITE,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		    OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) {
	if (mount) {
	    eprint ("CreateFile failed, mount failed.  Killing venus.");
	    kill(getpid(), SIGKILL);
	    exit(1); 
	} else { 
	    eprint ("CreateFile failed, umount failed.");
	    return 1;
	}
    } 
    
    // Set up the info for the DeviceIoControl.
    info.PseudoVolumeHandle = (HANDLE*)1;
    info.PseudoDeviceName = L"\\Device\\coda";
    info.PseudoDeviceNameLength =
	wcslen(info.PseudoDeviceName) * sizeof(WCHAR);
    info.PseudoLinkName = link;
    info.PseudoLinkNameLength = wcslen(info.PseudoLinkName) * sizeof(WCHAR);
    
    d = DeviceIoControl(h, ctlcode, &info, sizeof(info), NULL, 0,
			&nBytesReturned, NULL);
    if (!d) {
	if (mount) {
	    eprint ("Mount failed.  Killing venus.");
	    kill(getpid(), SIGKILL);
	    exit(1);
	} else {
	    eprint ("Umount failed.");
	    return 1;
	}
    } 

    return 0;
}

void nt_umount (char *drivename)
{
    drive = drivename[0];
    mount = 0;
    (void) nt_do_mounts (NULL);
}

void
nt_mount (char *drivename)
{
    HANDLE h;

    nt_umount (drivename);

    mount = 1;
    
    h = CreateThread(0, 0, nt_do_mounts, NULL, 0, NULL);

    if (!h) {
	eprint ("CreateThread failed.  Mount unsuccessful.  Killing venus.");
	kill(getpid(), SIGKILL);
	exit(1); 
    }

    CloseHandle (h);
}

//
// kernel -> venus ... using a socket pair.
//

static int sockfd;
static volatile int doexit;

static DWORD
listen_kernel (void *junk)
{
    HANDLE h;
    int rc;
    DWORD bytesret;
    char outbuf[VC_MAXMSGSIZE];

    // Get the device ready;
    rc = DefineDosDevice(DDD_RAW_TARGET_PATH, "codadev", "\\Device\\codadev");
    if ( rc == 0 ) {
	eprint ("DDD failed, listen_kernel failed.");
	kill(getpid(), SIGKILL);
	exit(1); 
    }

    h = CreateFile ("\\\\.\\codadev", GENERIC_READ | GENERIC_WRITE,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		    OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) {
	eprint ("CreateFile failed, listen_kernel failed.");
	kill(getpid(), SIGKILL);
	exit(1); 
    } 

    while (1) {
	// Do a device ioctl.
	bytesret = 0;
	rc = DeviceIoControl (h, CODA_FSCTL_FETCH, NULL, 0, outbuf,
			      VC_MAXMSGSIZE, &bytesret, NULL);
	if (rc) {
	    if (bytesret > 0) {
		write (sockfd, (char *)&bytesret, sizeof(bytesret));
		write (sockfd, outbuf, bytesret);
	    }
	} else {
	    eprint ("listen_kernel: fetch failed");
	}
	if (doexit)
	  ExitThread(0);
    }
}

// "public" interface for ipc

static HANDLE kerndev;
static HANDLE kernelmon;

int nt_initialize_ipc (int sock)
{
    int rc;

    // Get the device ready for writing to kernel
    rc = DefineDosDevice(DDD_RAW_TARGET_PATH, "codadev", "\\Device\\codadev");
    if ( rc == 0 ) {
	eprint ("nt_initialize_ipc: DDD failed.");
	return 0; 
    }

    kerndev = CreateFile ("\\\\.\\codadev", GENERIC_READ | GENERIC_WRITE,
			  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			  OPEN_EXISTING, 0, NULL);

    if (kerndev == INVALID_HANDLE_VALUE) {
	eprint ("nt_initialize_ipc: CreateFile failed.");
	return 0; 
    } 

    // Other initialization
    sockfd = sock;
    doexit = 0;

    // Start the kernel monitor
    kernelmon = CreateThread (0, 0, listen_kernel, NULL, 0, NULL);
    if (kernelmon == NULL) {
	return 0;
    }

    // All was successful 
    return 1;
}

int nt_msg_write (char *buf, int size)
{
    int rc;
    DWORD bytesret;

    //    eprint ("nt_msg_write: Start\n");
    rc = DeviceIoControl (kerndev, CODA_FSCTL_ANSWER, buf, size, NULL, 0,
			  &bytesret, NULL);
    //    eprint ("nt_msg_write: End\n");
    if (!rc)
	return 0;
    
    return size;
}

void nt_stop_ipc (void)
{
    (void) TerminateThread (kernelmon,  0);
    CloseHandle(kernelmon);
    doexit = 1;
}


#endif