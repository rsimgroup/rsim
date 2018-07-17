/*****************************************************************************//* This file is part of the RSIM Applications Library.                       */
/*                                                                           */
/************************ LICENSE TERMS AND CONDITIONS ***********************/
/*                                                                           */
/*  Copyright Notice                                                         */
/*       1997 Rice University                                                */
/*                                                                           */
/*  1. The "Software", below, refers to RSIM (Rice Simulator for ILP         */
/*  Multiprocessors) version 1.0 and includes the RSIM Simulator, the        */
/*  RSIM Applications Library, Example Applications ported to RSIM,          */
/*  and RSIM Utilities.  Each licensee is addressed as "you" or              */
/*  "Licensee."                                                              */
/*                                                                           */
/*  2. Rice University is copyright holder for the RSIM Simulator and RSIM   */
/*  Utilities. The copyright holders reserve all rights except those         */
/*  expressly granted to the Licensee herein.                                */
/*                                                                           */
/*  3. Permission to use, copy, and modify the RSIM Simulator and RSIM       */
/*  Utilities for any non-commercial purpose and without fee is hereby       */
/*  granted provided that the above copyright notice appears in all copies   */
/*  (verbatim or modified) and that both that copyright notice and this      */
/*  permission notice appear in supporting documentation. All other uses,    */
/*  including redistribution in whole or in part, are forbidden without      */
/*  prior written permission.                                                */
/*                                                                           */
/*  4. The RSIM Applications Library is free software; you can               */
/*  redistribute it and/or modify it under the terms of the GNU Library      */
/*  General Public License as published by the Free Software Foundation;     */
/*  either version 2 of the License, or (at your option) any later           */
/*  version.                                                                 */
/*                                                                           */
/*  The Library is distributed in the hope that it will be useful, but       */
/*  WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU         */
/*  Library General Public License for more details.                         */
/*                                                                           */
/*  You should have received a copy of the GNU Library General Public        */
/*  License along with the Library; if not, write to the Free Software       */
/*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,    */
/*  USA.                                                                     */
/*                                                                           */
/*  5. LICENSEE AGREES THAT THE EXPORT OF GOODS AND/OR TECHNICAL DATA FROM   */
/*  THE UNITED STATES MAY REQUIRE SOME FORM OF EXPORT CONTROL LICENSE FROM   */
/*  THE U.S.  GOVERNMENT AND THAT FAILURE TO OBTAIN SUCH EXPORT CONTROL      */
/*  LICENSE MAY RESULT IN CRIMINAL LIABILITY UNDER U.S. LAWS.                */
/*                                                                           */
/*  6. RICE UNIVERSITY NOR ANY OF THEIR EMPLOYEES MAKE ANY WARRANTY,         */
/*  EXPRESS OR IMPLIED, OR ASSUME ANY LEGAL LIABILITY OR RESPONSIBILITY      */
/*  FOR THE ACCURACY, COMPLETENESS, OR USEFULNESS OF ANY INFORMATION,        */
/*  APPARATUS, PRODUCT, OR PROCESS DISCLOSED AND COVERED BY A LICENSE        */
/*  GRANTED UNDER THIS LICENSE AGREEMENT, OR REPRESENT THAT ITS USE WOULD    */
/*  NOT INFRINGE PRIVATELY OWNED RIGHTS.                                     */
/*                                                                           */
/*  7. IN NO EVENT WILL RICE UNIVERSITY BE LIABLE FOR ANY DAMAGES,           */
/*  INCLUDING DIRECT, INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES          */
/*  RESULTING FROM EXERCISE OF THIS LICENSE AGREEMENT OR THE USE OF THE      */
/*  LICENSED SOFTWARE.                                                       */
/*                                                                           */
/*****************************************************************************/


/* includes exit and a few other simple functions that are one level
   away from traps or other simple functions */

#define MAX_ATEXITS 50
static int ATEXITCTR = 0;
#define MAX_ATFORKS 50
static int ATFORKCTR = 0;

static void (*atexitarr[MAX_ATEXITS])(void);
static void (*atforkarr[MAX_ATFORKS])(void);

int atexit(void (*func)(void))
{
  if (ATEXITCTR >= MAX_ATEXITS)
    return -1;
  atexitarr[ATEXITCTR++] = func;
  return 0;
}
int atfork(void (*func)(void))
{
  if (ATFORKCTR >= MAX_ATFORKS)
    return -1;
  atforkarr[ATFORKCTR++] = func;
  return 0;
}


void exit(int status)
{
  int i;
  for (i=ATEXITCTR-1; i>=0; i--)
    {
      (*(atexitarr[i]))();
    }
  _exit(status);
}

int fork()
{
  int i;
  for (i=ATFORKCTR-1; i>=0; i--)
    {
      (*(atforkarr[i]))();
    }
  return _fork();
}

int _mutex_lock()
{
  return 0;
}
int mutex_lock()
{
  return 0;
}
int _mutex_unlock()
{
  return 0;
}
int mutex_unlock()
{
  return 0;
}
int brk(unsigned endds)
{
  unsigned c;
  int i;
  c = (unsigned)sbrk(0);
  i = endds - c;
  if (i >= 0)
    sbrk(i);
  return 0;
}

int _brk(unsigned endds)
{
  unsigned c;
  int i;
  c = (unsigned)sbrk(0);
  i = endds - c;
  if (i >= 0)
    sbrk(i);
  return 0;
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int creat(const char *path, mode_t mode)
{
  return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int _creat(const char *path, mode_t mode)
{
  return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int __creat(const char *path, mode_t mode)
{
  return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int ___errno(void)
{
  extern int errno;
  return errno;
}

void *__memccpy(void *s1, const void *s2, int c, size_t n)
{
  return memccpy(s1,s2,c,n);
}

