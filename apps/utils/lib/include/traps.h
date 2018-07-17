/*****************************************************************************/
/* This file is part of the RSIM Applications Library.                       */
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


#ifndef _RSIM_TRAPS_H_
#define _RSIM_TRAPS_H_ 1
/* extern void *sys_malloc(int);
extern int sys_free(void *); 
extern void *sys_realloc(int); */

extern void *shmalloc(int);

/* extern void exit(int); */

extern int HALT(int); /* force *** everyone *** to exit */
extern int getpagesize(void);

extern int fork(void);
extern int getpid(void);

extern int sys_bzero(void *,int);
extern int sysclocks();

#define ACQ_MEMBAR asm("membar #LoadLoad|#LoadStore");
#define BARRIER_MEMBAR asm("membar #LoadLoad|#LoadStore|#StoreStore");
#define REL_MEMBAR asm("membar #LoadStore|#StoreStore");

#define START_USR1 asm("unimp 0x1001");
#define START_USR2 asm("unimp 0x1002");
#define START_USR3 asm("unimp 0x1003");
#define START_USR4 asm("unimp 0x1004");
#define START_USR5 asm("unimp 0x1005");
#define START_USR6 asm("unimp 0x1006");
#define START_USR7 asm("unimp 0x1007");
#define START_USR8 asm("unimp 0x1008");
#define START_USR9 asm("unimp 0x1009");
#define START_BAR  asm("unimp 0x100a");
#define START_SPIN asm("unimp 0x100b");
#define START_ACQ  asm("unimp 0x100c");
#define START_REL  asm("unimp 0x100d");
#define END_AGG    asm("unimp 0x1000");
#define END_USR1 END_AGG
#define END_USR2 END_AGG
#define END_USR3 END_AGG
#define END_USR4 END_AGG
#define END_USR5 END_AGG
#define END_USR6 END_AGG
#define END_USR7 END_AGG
#define END_USR8 END_AGG
#define END_USR9 END_AGG
#define END_BAR  END_AGG
#define END_SPIN END_AGG

#define MEMSYS_OFF asm("membar #MemIssue"); asm ("ld [%fp],%g0"); asm("unimp 0x21");
#define MEMSYS_ON asm("membar #MemIssue"); asm ("ld [%fp],%g0"); asm("unimp 0x22");
#define RSIM_OFF asm("unimp 0x32");
#define RSIM_ON asm("unimp 0x33");

extern int GET_L2CACHELINE_SIZE(void);
extern void StatReportAll(void);
extern void StatClearAll(void);
extern void AssociateAddrNode(void *start,void *end, int node, char *);
extern void AssociateAddrCohe(void *start,void *end, int cohe, char *);

/* the following are not really traps, but we'll put them here anyway. */

extern int SpecialInitInput(void *addr,int sz, char *filename);
extern int SpecialInitOutput(void *addr,int sz, char *filename);
/* these return nonzero on failure */


#endif
