/*****************************************************************************

 @(#) $RCSfile$ $Name$($Revision$) $Date$

 -----------------------------------------------------------------------------

 Copyright (c) 2001-2007  OpenSS7 Corporation <http://www.openss7.com/>
 Copyright (c) 1997-2000  Brian F. G. Bidulock <bidulock@openss7.org>

 All Rights Reserved.

 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation, version 3 of the license.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 details.

 You should have received a copy of the GNU General Public License along with
 this program.  If not, see <http://www.gnu.org/licenses/>, or write to the
 Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 -----------------------------------------------------------------------------

 U.S. GOVERNMENT RESTRICTED RIGHTS.  If you are licensing this Software on
 behalf of the U.S. Government ("Government"), the following provisions apply
 to you.  If the Software is supplied by the Department of Defense ("DoD"), it
 is classified as "Commercial Computer Software" under paragraph 252.227-7014
 of the DoD Supplement to the Federal Acquisition Regulations ("DFARS") (or any
 successor regulations) and the Government is acquiring only the license rights
 granted herein (the license rights customarily provided to non-Government
 users).  If the Software is supplied to any unit or agency of the Government
 other than DoD, it is classified as "Restricted Computer Software" and the
 Government's rights in the Software are defined in paragraph 52.227-19 of the
 Federal Acquisition Regulations ("FAR") (or any successor regulations) or, in
 the cases of NASA, in paragraph 18.52.227-86 of the NASA Supplement to the FAR
 (or any successor regulations).

 -----------------------------------------------------------------------------

 Commercial licensing and support of this software is available from OpenSS7
 Corporation at a fee.  See http://www.openss7.com/

 -----------------------------------------------------------------------------

 Last Modified $Date$ by $Author$

 -----------------------------------------------------------------------------

 $Log$
 *****************************************************************************/

#ident "@(#) $RCSfile$ $Name$($Revision$) $Date$"

static char const ident[] = "$RCSfile$ $Name$($Revision$) $Date$";

/* dgram.c - datagram (CL-mode TS) abstractions */

#ifndef	lint
static char *rcsid =
    "Header: /xtel/isode/isode/compat/RCS/dgram.c,v 9.0 1992/06/16 12:07:00 isode Rel";
#endif

/* 
 * Header: /xtel/isode/isode/compat/RCS/dgram.c,v 9.0 1992/06/16 12:07:00 isode Rel
 *
 *
 * Log: dgram.c,v
 * Revision 9.0  1992/06/16  12:07:00  isode
 * Release 8.0
 *
 */

/*
 *				  NOTICE
 *
 *    Acquisition, use, and distribution of this module and related
 *    materials are subject to the restrictions of a license agreement.
 *    Consult the Preface in the User's Manual for the full terms of
 *    this agreement.
 *
 */

/* LINTLIBRARY */

#include <unistd.h>
#define getdtablesize() (sysconf (_SC_OPEN_MAX))
#include <stdlib.h>
#include <search.h>
#include <errno.h>
#include <stdio.h>
#include "general.h"
#include "manifest.h"
#include "tailor.h"

#include "dgram.h"
#ifdef	TCP
#include "internet.h"
#endif
#ifdef	TP4
#include "tp4.h"
#endif

#if	defined(SOCKETS) && (defined(TCP) || defined(CLTS))
#ifndef	DEBUG
#define	action(s,f,i)
#else
static action();
#endif

extern int errno;
extern int (*set_check_fd)();

union sockaddr_union {			/* 'cause sizeof (struct sockaddr_iso) == 32 */
	struct sockaddr sa;

#ifdef	TCP
	struct sockaddr_in sin;
#endif

#ifdef	BSD_TP4
	union sockaddr_osi sosi;
#endif
};

struct dgramblk {
	int dgram_parent;
	union sockaddr_union dgram_peer;
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	unsigned char dgram_addrlen;
#endif

	struct qbuf dgram_queue;
};

static int maxpeers = 0;
static struct dgramblk *peers = NULL;

#ifdef	TCP

/* ARGSUSED */

