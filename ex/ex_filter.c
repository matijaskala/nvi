/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1994, 1995
 *	Keith Bostic.  All rights reserved.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "$Id: ex_filter.c,v 10.11 1995/09/25 11:11:31 bostic Exp $ (Berkeley) $Date: 1995/09/25 11:11:31 $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <bitstring.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"

static int filter_ldisplay __P((SCR *, FILE *));

/*
 * filtercmd --
 *	Run a range of lines through a filter utility and optionally
 *	replace the original text with the stdout/stderr output of
 *	the utility.
 *
 * PUBLIC: int filtercmd __P((SCR *, 
 * PUBLIC:    EXCMD *, MARK *, MARK *, MARK *, char *, enum filtertype));
 */
int
filtercmd(sp, cmdp, fm, tm, rp, cmd, ftype)
	SCR *sp;
	EXCMD *cmdp;
	MARK *fm, *tm, *rp;
	char *cmd;
	enum filtertype ftype;
{
	FILE *ifp, *ofp;
	pid_t parent_writer_pid, utility_pid;
	recno_t nread;
	int input[2], output[2], nf, rval;
	char *name, *p;

	/* Set return cursor position; guard against a line number of zero. */
	*rp = *fm;
	if (fm->lno == 0)
		rp->lno = 1;

	/*
	 * There are three different processes running through this code.
	 * They are the utility, the parent-writer and the parent-reader.
	 * The parent-writer is the process that writes from the file to
	 * the utility, the parent reader is the process that reads from
	 * the utility.
	 *
	 * Input and output are named from the utility's point of view.
	 * The utility reads from input[0] and the parent(s) write to
	 * input[1].  The parent(s) read from output[0] and the utility
	 * writes to output[1].
	 *
	 * !!!
	 * Historically, in the FILTER_READ case, the utility reads from
	 * the terminal (e.g. :r! cat works).  Otherwise open up utility
	 * input pipe.
	 */
	ofp = NULL;
	input[0] = input[1] = output[0] = output[1] = -1;
	if (ftype != FILTER_RBANG && ftype != FILTER_READ && pipe(input) < 0) {
		msgq(sp, M_SYSERR, "pipe");
		goto err;
	}

	/* Open up utility output pipe. */
	if (pipe(output) < 0) {
		msgq(sp, M_SYSERR, "pipe");
		goto err;
	}
	if ((ofp = fdopen(output[0], "r")) == NULL) {
		msgq(sp, M_SYSERR, "fdopen");
		goto err;
	}

	/*
	 * If doing a filter read, switch into ex canonical mode.  The reason
	 * to restore the original terminal modes for read filters is so that
	 * users can do things like ":r! cat /dev/tty".
	 */
	if (ftype == FILTER_READ && F_ISSET(sp, S_VI)) {
		if (sp->gp->scr_screen(sp, S_EX)) {
			ex_message(sp, cmdp->cmd->name, EXM_NOCANON);
			return (1);
		}
		F_SET(sp, S_SCREEN_READY);
	}

	/* Fork off the utility process. */
	switch (utility_pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "vfork");
err:		if (input[0] != -1)
			(void)close(input[0]);
		if (input[1] != -1)
			(void)close(input[1]);
		if (ofp != NULL)
			(void)fclose(ofp);
		else if (output[0] != -1)
			(void)close(output[0]);
		if (output[1] != -1)
			(void)close(output[1]);
		return (1);
	case 0:				/* Utility. */
		/*
		 * Redirect stdin from the read end of the input pipe, and
		 * redirect stdout/stderr to the write end of the output pipe.
		 *
		 * !!!
		 * Historically, ex only directed stdout into the input pipe,
		 * letting stderr come out on the terminal as usual.  Vi did
		 * not, directing both stdout and stderr into the input pipe.
		 * We match that practice in both ex and vi for consistency.
		 */
		if (input[0] != -1)
			(void)dup2(input[0], STDIN_FILENO);
		(void)dup2(output[1], STDOUT_FILENO);
		(void)dup2(output[1], STDERR_FILENO);

