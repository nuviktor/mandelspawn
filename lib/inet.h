/* inet.h -- internet-related declarations */

#ifndef _inet_h
#define _inet_h

#ifdef _AIX
#ifndef NEED_SYS_SELECT_H
#define NEED_SYS_SELECT_H
#endif
#endif

#ifdef NEED_SYS_SELECT_H
/* for AIX, mostly */
#include <sys/select.h>
#endif

#ifndef __TYPES__
/* __TYPES__ is defined by X headers that include <sys/types.h> */
#include <sys/types.h>
#include <sys/file.h>		/* HP-UX needs this for FNDELAY */
#ifdef HAVE_SELECT
#include <sys/time.h>		/* 4.3 BSD needs this and <sys/time.h> for select() */
#endif
#endif

#include <fcntl.h>

#ifdef HAVE_SOCKETS

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
typedef struct sockaddr_in NET_ADDRESS;

#else				/* !HAVE_SOCKETS */

typedef struct {
	int sin_port;		/* dummy, needed by mslaved */
} NET_ADDRESS;

/* define dummy byteorder functions (there is no network, so we don't */
/* need to be network portable) */
#define ntohl(x) (x)
#define htonl(x) (x)
#define ntohs(x) (x)
#define htons(x) (x)

#endif				/* !HAVE_SOCKETS */

/* network to host byteorder, long (signed) integer (ntohl may return an */
/* unsigned quantity) */
#define ntohli(x) ((sint32) ntohl(x))

#endif				/* _inet_h */
