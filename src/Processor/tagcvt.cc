/*

  tagcvt.cc

  Code for member functions of tag-to-instance converter class.

  */
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


#include "Processor/tagcvt.h"
#include "Processor/instance.h"
#include "Processor/state.h"
#include "Processor/FastNews.h"
#include "Processor/simio.h"
#include <stdlib.h>

/*************************************************************************/
/* AddtoTagConverter : initializes the tag-to-instance lookup table for  */
/*                   : a new instance                                    */
/*************************************************************************/

int AddtoTagConverter(int taag, instance *instt, state *proc)
{
#ifdef COREFILE
  if(proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "Adding tag %d to tag converter\n",taag);
#endif
  TagtoInst *tptr = NewTagtoInst(taag,instt,proc);
  int res=proc->tag_cvt->Insert(tptr);
  if (res == 0)
    {
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile,"FAILED TO ADD TAG %d to tag converter\n",taag);
#endif
      fprintf(simout,"FAILED TO ADD TAG %d to tag converter\n",taag);
    }
  return res;  
}

/*************************************************************************/
/* tag_inst_cmp: a helper function for searching the converter           */
/*************************************************************************/
static int tag_inst_cmp(TagtoInst *const& a, int b)
{
  return a->tag - b;
}

/*************************************************************************/
/* convert_tag_to_inst: finds the instance corresponding to a given tag  */
/*************************************************************************/

instance *convert_tag_to_inst(int tag, state *proc)
{
  TagtoInst *tmpptr;
  int jnk;
  int found = proc->tag_cvt->Search(tag,tmpptr,jnk,tag_inst_cmp);
  
  if (found)
    return (tmpptr->inst);
  else
    return (NULL);
}

/*************************************************************************/
/* GetTagcount: See the counter at some specific element in the tag      */
/*              converter. (used in graduate to make sure that           */
/*              both active list entries for an instruction have passed  */
/*              through, etc.)                                           */
/*************************************************************************/

int GetTagcount(int tag,state *proc)
{
  TagtoInst *tmpptr;
  int jnk;
  int found = proc->tag_cvt->Search(tag,tmpptr,jnk,tag_inst_cmp);
  
  if (found)
    return tmpptr->count;
  else
    return -1;
}


/*************************************************************************/
/* UpdateTagcount: Increment the counter for some element in the tag     */
/*                 converter                                             */
/*************************************************************************/

int UpdateTagcount(int tag,state *proc)
{
  TagtoInst *tmpptr;
  int jnk;
  int found = proc->tag_cvt->Search(tag,tmpptr,jnk,tag_inst_cmp);
  
  if (found)
    return ++tmpptr->count;
  else
    return -1;
}


/*************************************************************************/
/* UpdateTaghead: Increment the counter for the head element of the tag  */
/*                converter                                              */
/*************************************************************************/

int UpdateTaghead(int tag, state *proc)
{
  TagtoInst *junk;
  if (!proc->tag_cvt->PeekHead(junk) || junk->tag != tag)
    {
      fprintf(simerr,"UTh: Something stuck in the tag converter, tag %d, wanted tag %d\n",junk->tag,tag);
      exit(1);
    }
  return ++junk->count;
}

/*************************************************************************/
/* UpdateTagtail: Increment the counter at the end of the tag converter  */
/*************************************************************************/

int UpdateTagtail(int tag, state *proc)
{
  TagtoInst *junk;
  if (!proc->tag_cvt->PeekTail(junk) || junk->tag != tag)
    {
      fprintf(simerr,"UTt: Something extra in the tag converter, tag %d, wanted tag %d\n",junk->tag,tag);
      exit(1);
    }
  return ++junk->count;
}


/*************************************************************************/
/* TagCvtHead: Get instance at the head of the tag converter             */
/*************************************************************************/

instance *TagCvtHead(int taag, state *proc)
{
  TagtoInst *junk;
  
  if (!proc->tag_cvt->PeekHead(junk) || junk->tag != taag)
    {
      fprintf(simerr,"TCH: Something stuck in the tag converter, tag %d, wanted tag %d\n",junk->tag,taag);
      exit(1);
    }
  return junk->inst;
}

/*************************************************************************/
/* GetTagCvtByPosn: Get the instance at a specific index into the        */
/*                  tag converter                                        */
/*************************************************************************/

instance *GetTagCvtByPosn(int taag, int index, state *proc)
{
  TagtoInst *junk;
  
  if (!proc->tag_cvt->PeekElt(junk,index) || junk->tag != taag)
    {
      fprintf(simerr,"GTCBP: Something incorrect in the tag converter, tag %d, wanted tag %d\n",junk->tag,taag);
      exit(1);
    }
  return junk->inst;
}

/*************************************************************************/
/* GetHeadInst: Get instance at head of tag converter                    */
/*************************************************************************/

instance *GetHeadInst(state *proc)
{
  TagtoInst *junk;
  
  if (!proc->tag_cvt->PeekHead(junk))
    return NULL;
  return junk->inst;
}

/*************************************************************************/
/* TagCvtTail: Get instance at tail of tag converter                     */
/*************************************************************************/

instance *TagCvtTail(int taag, state *proc)
{
  TagtoInst *junk;
  
  if (!proc->tag_cvt->PeekTail(junk) || junk->tag != taag)
    {
      fprintf(simerr,"TCT: Something extra in the tag converter, tag %d, wanted tag %d\n",junk->tag,taag);
      exit(1);
    }
  return junk->inst;
}

/*************************************************************************/
/* GraduateTagConverter: Remove an element from head of tag converter    */
/*************************************************************************/

void GraduateTagConverter(int taag, state *proc)
{
  TagtoInst *junk;
  
  if (!proc->tag_cvt->Delete(junk) || junk->tag != taag)
    { 
      fprintf(simerr,"GTC: Something stuck in the tag converter, tag %d, wanted tag %d\n",junk->tag,taag);
      exit(1);
    }
  DeleteTagtoInst(junk,proc);
}

/*************************************************************************/
/* FlushTagConverter: Remove an element from end of tag converter        */
/*************************************************************************/

void FlushTagConverter(int taag, state *proc)
{
  TagtoInst *junk;
  
  if (!proc->tag_cvt->DeleteFromTail(junk) || junk->tag != taag)
    {
      fprintf(simerr,"FTC: Something extra in the tag converter, tag %d, wanted tag %d\n",junk->tag,taag);
      exit(1);
    }
  DeleteTagtoInst(junk,proc);
}