		/* Close the utility's file descriptors. */
		if (input[0] != -1)
			(void)close(input[0]);
		if (input[1] != -1)
			(void)close(input[1]);
		(void)close(output[0]);
		(void)close(output[1]);

		if ((name = strrchr(O_STR(sp, O_SHELL), '/')) == NULL)
			name = O_STR(sp, O_SHELL);
		else
			++name;

		execl(O_STR(sp, O_SHELL), name, "-c", cmd, NULL);
		p = msg_print(sp, O_STR(sp, O_SHELL), &nf);
		msgq(sp, M_SYSERR, "execl: %s", p);
		if (nf)
			FREE_SPACE(sp, p, 0);
		_exit (127);
		/* NOTREACHED */
	default:			/* Parent-reader, parent-writer. */
		/* Close the pipe ends neither parent will use. */
		if (input[0] != -1)
			(void)close(input[0]);
		(void)close(output[1]);
		break;
	}

	/*
	 * FILTER_RBANG, FILTER_READ:
	 *
	 * Reading is the simple case -- we don't need a parent writer,
	 * so the parent reads the output from the read end of the output
	 * pipe until it finishes, then waits for the child.  Ex_readfp
	 * appends to the MARK, and closes ofp.
	 *
	 * !!!
	 * Set the return cursor to the last line read in.  Historically,
	 * this behaves differently from ":r file" command, which leaves
	 * the cursor at the first line read in.  Check to make sure that
	 * it's not past EOF because we were reading into an empty file.
	 */
	if (ftype == FILTER_RBANG || ftype == FILTER_READ) {
		rval = ex_readfp(sp, "filter", ofp, fm, &nread, 0);
		sp->rptlines[L_ADDED] += nread;
		if (fm->lno == 0)
			rp->lno = nread;
		else
			rp->lno += nread;
		goto uwait;
	}

	/*
	 * FILTER_BANG, FILTER_WRITE
	 *
	 * Here we need both a reader and a writer.  Temporary files are
	 * expensive and we'd like to avoid disk I/O.  Using pipes has the
	 * obvious starvation conditions.  It's done as follows:
	 *
	 *	fork
	 *	child
	 *		write lines out
	 *		exit
	 *	parent
	 *		FILTER_BANG:
	 *			read lines into the file
	 *			delete old lines
	 *		FILTER_WRITE
	 *			read and display lines
	 *		wait for child
	 *
	 * XXX
	 * We get away without locking the underlying database because we know
	 * that none of the records that we're reading will be modified until
	 * after we've read them.  This depends on the fact that the current
	 * B+tree implementation doesn't balance pages or similar things when
	 * it inserts new records.  When the DB code has locking, we should
	 * treat vi as if it were multiple applications sharing a database, and
	 * do the required locking.  If necessary a work-around would be to do
	 * explicit locking in the line.c:file_gline() code, based on the flag
	 * set here.
	 */
	rval = 0;
	F_SET(sp->ep, F_MULTILOCK);
	switch (parent_writer_pid = fork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "fork");
		(void)close(input[1]);
		(void)close(output[0]);
		rval = 1;
		break;
	case 0:				/* Parent-writer. */
		/*
		 * Write the selected lines to the write end of the input
		 * pipe.  This instance of ifp is closed by ex_writefp.
		 */
		(void)close(output[0]);
		if ((ifp = fdopen(input[1], "w")) == NULL)
			_exit (1);
		_exit(ex_writefp(sp, "filter", ifp, fm, tm, NULL, NULL));

		/* NOTREACHED */
	default:			/* Parent-reader. */
		(void)close(input[1]);
		if (ftype == FILTER_WRITE)
			/*
			 * Read the output from the read end of the output
			 * pipe and display it.  Filter_ldisplay closes ofp.
			 */
			rval = filter_ldisplay(sp, ofp);
		else {
			/*
			 * Read the output from the read end of the output
			 * pipe.  Ex_readfp appends to the MARK and closes
			 * ofp.
			 */
			rval = ex_readfp(sp, "filter", ofp, tm, &nread, 0);
			sp->rptlines[L_ADDED] += nread;
		}

		/* Wait for the parent-writer. */
		rval |= proc_wait(sp,
		    (long)parent_writer_pid, "parent-writer", 1);

		/* Delete any lines written to the utility. */
		if (rval == 0 && ftype == FILTER_BANG &&
		    (cut(sp, NULL, fm, tm, CUT_LINEMODE) ||
		    delete(sp, fm, tm, 1))) {
			rval = 1;
			break;
		}

		/*
		 * If the filter had no output, we may have just deleted
		 * the cursor.  Don't do any real error correction, we'll
		 * try and recover later.
		 */
		 if (rp->lno > 1 && !file_eline(sp, rp->lno))
			--rp->lno;
		break;
	}
	F_CLR(sp->ep, F_MULTILOCK);