int
start_udp_server(sock, backlog, opt1, opt2)
	struct sockaddr_in *sock;
	int backlog, opt1, opt2;
{
	register int port;
	int sd;

#ifdef	BSD43
	int onoff;
#endif
	register struct dgramblk *up, *vp;

	if (peers == NULL) {
		maxpeers = getdtablesize();
		peers = (struct dgramblk *) calloc((unsigned) maxpeers, sizeof *peers);
		if (peers == NULL)
			return NOTOK;

		for (vp = (up = peers) + maxpeers; up < vp; up++) {
			up->dgram_parent = NOTOK;
			up->dgram_queue.qb_forw = up->dgram_queue.qb_back = &up->dgram_queue;
		}
	}

	if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) == NOTOK)
		return NOTOK;

	if (sock->sin_port != 0) {
		action("BIND", sd, (struct sockaddr *) sock);

		if (bind(sd, (struct sockaddr *) sock, sizeof *sock) != NOTOK)
			goto got_socket;

		(void) close(sd);
		return NOTOK;
	} else
		sock->sin_family = AF_INET;

	for (port = IPPORT_RESERVED;; port++) {
		sock->sin_port = htons((unsigned short) port);

		action("BIND", sd, (struct sockaddr *) sock);

		if (bind(sd, (struct sockaddr *) sock, sizeof *sock) != NOTOK)
			break;

		switch (errno) {
		case EADDRINUSE:
			continue;

		case EADDRNOTAVAIL:
		default:
			(void) close(sd);
			return NOTOK;
		}
	}

      got_socket:;
#ifdef	DEBUG
	{
		int len = sizeof *sock;

		action("FOO1", sd, (struct sockaddr *) sock);
		if (getsockname(sd, (struct sockaddr *) sock, &len) != NOTOK)
			action("FOO2", sd, (struct sockaddr *) sock);
	}
#endif

#ifndef	BSD43
	if (opt1)
		(void) setsockopt(sd, SOL_SOCKET, opt1, NULLCP, 0);
	if (opt2)
		(void) setsockopt(sd, SOL_SOCKET, opt2, NULLCP, 0);
#else
	onoff = 1;
	if (opt1)
		(void) setsockopt(sd, SOL_SOCKET, opt1, (char *) &onoff, sizeof onoff);
	if (opt2)
		(void) setsockopt(sd, SOL_SOCKET, opt2, (char *) &onoff, sizeof onoff);
#endif

	(void) set_check_fd(sd, check_dgram_socket, NULLCP);
	return (peers[sd].dgram_parent = sd);
}
#endif

#ifdef	BSD_TP4

/* ARGSUSED */

int
start_clts_server(sock, backlog, opt1, opt2)
	union sockaddr_osi *sock;
	int backlog, opt1, opt2;
{
	int sd;

#ifdef	BSD43
	int onoff;
#endif
	unsigned char *cp;
	register struct dgramblk *up, *vp;
	struct sockaddr_iso *ifaddr = &sock->osi_sockaddr;

	if (peers == NULL) {
		maxpeers = getdtablesize();
		peers = (struct dgramblk *) calloc((unsigned) maxpeers, sizeof *peers);
		if (peers == NULL)
			return NOTOK;

		for (vp = (up = peers) + maxpeers; up < vp; up++) {
			up->dgram_parent = NOTOK;
			up->dgram_queue.qb_forw = up->dgram_queue.qb_back = &up->dgram_queue;
		}
	}

	if ((sd = socket(AF_ISO, SOCK_DGRAM, 0)) == NOTOK)
		return NOTOK;

	if (ifaddr->siso_tlen != 0) {
		action("BIND", sd, (struct sockaddr *) ifaddr);

		if (bind(sd, (struct sockaddr *) ifaddr, (int) ifaddr->siso_len)
		    != NOTOK)
			goto got_socket;

		(void) close(sd);
		return NOTOK;
	} else
		ifaddr->siso_family = AF_ISO;

	{
		int pid;
		unsigned char *dp, *ep, *fp;

		pid = getpid();
		cp = fp = (unsigned char *) ifaddr->siso_data + ifaddr->siso_nlen;
		for (ep = (dp = (unsigned char *) &pid) + sizeof pid; dp < ep; dp++)
			*cp++ = *dp;
		ifaddr->siso_tlen = (cp - fp) + 1;
		ifaddr->siso_slen = ifaddr->siso_plen = 0;
		ifaddr->siso_len = sizeof *ifaddr;
	}

	for (*cp = 0x00; *cp < 0xff; *cp += 1) {
		action("BIND", sd, (struct sockaddr *) ifaddr);

		if (bind(sd, (struct sockaddr *) ifaddr, (int) ifaddr->siso_len)
		    != NOTOK)
			goto got_socket;

		switch (errno) {
		case EADDRINUSE:
			continue;

		case EADDRNOTAVAIL:
		default:
			(void) close(sd);
			return NOTOK;
		}
	}
	(void) close(sd);
	errno = EADDRNOTAVAIL;
	return NOTOK;

      got_socket:;
#ifdef	DEBUG
	{
		int len = sizeof *sock;

		action("FOO1", sd, ifaddr);
		if (getsockname(sd, (struct sockaddr *) ifaddr, &len) != NOTOK)
			action("FOO2", sd, ifaddr);
	}
#endif

#ifndef	BSD43
	if (opt1)
		(void) setsockopt(sd, SOL_SOCKET, opt1, NULLCP, 0);
	if (opt2)
		(void) setsockopt(sd, SOL_SOCKET, opt2, NULLCP, 0);
#else
	onoff = 1;
	if (opt1)
		(void) setsockopt(sd, SOL_SOCKET, opt1, (char *) &onoff, sizeof onoff);
	if (opt2)
		(void) setsockopt(sd, SOL_SOCKET, opt2, (char *) &onoff, sizeof onoff);
#endif

	(void) set_check_fd(sd, check_dgram_socket, NULLCP);
	return (peers[sd].dgram_parent = sd);
}
#endif

