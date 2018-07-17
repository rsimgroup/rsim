/* This file is part of the RSIM Applications Library and was modified
from the GNU C library as follows:

Separate initialization calls for stdio streams
Testing against the UNIX stdio streams to determine if the user
is using those structures instead.
Added shutdown and cleanup routines.

*/  
/* Definitions of global stdio data structures.

Copyright (C) 1991, 1992, 1993, 1994 Free Software Foundation, Inc.
This file is part of the GNU C Library.

The GNU C Library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The GNU C Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with the GNU C Library; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 675 Mass Ave,
Cambridge, MA 02139, USA.  */

#include <ansidecl.h>
#include <errno.h>
#include <stdio.h>

static FILE stdinf, stdoutf, stderrf;
FILE *stdin = &stdinf;
FILE *stdout= &stdoutf;
FILE *stderr= &stderrf;

/* Pointer to the first stream in the list.  */
FILE *__stdio_head = &stdinf;
int __stdio_inited = 0;

static void shutdownfiles(void)
{
  fflush(NULL);
  /* fclose(NULL); */ /* don't let this happen unless we're sure everyone is done */
}

static void cleanupfiles(void) /* this is needed since we don't support PTHREADS style address space... */
{
  fflush(NULL);
}

void __init_stdio (void)
{
  __io_mode m;
  stdin->__cookie = (PTR)0; /* filedes is 0 */
  if (!__getmode("r",&m))
    abort(-1);
  stdin->__mode = m;
  stdin->__magic = _IOMAGIC;
  stdin->__offset = (fpos_t) -1;
  stdin->__target = (fpos_t) -1;
  stdin->__next = stdout;

  stdout->__cookie = (PTR)1; /* filedes is 1 */
  if (!__getmode("w",&m))
    abort(-1);
  stdout->__mode = m;
  stdout->__magic = _IOMAGIC;
  stdout->__offset = (fpos_t) -1;
  stdout->__target = (fpos_t) -1;
  stdout->__next = stderr;

  stderr->__cookie = (PTR)2; /* filedes is 2 */
  if (!__getmode("w",&m))
    abort(-1);
  stderr->__mode = m;
  stderr->__magic = _IOMAGIC;
  stderr->__offset = (fpos_t) -1;
  stderr->__target = (fpos_t) -1;
  stderr->__next = NULL;

  /* let seen be 0 so that these things will get buffers assigned
     to them and so forth... */
  
#if 0
  stdin=fdopen(0,"r");
  stdout=fdopen(1,"w");
  stderr=fdopen(2,"w");
#endif
  __stdio_inited = 1;
  atexit(shutdownfiles);
  atfork(cleanupfiles);
}

#if 0
  inline void init (FILE **streamptr, int fd)
    {
      /* We want to use the existing FILE object if one has been allocated.
	 (This will only be the case if our image came from something like
	 Emacs's unexec, where we were called in the first run.)  */
      FILE *s = *streamptr ?: __newstream ();
      struct hurd_fd *d = _hurd_fd_get (fd);
      if (d == NULL)
	/* There is no file descriptor allocated.  We want the standard
	   streams to always refer to their standard file descriptors, even
	   if those descriptors are not set up until later.  So allocate
	   the descriptor structure with no ports and store it in the
	   stream.  Operations will fail until ports are installed in the
	   file descriptor.  */
	d = _hurd_alloc_fd (NULL, fd);
      if (d)
	__spin_unlock (&d->port.lock);
      if (s)
	s->__cookie = d;
      *streamptr = s;
    }
#define S(NAME, FD, MODE) \
  init (&NAME, FD); if (NAME) NAME->__mode.__##MODE = 1;

  S (stdin, STDIN_FILENO, read);
  S (stdout, STDOUT_FILENO, write);
  S (stderr, STDERR_FILENO, write);

#undef S

  if (stderr)
    stderr->__userbuf = 1;	/* stderr is always unbuffered.  */

  (void) &init_stdio;		/* Avoid "defined but not used" warning.  */
}
text_set_element (_hurd_fd_subinit, init_stdio);

/* This function MUST be in this file!
   This is because we want _cleanup to go into the __libc_atexit set
   when any stdio code is used (and to use any stdio code, one must reference
   something defined in this file), and since only local symbols can be made
   set elements, having the set element stab entry here and _cleanup elsewhere
   loses; and having them both elsewhere loses because there is no reference
   to cause _cleanup to be linked in.  */

void
DEFUN_VOID(_cleanup)
{
  (void) fclose ((FILE *) NULL);
}
text_set_element (__libc_atexit, _cleanup);
#endif
