/***************************************************************************

  capconf.h

   This is a data structure used to determine capacity and conflict
   misses on the fly, as it is not possible to use traditional
   trace-driven methods for such determination in ILP processors
   (because of speculation and reordering).

   ***********************************************************************/
/*****************************************************************************/
/* This file is part of the RSIM Simulator.                                  */
/*                                                                           */
/******************************************************************************/
/* University of Illinois/NCSA Open Source License                            */
/*                                                                            */
/* Copyright (c) 2002 The Board of Trustees of the University of Illinois and */
/* William Marsh Rice University                                              */
/*                                                                            */
/* All rights reserved.                                                       */
/*                                                                            */
/* Developed by: Professor Sarita Adve's RSIM research group                  */
/*               University of Illinois at Urbana-Champaign and William Marsh */
/*                 Rice University                                            */
/*               http://www.cs.uiuc.edu/rsim and                              */
/*                 http://www.ece.rice.edu/~rsim/dist.html                    */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal with the Software without restriction, including without           */
/* limitation the rights to use, copy, modify, merge, publish, distribute,    */
/* sublicense, and/or sell copies of the Software, and to permit persons to   */
/* whom the Software is furnished to do so, subject to the following          */
/* conditions:                                                                */
/*                                                                            */
/*     * Redistributions of source code must retain the above copyright       */
/* notice, this list of conditions and the following disclaimers.             */
/*                                                                            */
/*     * Redistributions in binary form must reproduce the above copyright    */
/* notice, this list of conditions and the following disclaimers in the       */
/* documentation and/or other materials provided with the distribution.       */
/*                                                                            */
/*     * Neither the names of Professor Sarita Adve's RSIM research group,    */
/* the University of Illinois at Urbana-Champaign, William Marsh Rice         */
/* University, nor the names of its contributors may be used to endorse or    */
/* promote products derived from this Software without specific prior written */
/* permission.                                                                */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    */
/* THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR  */
/* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,      */
/* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR      */
/* OTHER DEALINGS WITH THE SOFTWARE.                                          */
/******************************************************************************/


#ifndef _capconf_h_
#define _capconf_h_ 1

#include "MemSys/stats.h"

#ifdef _LANGUAGE_C_PLUS_PLUS
#ifndef __cplusplus
#define __cplusplus
#endif
#endif

/*****************************************************************************/
/*********************** CapConfDetector structure definition ****************/
/*   The structure consists of two components -- a circular array of         */
/*   fixed size (equal to the number of lines in the cache) and a hash	     */
/*   table. The values stored in the structure are the "tags" for each	     */
/*   line. On an insert, the structure first checks the hash table to	     */
/*   make sure that that line isn't already being tracked. If it is, the     */
/*   structure does not insert it again. If it is not tracked, the	     */
/*   structure puts it into the sequentially next position in the	     */
/*   circular array (possibly overwriting an old value), and also into	     */
/*   the hash table. If an old value was overwritten, the structure must     */
/*   remove that value from the hash table also.			     */
/*   									     */
/*   On a query (miss), the structure sees if that element is available	     */
/*   in the hash table. If it is, that means that this line was in the	     */
/*   cache some time ago, and we have not yet swamped the cache with	     */
/*   other data in that time. Thus, this is a conflict miss. If the line     */
/*   is no longer in the structure, that means that at least Z new lines     */
/*   have been brought into the structure, where Z is the number of	     */
/*   lines available in the cache. Thus, this can be considered a	     */
/*   capacity miss.							     */
/*****************************************************************************/

#ifdef __cplusplus
#include "hash.h"
#include "circq.h"
struct CapConfDetector
{
  HashTable<unsigned,unsigned> lines_hash;
  circq<unsigned> lines_circq;
  int lines;
  CapConfDetector(int);
};
extern "C"
{
#else
  struct CapConfDetector;
#endif
  struct CapConfDetector *NewCapConfDetector(int);
  enum CacheMissType CCD_InsertNewCacheLine(struct CapConfDetector *,int);
#ifdef __cplusplus
}
#endif

#endif
