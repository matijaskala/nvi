/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static const char sccsid[] = "$Id: v_increment.c,v 8.10 1994/08/17 09:51:36 bostic Exp $ (Berkeley) $Date: 1994/08/17 09:51:36 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

static char * const fmt[] = {
#define	DEC	0
	"%ld",
#define	SDEC	1
	"%+ld",
#define	HEXC	2
	"%#0.*lX",
#define	HEXL	3
	"%#0.*lx",
#define	OCTAL	4
	"%#0.*lo",
};

/*
 * v_increment -- [count]#[#+-]
 *	Increment/decrement a keyword number.
 */
int
v_increment(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	VI_PRIVATE *vip;
	u_long ulval;
	long lval;
	size_t blen, len, nlen;
	int rval;
	char *bp, *ntype, *p, nbuf[100];

	vip = VIP(sp);

	/* Do repeat operations. */
	if (vp->character == '#')
		vp->character = vip->inc_lastch;

	/* Get new value. */
	if (F_ISSET(vp, VC_C1SET))
		vip->inc_lastval = vp->count;

	if (vp->character != '+' && vp->character != '-') {
		msgq(sp, M_ERR, "usage: %s", vp->kp->usage);
		return (1);
	}
	vip->inc_lastch = vp->character;

	/* Figure out the resulting type and number. */
	p = vp->keyword;
	len = vp->klen;
	if (len > 1 && p[0] == '0') {
		if (vp->character == '+') {
			ulval = strtoul(vp->keyword, NULL, 0);
			if (ULONG_MAX - ulval < vip->inc_lastval)
				goto overflow;
			ulval += vip->inc_lastval;
		} else {
			ulval = strtoul(vp->keyword, NULL, 0);
			if (ulval < vip->inc_lastval)
				goto underflow;
			ulval -= vip->inc_lastval;
		}
		ntype = fmt[OCTAL];
		if (len > 2)
			if (p[1] == 'X')
				ntype = fmt[HEXC];
			else if (p[1] == 'x')
				ntype = fmt[HEXL];
		nlen = snprintf(nbuf, sizeof(nbuf), ntype, len, ulval);
	} else {
		if (vp->character == '+') {
			lval = strtol(vp->keyword, NULL, 0);
			if (lval > 0 && LONG_MAX - lval < vip->inc_lastval) {
overflow:			msgq(sp, M_ERR, "Resulting number too large");
				return (1);
			}
			lval += vip->inc_lastval;
		} else {
			lval = strtol(vp->keyword, NULL, 0);
			if (lval < 0 && -(LONG_MIN - lval) < vip->inc_lastval) {
underflow:			msgq(sp, M_ERR, "Resulting number too small");
				return (1);
			}
			lval -= vip->inc_lastval;
		}
		ntype = lval != 0 &&
		    (*vp->keyword == '+' || *vp->keyword == '-') ?
		    fmt[SDEC] : fmt[DEC];
		nlen = snprintf(nbuf, sizeof(nbuf), ntype, lval);
	}

	if ((p = file_gline(sp, ep, vp->m_start.lno, &len)) == NULL) {
		GETLINE_ERR(sp, vp->m_start.lno);
		return (1);
	}

	GET_SPACE_RET(sp, bp, blen, len + nlen);
	memmove(bp, p, vp->m_start.cno);
	memmove(bp + vp->m_start.cno, nbuf, nlen);
	memmove(bp + vp->m_start.cno + nlen,
	    p + vp->m_start.cno + vp->klen, len - vp->m_start.cno - vp->klen);
	len = len - vp->klen + nlen;

	rval = file_sline(sp, ep, vp->m_start.lno, bp, len);
	FREE_SPACE(sp, bp, blen);
	return (rval);
}