int
join_dgram_aux(fd, sock, newfd)
	int fd, newfd;
	struct sockaddr *sock;
{
	int nfds, sd;
	fd_set ifds;
	register struct qbuf *qb;
	register struct dgramblk *up;

	if (fd < 0 || fd >= maxpeers || peers[fd].dgram_parent != fd) {
		errno = EINVAL;
		return NOTOK;
	}

	if (newfd) {
		FD_ZERO(&ifds);

		nfds = fd + 1;
		FD_SET(fd, &ifds);
		if (select_dgram_socket(nfds, &ifds, NULLFD, NULLFD, OK) == NOTOK)
			return NOTOK;

		up = &peers[fd];
		if ((qb = up->dgram_queue.qb_forw) == &up->dgram_queue) {
			errno = EWOULDBLOCK;
			return NOTOK;
		}

		if ((sd = dup(fd)) == NOTOK)
			return NOTOK;
		(void) set_check_fd(fd, check_dgram_socket, NULLCP);

		up = &peers[sd];
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
		bcopy(qb->qb_base, (caddr_t) sock, ((struct sockaddr *) qb->qb_base)->sa_len);
#else
		*sock = *((struct sockaddr *) qb->qb_base);	/* struct copy */
#endif

		remque(qb);
		insque(qb, up->dgram_queue.qb_back);
	} else
		up = &peers[fd];

	up->dgram_parent = fd;
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	if (sock->sa_len == 0)
		sock->sa_len = sizeof *sock;
	bcopy((caddr_t) sock, (caddr_t) &up->dgram_peer, sock->sa_len);
	{
		struct sockaddr_in *sin;

		up->dgram_addrlen = sock->sa_family != AF_INET ? sock->sa_len
		    : sizeof *sin - sizeof sin->sin_zero;
	}
#else
	up->dgram_peer.sa = *sock;	/* struct copy */
#endif

	action("JOIN", newfd ? sd : fd, sock);

	return (newfd ? sd : OK);
}

int
read_dgram_socket(fd, q)
	int fd;
	struct qbuf **q;
{
	int nfds;
	fd_set ifds, mask;
	register struct qbuf *qb;
	register struct dgramblk *up;

	if (fd < 0 || fd >= maxpeers || (up = &peers[fd])->dgram_parent == NOTOK) {
		errno = EINVAL;
		return NOTOK;
	}

	if ((qb = up->dgram_queue.qb_forw) == &up->dgram_queue) {
		FD_ZERO(&mask);

		nfds = fd + 1;
		FD_SET(fd, &mask);
		for (ifds = mask;; ifds = mask) {
			if (select_dgram_socket(nfds, &ifds, NULLFD, NULLFD, NOTOK)
			    == NOTOK)
				return NOTOK;

			if ((qb = up->dgram_queue.qb_forw) != &up->dgram_queue)
				break;
		}
	}

	remque(qb);
	qb->qb_forw = qb->qb_back = qb;

	*q = qb;

	return qb->qb_len;
}

int
hack_dgram_socket(fd, sock)
	int fd;
	struct sockaddr *sock;
{
	register struct dgramblk *up;

	if (fd < 0 || fd >= maxpeers || (up = &peers[fd])->dgram_parent != fd) {
		errno = EINVAL;
		return NOTOK;
	}

	if (sock == NULL) {
		bzero((caddr_t) &up->dgram_peer, sizeof up->dgram_peer);
		return OK;
	}
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	if (sock->sa_len == 0)
		sock->sa_len = sizeof *sock;
	bcopy((caddr_t) sock, (caddr_t) &up->dgram_peer, sock->sa_len);
	up->dgram_addrlen = 0;
#else
	up->dgram_peer.sa = *sock;	/* struct copy */
#endif