uwait:	return (proc_wait(sp, (long)utility_pid, cmd, 0) || rval);
}

/*
 * proc_wait --
 *	Wait for one of the processes.
 *
 * !!!
 * The pid_t type varies in size from a short to a long depending on the
 * system.  It has to be cast into something or the standard promotion
 * rules get you.  I'm using a long based on the belief that nobody is
 * going to make it unsigned and it's unlikely to be a quad.
 *
 * PUBLIC: int proc_wait __P((SCR *, long, const char *, int));
 */
int
proc_wait(sp, pid, cmd, okpipe)
	SCR *sp;
	long pid;
	const char *cmd;
	int okpipe;
{
	extern const char *const sys_siglist[];
	size_t len;
	int nf, pstat;
	char *p;

	/*
	 * Wait for the utility to finish.  We can get interrupted
	 * by SIGALRM, just ignore it.
	 */
	for (;;) {
		errno = 0;
		if (waitpid((pid_t)pid, &pstat, 0) != -1)
			break;
		if (errno != EINTR) {
			msgq(sp, M_SYSERR, "waitpid");
			return (1);
		}
	}

	/*
	 * Display the utility's exit status.  Ignore SIGPIPE from the
	 * parent-writer, as that only means that the utility chose to
	 * exit before reading all of its input.
	 */
	if (WIFSIGNALED(pstat) && (!okpipe || WTERMSIG(pstat) != SIGPIPE)) {
		for (; isblank(*cmd); ++cmd);
		p = msg_print(sp, cmd, &nf);
		len = strlen(p);
		msgq(sp, M_ERR, "%.*s%s: received signal: %s%s",
		    MIN(len, 20), p, len > 20 ? " ..." : "",
		    sys_siglist[WTERMSIG(pstat)],
		    WCOREDUMP(pstat) ? "; core dumped" : "");
		if (nf)
			FREE_SPACE(sp, p, 0);
		return (1);
	}
	if (WIFEXITED(pstat) && WEXITSTATUS(pstat)) {
		for (; isblank(*cmd); ++cmd);
		p = msg_print(sp, cmd, &nf);
		len = strlen(p);
		msgq(sp, M_ERR, "%.*s%s: exited with status %d",
		    MIN(len, 20), p, len > 20 ? " ..." : "",
		    WEXITSTATUS(pstat));
		if (nf)
			FREE_SPACE(sp, p, 0);
		return (1);
	}
	return (0);
}

/*
 * filter_ldisplay --
 *	Display output from a utility.
 *
 * !!!
 * Historically, the characters were passed unmodified to the terminal.
 * We use the ex print routines to make sure they're printable.
 */
static int
filter_ldisplay(sp, fp)
	SCR *sp;
	FILE *fp;
{
	size_t len;

	EX_PRIVATE *exp;

	for (exp = EXP(sp); !ex_getline(sp, fp, &len) && !INTERRUPTED(sp);)
		if (ex_ldisplay(sp, exp->ibp, len, 0, 0))
			break;
	if (ferror(fp))
		msgq(sp, M_SYSERR, "filter read");
	(void)fclose(fp);
	return (0);
}
