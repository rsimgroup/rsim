/* This file is part of the RSIM Applications Library and was modified
from the GNU C library as follows:

Separate initialization calls for stdio streams
Testing against the UNIX stdio streams to determine if the user
is using those structures instead. */  
/* Copyright (C) 1991 Free Software Foundation, Inc.
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

/* Open a new stream on a given system file descriptor.  */
FILE *
DEFUN(fdopen, (fd, mode), int fd AND CONST char *mode)
{
  /* This function provided by VSP 7/4/97:
     Note that this is a use at your own risk function -- if
     the fd passed in doesn't match the correct mode, it'll
     behave randomly, rather than simply returning NULL and
     quitting */
  FILE *stream;
  __io_mode m;

  /* NOTE: THIS FUNCTION MUST BE ABLE TO WORK EVEN IF STDIO
     HAS NOT BEEN INITED, since this is called in init... */
  if (!__getmode (mode, &m))
    return NULL;

  stream = __newstream ();
  if (stream == NULL)
    return NULL;

  stream->__cookie = (PTR) fd;
  stream->__mode = m;

  return stream;
}