	action("HACK", fd, sock);

	return OK;
}

int
write_dgram_socket(fd, qb)
	int fd;
	register struct qbuf *qb;
{
	register struct dgramblk *up;

	if (fd < 0
	    || fd >= maxpeers
	    || (up = &peers[fd])->dgram_parent == NOTOK || up->dgram_peer.sa.sa_family == 0) {
		errno = EINVAL;
		return NOTOK;
	}

	action("SENDTO", fd, &up->dgram_peer.sa);

#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	return sendto(fd, qb->qb_data, qb->qb_len, NULL,
		      &up->dgram_peer.sa, (int) up->dgram_peer.sa.sa_len);
#else
	return sendto(fd, qb->qb_data, qb->qb_len, 0,
		      &up->dgram_peer.sa, sizeof(up->dgram_peer.sa));
#endif
}

int
close_dgram_socket(fd)
	int fd;
{
	register struct dgramblk *up, *vp;

	if (fd < 0 || fd >= maxpeers || (up = &peers[fd])->dgram_parent == NOTOK) {
		errno = EINVAL;
		return NOTOK;
	}

	action("CLOSE", fd, &up->dgram_peer.sa);

	up->dgram_parent = NOTOK;
	bzero((char *) &up->dgram_peer, sizeof up->dgram_peer);
	QBFREE(&up->dgram_queue);

	for (vp = (up = peers) + maxpeers; up < vp; up++)
		if (up->dgram_parent == fd)
			up->dgram_parent = up - peers;

	(void) set_check_fd(fd, NULL, NULLCP);
	return close(fd);
}

int
select_dgram_socket(nfds, rfds, wfds, efds, secs)
	int nfds;
	fd_set *rfds, *wfds, *efds;
	int secs;
{
	register int fd;
	int cc, mfds, result;
	fd_set ifds, jfds;
	register struct qbuf *qb;
	register struct dgramblk *up, *vp;
	struct dgramblk *wp;
	union sockaddr_union *sock;

	if (rfds) {
		jfds = *rfds;

		if (secs != OK)
			for (vp = (up = peers) + maxpeers, fd = 0; up < vp; up++, fd++)
				if (up->dgram_parent != NOTOK && FD_ISSET(fd, &jfds)
				    && up->dgram_queue.qb_forw != &up->dgram_queue) {
					secs = OK;
					break;
				}
	}

	if ((result = selsocket(nfds, rfds, wfds, efds, secs)) == NOTOK || rfds == NULLFD)
		return result;

	ifds = *rfds;
	if ((mfds = nfds) > maxpeers)
		mfds = maxpeers;
	for (fd = 0, up = peers; fd < mfds; fd++, up++)
		if (FD_ISSET(fd, &ifds)) {
			socklen_t slen;
			unsigned char len;
			char *data;

			FD_CLR(fd, &ifds);

			if (up->dgram_parent == NOTOK)
				continue;

			if ((qb = (struct qbuf *) malloc((unsigned) (sizeof *qb
								     + (slen = sizeof *sock)
								     + MAXDGRAM)))
			    == NULL)
				return NOTOK;

			sock = (union sockaddr_union *) qb->qb_base;
			qb->qb_data = qb->qb_base + slen;
			if ((cc = recvfrom(fd, qb->qb_data, MAXDGRAM, 0,
					   &sock->sa, &slen)) == NOTOK) {
				free((char *) qb);
				return NOTOK;
			}
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
			sock->sa.sa_len = slen;
#endif
			qb->qb_len = cc;

			action("RECVFROM", fd, &sock->sa);

			vp = up;
			data = sock->sa.sa_data;
			switch (sock->sa.sa_family) {
			case AF_INET:	/* XXX: doesn't take into account padding */
				len = sizeof sock->sa.sa_data - sizeof sock->sin.sin_zero;
				break;

			default:
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
				len = sock->sa.sa_len;
#else
				len = sizeof sock->sa;
#endif
				break;
			}
			if (
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
				   len != up->dgram_addrlen ||
#endif
				   bcmp(data, up->dgram_peer.sa.sa_data, (int) len)
				   != 0) {
				for (wp = (vp = peers) + maxpeers; vp < wp; vp++)
					if (vp != up && vp->dgram_parent == up->dgram_parent
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
					    && len == vp->dgram_addrlen
#endif
					    && bcmp(data, vp->dgram_peer.sa.sa_data,
						    (int) len) == 0)
						break;
				if (vp >= wp && (vp = &peers[up->dgram_parent])
				    ->dgram_peer.sa.sa_family != 0) {
					free((char *) qb);
					continue;
				}
			}

			insque(qb, vp->dgram_queue.qb_back);

			if (--result <= 0 || (result = selsocket(nfds, &ifds, NULLFD, NULLFD, OK))
			    <= 0)
				break;

		}

	for (vp = (up = peers) + maxpeers, fd = 0; up < vp; up++, fd++) {
		if (up->dgram_parent != NOTOK && FD_ISSET(fd, &jfds)) {
			if (up->dgram_queue.qb_forw != &up->dgram_queue)
				FD_SET(fd, rfds);
			else
				FD_CLR(fd, rfds);
		}
	}

	result = 0;
	ifds = *rfds;
	if (wfds)
		for (fd = 0; fd < nfds; fd++)
			if (FD_ISSET(fd, wfds))
				FD_SET(fd, &ifds);
	if (efds)
		for (fd = 0; fd < nfds; fd++)
			if (FD_ISSET(fd, efds))
				FD_SET(fd, &ifds);
	for (fd = 0; fd < nfds; fd++)
		if (FD_ISSET(fd, &ifds))
			result++;

	return result;
}

