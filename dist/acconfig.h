/* $Id: acconfig.h,v 8.10 1996/03/03 16:39:47 bostic Exp $ (Berkeley) $Date: 1996/03/03 16:39:47 $ */

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef ssize_t

/* Define if you have a BSD version of curses. */
#undef HAVE_BSD_CURSES

/* Define if you have the curses(3) addnstr function. */
#undef HAVE_CURSES_ADDNSTR

/* Define if you have the curses(3) beep function. */
#undef HAVE_CURSES_BEEP

/* Define if you have the curses(3) flash function. */
#undef HAVE_CURSES_FLASH

/* Define if you have the curses(3) idlok function. */
#undef HAVE_CURSES_IDLOK

/* Define if you have the curses(3) keypad function. */
#undef HAVE_CURSES_KEYPAD

/* Define if you have the curses(3) newterm function. */
#undef HAVE_CURSES_NEWTERM

/* Define if you have the curses(3) setupterm function. */
#undef HAVE_CURSES_SETUPTERM

/* Define if you have the curses(3) tigetstr/tigetnum functions. */
#undef HAVE_CURSES_TIGETSTR

/* Define if you have the DB __hash_open call in the C library. */
#undef HAVE_DB_HASH_OPEN

/* Define if you have the chsize(2) system call. */
#undef HAVE_FTRUNCATE_CHSIZE

/* Define if you have the ftruncate(2) system call. */
#undef HAVE_FTRUNCATE_FTRUNCATE

/* Define if you have fcntl(2) style locking. */
#undef HAVE_LOCK_FCNTL

/* Define if you have flock(2) style locking. */
#undef HAVE_LOCK_FLOCK

/* Define if you want to compile in the Perl interpreter. */
#undef HAVE_PERL_INTERP

/* Define if you have the Berkeley style revoke(2) system call. */
#undef HAVE_REVOKE

/* Define if you have the System V style pty calls. */
#undef HAVE_SYS5_PTY

/* Define if you want to compile in the Tcl interpreter. */
#undef HAVE_TCL_INTERP

/* Define if your sprintf returns a pointer, not a length. */
#undef SPRINTF_RET_CHARPNT

/* @BOTTOM@ */

#include "port.h"
