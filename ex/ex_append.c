/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_append.c,v 9.10 1995/02/01 12:29:32 bostic Exp $ (Berkeley) $Date: 1995/02/01 12:29:32 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "../sex/sex_screen.h"

enum which {APPEND, CHANGE, INSERT};

static int aci __P((SCR *, EXCMDARG *, enum which));

/*
 * ex_append -- :[line] a[ppend][!]
 *	Append one or more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
int
ex_append(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	return (aci(sp, cmdp, APPEND));
}

/*
 * ex_change -- :[line[,line]] c[hange][!] [count]
 *	Change one or more lines to the input text.
 */
int
ex_change(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	return (aci(sp, cmdp, CHANGE));
}

/*
 * ex_insert -- :[line] i[nsert][!]
 *	Insert one or more lines of new text before the specified line,
 *	or the current line if no address is specified.
 */
int
ex_insert(sp, cmdp)
	SCR *sp;
	EXCMDARG *cmdp;
{
	return (aci(sp, cmdp, INSERT));
}

static int
aci(sp, cmdp, cmd)
	SCR *sp;
	EXCMDARG *cmdp;
	enum which cmd;
{
	MARK m;
	TEXTH *sv_tiqp, tiq;
	TEXT *tp;
	struct termios ts;
	size_t len;
	u_int flags;
	int rval;
	char *p, *t;

	NEEDFILE(sp, cmdp->cmd);

	rval = 0;
	if (F_ISSET(sp, S_GLOBAL)) {
		if (cmdp->argc == 0 || cmdp->argv[0]->len == 0)
			return (0);
	} else {
		/*
		 * Set input flags; the ! flag turns off autoindent for append,
		 * change and insert.
		 */
		LF_INIT(TXT_DOTTERM | TXT_NUMBER);
		if (!F_ISSET(cmdp, E_FORCE) && O_ISSET(sp, O_AUTOINDENT))
			LF_SET(TXT_AUTOINDENT);
		if (O_ISSET(sp, O_BEAUTIFY))
			LF_SET(TXT_BEAUTIFY);

		/* The screen is now dirty. */
		F_SET(sp, S_SCR_EXWROTE);

		/*
		 * If this code is called by vi, the screen TEXTH structure
		 * (sp->tiqp) may already be in use, e.g. ":append|s/abc/ABC/"
		 * would fail as we're only halfway through the line when the
		 * append code fires.  Use the local structure instead.
		 *
		 * If this code is called by vi, we want to reset the terminal
		 * and use ex's line get routine.  It actually works fine if we
		 * use vi's get routine, but it doesn't look as nice.  Maybe if
		 * we had a separate window or something, but getting a line at
		 * a time looks awkward.
		 */
		if (F_ISSET(sp, S_VI)) {
			memset(&tiq, 0, sizeof(TEXTH));
			CIRCLEQ_INIT(&tiq);
			sv_tiqp = sp->tiqp;
			sp->tiqp = &tiq;

			if (sex_tsetup(sp, &ts))
				return (1);
			(void)write(STDOUT_FILENO, "\n", 1);
		}

		/* Set the line number, so that autoindent works correctly. */
		sp->lno = cmdp->addr1.lno;

		if (sex_get(sp, sp->tiqp, 0, flags) != INP_OK)
			goto err;
	}

	/*
	 * If doing a change, replace lines for as long as possible.  Then,
	 * append more lines or delete remaining lines.  Changes to an empty
	 * file are just appends, and inserts are the same as appends to the
	 * previous line.
	 *
	 * !!!
	 * Adjust the current line number for the commands to match historic
	 * practice if the user doesn't enter anything, and set the address
	 * to which we'll append.  This is safe because an address of 0 is
	 * illegal for change and insert.
	 */
	m = cmdp->addr1;
	switch (cmd) {
	case INSERT:
		if (m.lno != 0)
			--m.lno;
		/* FALLTHROUGH */
	case APPEND:
		if (sp->lno == 0)
			sp->lno = 1;
		break;
	case CHANGE:
		if (m.lno != 0)
			--m.lno;
		if (sp->lno != 1)
			--sp->lno;
		break;
	}

	/*
	 * !!!
	 * Cut into the unnamed buffer, if the file isn't empty.
	 */
	if (cmd == CHANGE && cmdp->addr1.lno != 0 &&
	    (cut(sp, NULL, &cmdp->addr1, &cmdp->addr2, CUT_LINEMODE) ||
	    delete(sp, &cmdp->addr1, &cmdp->addr2, 1)))
		goto err;

	/*
	 * !!!
	 * Input lines specified on the ex command line (see the comment in
	 * ex.c:ex_cmd()) lines are separated by <backslash><newline> pairs.
	 * If there is a trailing delimiter pair an empty line is inserted.
	 * There may also be a leading delimiter pair, which is ignored unless
	 * it's also a trailing delimiter pair.
	 */
	if (cmdp->argc != 0)
		for (p = cmdp->argv[0]->bp,
		    len = cmdp->argv[0]->len; len > 0; p = t) {
			for (t = p; len > 0; ++t, --len)
				if (t[0] == '\\' && len > 1 && t[1] == '\n')
					break;
			if (len == 0 || t != p) {
				if (file_aline(sp, 1, m.lno, p, t - p))
					goto err;
				sp->lno = ++m.lno;
			}
			if (len != 0) {
				t += 2;
				if ((len -= 2) == 0) {
					if (file_aline(sp, 1, m.lno, "", 0))
						goto err;
					sp->lno = ++m.lno;
				}
			}
		}

	if (!F_ISSET(sp, S_GLOBAL))
		for (tp = sp->tiqp->cqh_first;
		    tp != (TEXT *)sp->tiqp; tp = tp->q.cqe_next) {
			if (file_aline(sp, 1, m.lno, tp->lb, tp->len))
				goto err;
			sp->lno = ++m.lno;
		}

	if (0) {
err:		rval = 1;
	}

	if (!F_ISSET(sp, S_GLOBAL) && F_ISSET(sp, S_VI)) {
		sp->tiqp = sv_tiqp;
		text_lfree(&tiq);

		/* Reset the terminal state. */
		if (sex_tteardown(sp, &ts))
			rval = 1;
		F_SET(sp, S_SCR_REFRESH);
	}
	return (rval);
}