int
check_dgram_socket(fd)
	int fd;
{
	int nfds;
	fd_set ifds;

	FD_ZERO(&ifds);

	nfds = fd + 1;
	FD_SET(fd, &ifds);

	return select_dgram_socket(nfds, &ifds, NULLFD, NULLFD, OK);
}

#ifdef	DEBUG

#ifdef	TCP
#include "isoaddrs.h"

static
inetprint(sin, bp)
	struct sockaddr_in *sin;
	char *bp;
{
	(void) sprintf(bp, "Internet=%s+%d+%d", inet_ntoa(sin->sin_addr),
		       (int) ntohs(sin->sin_port), NA_TSET_UDP);
}
#endif

#ifdef	CLTS
/* prints OSI address using the format described in:

	"A string encoding of Presentation Address"

	S.E. Kille, Research Note RN/89/14, February 1989
	Department of Computer Science
	University College London

 */

#ifndef SSEL
#define SSEL(s) ((s)->siso_tlen + TSEL(s))
#define PSEL(s) ((s)->siso_slen + SSEL(s))
#endif

static
isoprint(siso, bp)
	register struct sockaddr_iso *siso;
	char *bp;
{
	int didone = 0;

	if (siso->siso_plen) {
		hexprint(bp, siso->siso_plen, PSEL(siso), "'", "'H");
		bp += strlen(bp);
		*bp++ = '/';
		didone++;
	}
	if (siso->siso_slen || didone) {
		hexprint(bp, siso->siso_slen, SSEL(siso), "'", "'H");
		bp += strlen(bp);
		*bp++ = '/';
		didone++;
	}
	if (siso->siso_tlen || didone) {
		hexprint(bp, siso->siso_tlen, TSEL(siso), "'", "'H");
		bp += strlen(bp);
		*bp++ = '/';
		didone++;
	}
	hexprint(bp, siso->siso_nlen, siso->siso_data, "NS+", "");
}

static
hexprint(bp, n, buf, start, stop)
	char *bp;
	int n;
	unsigned char *buf;
	char *start, *stop;
{
	register unsigned char *in = buf, *top = in + n;

	if (n == 0)
		return;

	(void) strcpy(bp, start);
	bp += strlen(bp);

	while (in < top) {
		(void) sprintf(bp, "%02x", *in++ & 0xff);
		bp += 2;
	}

	(void) strcpy(bp, stop);
}
#endif

static struct printent {
	int p_family;
	int (* p_function)();
} ents[] = {
#ifdef	TCP
	AF_INET, inetprint,
#endif
#ifdef	CLTS
	    AF_ISO, isoprint,
#endif
NULL};

static
action(s, fd, sock)
	char *s;
	int fd;
	struct sockaddr *sock;
{
	char buffer[BUFSIZ];
	register struct printent *p;

	if (!(compat_log->ll_events & LLOG_TRACE))
		return;

	for (p = ents; p->p_family; p++)
		if (p->p_family == sock->sa_family)
			break;
	if (!p->p_family) {
		DLOG(compat_log, LLOG_EXCEPTIONS,
		     ("unknown dgram address family 0x%x", sock->sa_family));
		return;
	}

	(void) (*p->p_function) (sock, buffer);

	DLOG(compat_log, LLOG_TRACE, ("%-10.10s %d %s", s, fd, buffer));
}
#endif

#else

int
dgram_dummy()
{
	return (0);
}

#endif

static inline void
dummy(void)
{
	(void) rcsid;
}