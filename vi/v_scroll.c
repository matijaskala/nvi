/*-
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: v_scroll.c,v 5.30 1993/04/12 14:54:30 bostic Exp $ (Berkeley) $Date: 1993/04/12 14:54:30 $";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_lgoto -- [count]G
 *	Go to first non-blank character of the line count, the last line
 *	of the file by default.
 */
int
v_lgoto(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t last;

	last = file_lline(sp, ep);
	if (F_ISSET(vp, VC_C1SET)) {
		if (last < vp->count) {
			v_eof(sp, ep, fm);
			return (1);
		}
		rp->lno = vp->count;
	} else
		rp->lno = last;
	return (0);
}

/* 
 * v_home -- [count]H
 *	Move to the first non-blank character of the line count from
 *	the top of the screen, 1 by default.
 */
int
v_home(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (sp->position(sp, ep, &rp->lno,
	    F_ISSET(vp, VC_C1SET) ? vp->count : 1, P_TOP));
}

/*
 * v_middle -- M
 *	Move to the first non-blank character of the line in the middle
 *	of the screen.
 */
int
v_middle(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (sp->position(sp, ep, &rp->lno, 0, P_MIDDLE));
}

/*
 * v_bottom -- [count]L
 *	Move to the first non-blank character of the line count from
 *	the bottom of the screen, 1 by default.
 */
int
v_bottom(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (sp->position(sp, ep, &rp->lno,
	    F_ISSET(vp, VC_C1SET) ? vp->count : 1, P_BOTTOM));
}

/*
 * v_up -- [count]^P, [count]k, [count]-
 *	Move up by lines.
 */
int
v_up(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;

	lno = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	if (fm->lno <= lno) {
		v_sof(sp, fm);
		return (1);
	}
	rp->lno = fm->lno - lno;
	return (0);
}

/*
 * v_down -- [count]^J, [count]^N, [count]j, [count]^M, [count]+
 *	Move down by lines.
 */
int
v_down(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t len;

	lno = fm->lno + (F_ISSET(vp, VC_C1SET) ? vp->count : 1);

	if (file_gline(sp, ep, lno, &len) == NULL) {
		v_eof(sp, ep, fm);
		return (1);
	}
	rp->lno = lno;
	rp->cno = len ? fm->cno > len - 1 ? len - 1 : fm->cno : 0;
	return (0);
}

/*
 * The historic vi had a problem in that all movements were by physical
 * lines, not by logical, or screen lines.  Arguments can be made that this
 * is the right thing to do.  For example, single line movements, such as
 * 'j' or 'k', should probably work on physical lines.  Commands like "dj",
 * or "j.", where '.' is a change command, make more sense for physical lines
 * than they do logical lines.  The arguments, however, don't apply to
 * scrolling commands like ^D and ^F -- if the window is fairly small, using
 * physical lines can result in a half-page scroll repainting the entire
 * screen, which is not what the user wanted.  This implementation does the
 * scrolling (^B, ^D, ^F, ^U), ^Y and ^E commands using logical lines, not
 * physical.
 */

/*
 * v_hpageup -- [count]^U
 *	Page up half screens.
 */
int
v_hpageup(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/* 
	 * Half screens always succeed unless already at SOF.  Half screens
	 * set the scroll value, even if the command ultimately failed, in
	 * historic vi.  It's probably a don't care.
	 */
	if (F_ISSET(vp, VC_C1SET))
		O_VAL(sp, O_SCROLL) = vp->count;
	else
		vp->count = O_VAL(sp, O_SCROLL);

	return (sp->down(sp, ep, rp, (recno_t)O_VAL(sp, O_SCROLL), 1));
}

/*
 * v_hpagedown -- [count]^D
 *	Page down half screens.
 */
int
v_hpagedown(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/* 
	 * Half screens always succeed unless already at EOF.  Half screens
	 * set the scroll value, even if the command ultimately failed, in
	 * historic vi.  It's probably a don't care.
	 */
	if (F_ISSET(vp, VC_C1SET))
		O_VAL(sp, O_SCROLL) = vp->count;
	else
		vp->count = O_VAL(sp, O_SCROLL);

	return (sp->up(sp, ep, rp, (recno_t)O_VAL(sp, O_SCROLL), 1));
}

/*
 * v_pageup -- [count]^B
 *	Page up full screens.
 */
int
v_pageup(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t count;

	/* Calculation from POSIX 1003.2/D8. */
	count = (F_ISSET(vp, VC_C1SET) ? vp->count : 1) * (sp->t_rows - 1);
	return (sp->down(sp, ep, rp, count, 1));
}

/*
 * v_pagedown -- [count]^F
 *	Page down full screens.
 */
int
v_pagedown(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t count;

	/* Calculation from POSIX 1003.2/D8. */
	count = (F_ISSET(vp, VC_C1SET) ? vp->count : 1) * (sp->t_rows - 1);
	return (sp->up(sp, ep, rp, count, 1));
}

/*
 * v_lineup -- [count]^Y
 *	Page up by lines.
 */
int
v_lineup(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/*
	 * The cursor moves down, staying with its original line, unless it
	 * reaches the bottom of the screen.
	 */
	return (sp->down(sp, ep,
	    rp, F_ISSET(vp, VC_C1SET) ? vp->count : 1, 0));
}

/*
 * v_linedown -- [count]^E
 *	Page down by lines.
 */
int
v_linedown(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/*
	 * The cursor moves up, staying with its original line, unless it
	 * reaches the top of the screen.
	 */
	return (sp->up(sp, ep,
	    rp, F_ISSET(vp, VC_C1SET) ? vp->count : 1, 0));
}
