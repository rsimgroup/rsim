/*
  setup_cohe.c

  This file includes functions that implement the cache-coherence
  protocol of the memory-system simulator.

  */
/*****************************************************************************/
/* This file is part of the RSIM Simulator, and is based on earlier code     */
/* from RPPT: The Rice Parallel Processing Testbed.                          */
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


#include "MemSys/cache.h"
#include "MemSys/req.h"
#include "MemSys/misc.h"
#include "MemSys/simsys.h"
#include "Processor/simio.h"


/*****************************************************************************/
/* cohe_table structure: this structure holds all the information needed for */
/* the coherence routines of the caches. This can be thought of as the state */
/* transition table for the coherence diagram, as it is indexed by the       */
/* current state and the message type and gives back the next state, as      */
/* well as information about other actions that need to take place as a      */
/* result of this transaction (such as whether or not a message needs to be  */
/* sent to lower levels of the hierarchy, and, if so, what type of message)  */
/*****************************************************************************/
   
struct cohe_table
{
  struct
  {
    struct
    {
      int valid;
      CacheLineState nxt_st;
      ReqType nxt_mod_req;
      int req_sz;
      int rep_sz;
      ReqType nxt_req;
      int nxt_req_sz;
      int noalloc;
    } sttbl[NUM_CACHE_LINE_STATES];
  } reqtbl[Req_type_max];
};

struct cohe_table Secondary_WB;
struct cohe_table Primary_WB;
struct cohe_table Primary_WT;

/*****************************************************************************/
/* AddEntryToTable: adds a new valid entry into the cache-coherence table    */
/* The user must specify the request type, the current state, the next       */
/* state, the request type used when this REQ is sent to the next module     */
/* (nxt_mod_req), the size of the request as sent to the next module, the    */
/* size of the expected reply, the request type to use when creating a new   */
/* request to send to the previous module (nxt_req; used only in replacement */
/* messages), the size of the message to the previous module, and whether or */
/* not this request allocates a line in the cache                            */
/*****************************************************************************/

static void AddEntryToTable(struct cohe_table *tbl, ReqType req, CacheLineState st, CacheLineState nxt_st, ReqType nxt_mod_req,int req_sz,int rep_sz,ReqType nxt_req, int nxt_req_sz, int noalloc)
{
  tbl->reqtbl[(int)req].sttbl[(int)st].valid = 1;
  tbl->reqtbl[(int)req].sttbl[(int)st].nxt_st = nxt_st;
  tbl->reqtbl[(int)req].sttbl[(int)st].nxt_mod_req = nxt_mod_req;
  tbl->reqtbl[(int)req].sttbl[(int)st].req_sz = req_sz;
  tbl->reqtbl[(int)req].sttbl[(int)st].rep_sz = rep_sz;
  tbl->reqtbl[(int)req].sttbl[(int)st].nxt_req = nxt_req;
  tbl->reqtbl[(int)req].sttbl[(int)st].nxt_req_sz = nxt_req_sz;
  tbl->reqtbl[(int)req].sttbl[(int)st].noalloc = noalloc;
}

/*****************************************************************************/
/* setup_tables: builds the cache-coherence tables for each cache based on   */
/* the protocol type supported. In some cases, the ReplacementHints field is */
/* also checked -- this must be REPLHINTS_EXCL with the current setup, but   */
/* users interested in changing the Directory structure may be able to use   */
/* REPLHINTS_NONE or REPLHINTS_ALL as appropriate                            */
/*****************************************************************************/

void setup_tables()
{
  
  /*:::::::::::::::: Primary Write Through with Wbuf support ::::::::::::::::*/
  /* With a write-through cache, the only states are INVALID and PR_CL --    */
  /* the cache does not need to think about sharing, and has no dirty lines  */
  /* Additionally, this cache is no-write-allocate                           */
  /***************************************************************************/

  AddEntryToTable(&Primary_WT,READ,INVALID,BAD_LINE_STATE,READ,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WT,READ,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Primary_WT,WRITE,INVALID,BAD_LINE_STATE,WRITE,REQ_SZ,REQ_SZ,0,0,1);
  AddEntryToTable(&Primary_WT,WRITE,PR_CL,PR_CL,WRITE,REQ_SZ,REQ_SZ,0,0,1);
  
  AddEntryToTable(&Primary_WT,RMW,INVALID,BAD_LINE_STATE,RMW,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WT,RMW,PR_CL,PR_CL,RMW,REQ_SZ,REQ_SZ,0,0,0);
  
  AddEntryToTable(&Primary_WT,L1WRITE_PREFETCH,INVALID,BAD_LINE_STATE,L1WRITE_PREFETCH,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WT,L1WRITE_PREFETCH,PR_CL,PR_CL,L1WRITE_PREFETCH,REQ_SZ,REQ_SZ,0,0,0);
  
  AddEntryToTable(&Primary_WT,L1READ_PREFETCH,INVALID,BAD_LINE_STATE,L1READ_PREFETCH,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WT,L1READ_PREFETCH,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Primary_WT,L2WRITE_PREFETCH,INVALID,INVALID,L2WRITE_PREFETCH,REQ_SZ,REQ_SZ,0,0,1);
  AddEntryToTable(&Primary_WT,L2WRITE_PREFETCH,PR_CL,PR_CL,L2WRITE_PREFETCH,REQ_SZ,REQ_SZ,0,0,0);
  
  AddEntryToTable(&Primary_WT,L2READ_PREFETCH,INVALID,INVALID,L2READ_PREFETCH,REQ_SZ,REQ_SZ,0,0,1);
  AddEntryToTable(&Primary_WT,L2READ_PREFETCH,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);

  /* NOTE: types like READ_SH, READ_OWN, UPGRADE never come to PrimaryWT --
     these are requests that just go to the directory, etc. */

  AddEntryToTable(&Primary_WT,REPLY_SH,INVALID,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Primary_WT,REPLY_EXCL,INVALID,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WT,REPLY_EXCL,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  
  AddEntryToTable(&Primary_WT,REPLY_EXCLDY,INVALID,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WT,REPLY_EXCLDY,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  
  AddEntryToTable(&Primary_WT,REPLY_UPGRADE,INVALID,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WT,REPLY_UPGRADE,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Primary_WT,COPYBACK,INVALID,INVALID,COPYBACK,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WT,COPYBACK,PR_CL,PR_CL,COPYBACK,REQ_SZ,REQ_SZ,0,0,0);

  AddEntryToTable(&Primary_WT,COPYBACK_INVL,INVALID,INVALID,COPYBACK_INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WT,COPYBACK_INVL,PR_CL,INVALID,COPYBACK_INVL,REQ_SZ,REQ_SZ,0,0,0);

  AddEntryToTable(&Primary_WT,INVL,INVALID,INVALID,INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WT,INVL,PR_CL,INVALID,INVL,REQ_SZ,REQ_SZ,0,0,0);

  AddEntryToTable(&Primary_WT,REPL,INVALID,INVALID,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WT,REPL,PR_CL,INVALID,BAD_REQ_TYPE,0,0,0,0,0);
  
  /*:::::::::::::::: Second-level write-back cache ::::::::::::::::*/

  AddEntryToTable(&Secondary_WB,READ,INVALID,BAD_LINE_STATE,READ_SH,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,READ,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Secondary_WB,READ,SH_CL,SH_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Secondary_WB,READ,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Secondary_WB,WRITE,INVALID,BAD_LINE_STATE,READ_OWN,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,WRITE,PR_CL,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Secondary_WB,WRITE,SH_CL,BAD_LINE_STATE,UPGRADE,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,WRITE,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  
  AddEntryToTable(&Secondary_WB,RMW,INVALID,BAD_LINE_STATE,READ_OWN,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,RMW,PR_CL,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Secondary_WB,RMW,SH_CL,BAD_LINE_STATE,UPGRADE,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,RMW,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  
  AddEntryToTable(&Secondary_WB,L1WRITE_PREFETCH,INVALID,BAD_LINE_STATE,READ_OWN,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,L1WRITE_PREFETCH,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Secondary_WB,L1WRITE_PREFETCH,SH_CL,BAD_LINE_STATE,UPGRADE,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,L1WRITE_PREFETCH,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  
  AddEntryToTable(&Secondary_WB,L1READ_PREFETCH,INVALID,BAD_LINE_STATE,READ_SH,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,L1READ_PREFETCH,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Secondary_WB,L1READ_PREFETCH,SH_CL,SH_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Secondary_WB,L1READ_PREFETCH,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Secondary_WB,L2WRITE_PREFETCH,INVALID,BAD_LINE_STATE,READ_OWN,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,L2WRITE_PREFETCH,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Secondary_WB,L2WRITE_PREFETCH,SH_CL,BAD_LINE_STATE,UPGRADE,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,L2WRITE_PREFETCH,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  
  AddEntryToTable(&Secondary_WB,L2READ_PREFETCH,INVALID,BAD_LINE_STATE,READ_SH,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,L2READ_PREFETCH,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Secondary_WB,L2READ_PREFETCH,SH_CL,SH_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Secondary_WB,L2READ_PREFETCH,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);

  /* NOTE: types like READ_SH, READ_OWN, UPGRADE never come to SecondaryWB */

  AddEntryToTable(&Secondary_WB,REPLY_SH,INVALID,SH_CL,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Secondary_WB,REPLY_EXCLDY,INVALID,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);

  if (CCProtocol != MSI) 
    {
      /* the protocol has a private clean state */
      AddEntryToTable(&Secondary_WB,REPLY_EXCL,INVALID,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
      AddEntryToTable(&Secondary_WB,REPLY_UPGRADE,SH_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
    }
  else
    {
      AddEntryToTable(&Secondary_WB,REPLY_EXCL,INVALID,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
      AddEntryToTable(&Secondary_WB,REPLY_UPGRADE,SH_CL,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
    }
  
  AddEntryToTable(&Secondary_WB,COPYBACK,INVALID,INVALID,COPYBACK,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,COPYBACK,PR_CL,SH_CL,COPYBACK,LINESZ+REQ_SZ,REQ_SZ,0,0,0); /* NOTE: in some protocols, this would just be an ACK, but in ours we need to give the $-$ the whole line: we give the directory only an ACK, though */
  AddEntryToTable(&Secondary_WB,COPYBACK,SH_CL,SH_CL,COPYBACK,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,COPYBACK,PR_DY,SH_CL,COPYBACK,LINESZ+REQ_SZ,LINESZ+REQ_SZ,0,0,0);

  AddEntryToTable(&Secondary_WB,COPYBACK_INVL,INVALID,INVALID,COPYBACK_INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,COPYBACK_INVL,PR_CL,INVALID,COPYBACK_INVL,LINESZ+REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,COPYBACK_INVL,SH_CL,INVALID,COPYBACK_INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,COPYBACK_INVL,PR_DY,INVALID,COPYBACK_INVL,LINESZ+REQ_SZ,LINESZ+REQ_SZ,0,0,0);

  AddEntryToTable(&Secondary_WB,INVL,INVALID,INVALID,INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,INVL,PR_CL,INVALID,INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,INVL,SH_CL,INVALID,INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Secondary_WB,INVL,PR_DY,INVALID,INVL,REQ_SZ,REQ_SZ,0,0,0);

  AddEntryToTable(&Secondary_WB,REPL,INVALID,INVALID,BAD_REQ_TYPE,0,0,0,0,0);

  if (ReplacementHintsLevel >= REPLHINTS_EXCL)
    AddEntryToTable(&Secondary_WB,REPL,PR_CL,INVALID,REPL,REQ_SZ,REQ_SZ,COPYBACK_INVL,REQ_SZ,0);
  else
    AddEntryToTable(&Secondary_WB,REPL,PR_CL,INVALID,BAD_REQ_TYPE,0,0,COPYBACK_INVL,REQ_SZ,0);
  
  if (ReplacementHintsLevel >= REPLHINTS_ALL)
    AddEntryToTable(&Secondary_WB,REPL,SH_CL,INVALID,REPL,REQ_SZ,REQ_SZ,INVL,REQ_SZ,0);
  else
    AddEntryToTable(&Secondary_WB,REPL,SH_CL,INVALID,BAD_REQ_TYPE,0,0,INVL,REQ_SZ,0);

  AddEntryToTable(&Secondary_WB,REPL,PR_DY,INVALID,WRB,LINESZ+REQ_SZ,REQ_SZ,COPYBACK_INVL,REQ_SZ,0);

  /*:::::::::::::::: First Level Write Back ::::::::::::::::::*/
			
  AddEntryToTable(&Primary_WB,READ,INVALID,BAD_LINE_STATE,READ,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,READ,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,READ,SH_CL,SH_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,READ,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Primary_WB,WRITE,INVALID,BAD_LINE_STATE,WRITE,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,WRITE,PR_CL,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,WRITE,SH_CL,BAD_LINE_STATE,WRITE,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,WRITE,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  
  AddEntryToTable(&Primary_WB,RMW,INVALID,BAD_LINE_STATE,RMW,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,RMW,PR_CL,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,RMW,SH_CL,BAD_LINE_STATE,RMW,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,RMW,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  
  AddEntryToTable(&Primary_WB,L1WRITE_PREFETCH,INVALID,BAD_LINE_STATE,L1WRITE_PREFETCH,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,L1WRITE_PREFETCH,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,L1WRITE_PREFETCH,SH_CL,BAD_LINE_STATE,L1WRITE_PREFETCH,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,L1WRITE_PREFETCH,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  
  AddEntryToTable(&Primary_WB,L1READ_PREFETCH,INVALID,BAD_LINE_STATE,L1READ_PREFETCH,REQ_SZ,LINESZ+REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,L1READ_PREFETCH,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,L1READ_PREFETCH,SH_CL,SH_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,L1READ_PREFETCH,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Primary_WB,L2WRITE_PREFETCH,INVALID,INVALID,L2WRITE_PREFETCH,REQ_SZ,REQ_SZ,0,0,1);
  AddEntryToTable(&Primary_WB,L2WRITE_PREFETCH,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,L2WRITE_PREFETCH,SH_CL,SH_CL,L2WRITE_PREFETCH,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,L2WRITE_PREFETCH,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);
  
  AddEntryToTable(&Primary_WB,L2READ_PREFETCH,INVALID,INVALID,L2READ_PREFETCH,REQ_SZ,REQ_SZ,0,0,1);
  AddEntryToTable(&Primary_WB,L2READ_PREFETCH,PR_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,L2READ_PREFETCH,SH_CL,SH_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,L2READ_PREFETCH,PR_DY,PR_DY,BAD_REQ_TYPE,0,0,0,0,0);

  /* NOTE: types like READ_SH, READ_OWN, UPGRADE never come to PrimaryWB */

  AddEntryToTable(&Primary_WB,REPLY_SH,INVALID,SH_CL,BAD_REQ_TYPE,0,0,0,0,0);

  /* NOTE: even though the 2nd level cache might only have MSI, this
     cache can use a (partial) MESI protocol internally. Namely, this
     cache can keep a PR_CL state so that it does not need to send WRB's
     to the L2 on private lines that have not been written. Further,
     PR_CL at the L1 cache basically means accesses that have a different
     value from the values seen at the L2; they may be dirty as far as the
     system can tell. */
  
  AddEntryToTable(&Primary_WB,REPLY_EXCL,INVALID,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,REPLY_EXCL,SH_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,REPLY_EXCLDY,INVALID,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Primary_WB,REPLY_UPGRADE,INVALID,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,REPLY_UPGRADE,SH_CL,PR_CL,BAD_REQ_TYPE,0,0,0,0,0);

  AddEntryToTable(&Primary_WB,COPYBACK,INVALID,INVALID,COPYBACK,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,COPYBACK,PR_CL,SH_CL,COPYBACK,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,COPYBACK,SH_CL,SH_CL,COPYBACK,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,COPYBACK,PR_DY,SH_CL,COPYBACK,LINESZ+REQ_SZ,LINESZ+REQ_SZ,0,0,0);

  AddEntryToTable(&Primary_WB,COPYBACK_INVL,INVALID,INVALID,COPYBACK_INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,COPYBACK_INVL,PR_CL,INVALID,COPYBACK_INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,COPYBACK_INVL,SH_CL,INVALID,COPYBACK_INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,COPYBACK_INVL,PR_DY,INVALID,COPYBACK_INVL,LINESZ+REQ_SZ,LINESZ+REQ_SZ,0,0,0);

  AddEntryToTable(&Primary_WB,INVL,INVALID,INVALID,INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,INVL,PR_CL,INVALID,INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,INVL,SH_CL,INVALID,INVL,REQ_SZ,REQ_SZ,0,0,0);
  AddEntryToTable(&Primary_WB,INVL,PR_DY,INVALID,INVL,REQ_SZ,REQ_SZ,0,0,0);

  AddEntryToTable(&Primary_WB,REPL,INVALID,INVALID,BAD_REQ_TYPE,0,0,0,0,0);

  /* NOTE: even if the 2nd level cache has to send replacement hints, this
     cache does not need to; the 2nd level will check this cache anyway
     before sending something down to the directory */
  AddEntryToTable(&Primary_WB,REPL,PR_CL,INVALID,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,REPL,SH_CL,INVALID,BAD_REQ_TYPE,0,0,0,0,0);
  AddEntryToTable(&Primary_WB,REPL,PR_DY,INVALID,WRB,LINESZ+REQ_SZ,REQ_SZ,0,0,0);
}

/*****************************************************************************/
/* cohe_sl: This function looks the access up in the coherence table for L2  */
/* caches and informs the cache about the next state and any other           */
/* REQs caused by this transaction                                           */
/*****************************************************************************/

void cohe_sl(int req, CacheLineState cur_state, int allo_type, ReqType req_type, int cohe_type, int dubref, CacheLineState *nxt_st, ReqType *nxt_mod_req, int *req_sz, int *rep_sz, ReqType *nxt_req, int *nxt_req_sz, int *allocate)
{
  struct cohe_table *table;

  if (cohe_type == WB_NREF) /* the only supported cohe_type for L2 cache */
    table = &Secondary_WB;
  else
    {
      fprintf(simout,"cohe_sl(): Coherence type %d (%s) is invalid in this context\n",
	      cohe_type, cohe_type < Cohe_max ? Cohe[cohe_type] : "");
      YS__errmsg("cohe_sl(): Invalid coherence type");
    }
  
  if (table->reqtbl[req_type].sttbl[cur_state].valid == 0)
    {
      /* The access is either not a supported request type or in an invalid
	 current state for the coherence protocol in question */
       
      fprintf(simout,"cohe_sl(): Request type %d (%s) is not valid for this coherence type %d (%s) for a \"%s\" request for state %s\n",
	     req_type, req_type < Req_type_max ? Req_Type[req_type]:"",
	     cohe_type, cohe_type < Cohe_max ? Cohe[cohe_type] : "",
	     Request_st[req], State[cur_state]);
      YS__errmsg("cohe_sl(): Invalid request type");
    }


  /* Fill in all the information associated with the access */
  *nxt_st = table->reqtbl[req_type].sttbl[cur_state].nxt_st; /* next state */
  *nxt_mod_req = table->reqtbl[req_type].sttbl[cur_state].nxt_mod_req; /* REQ-type to next module */
  *req_sz = table->reqtbl[req_type].sttbl[cur_state].req_sz; /* size of that REQ */
  *rep_sz = table->reqtbl[req_type].sttbl[cur_state].rep_sz; /* expected possible reply size */
  *nxt_req = table->reqtbl[req_type].sttbl[cur_state].nxt_req; /* REQ-type to module above */
  *nxt_req_sz = table->reqtbl[req_type].sttbl[cur_state].nxt_req_sz; /* size of that module */
  *allocate = (cur_state != INVALID) || allo_type || !table->reqtbl[req_type].sttbl[cur_state].noalloc; /* does this access allocate? */
}

/*****************************************************************************/
/* cohe_pr: This function looks the access up in the coherence table for the */
/* L1 cache (either WT or WB) and informs the cache about the next state and */
/* any other REQs caused by this transaction                                 */
/*****************************************************************************/

void cohe_pr(int req, CacheLineState cur_state, int allo_type, ReqType req_type, int cohe_type, int dubref, CacheLineState *nxt_st, ReqType *nxt_mod_req, int *req_sz, int *rep_sz, ReqType *nxt_req, int *nxt_req_sz, int *allocate)
{
  struct cohe_table *table;

  if (cohe_type == PR_WT)
    table = &Primary_WT;
  else if (cohe_type == PR_WB)
    table = &Primary_WB;
  else
    {
      /* This is a cache type that does not have a coherence table
	 right now, and thus cannot be processed by this function */
      fprintf(simout,"cohe_pr(): Coherence type %d (%s) is invalid in this context\n",
	      cohe_type, cohe_type < Cohe_max ? Cohe[cohe_type] : "");
      YS__errmsg("cohe_pr(): Invalid coherence type");
    }

  if (table->reqtbl[req_type].sttbl[cur_state].valid == 0)
    {
      /* The access is either not a supported request type or in an invalid
	 current state for the coherence protocol in question */

      fprintf(simout,"cohe_pr(): Request type %d (%s) is not valid for this coherence type %d (%s) for a \"%s\" request for state %s\n",
	     req_type, req_type < Req_type_max ? Req_Type[req_type]:"",
	      cohe_type, cohe_type < Cohe_max ? Cohe[cohe_type] : "",
	     Request_st[req], State[cur_state]);
      YS__errmsg("cohe_pr(): Invalid request type");
    }
  
  *nxt_st = table->reqtbl[req_type].sttbl[cur_state].nxt_st; /* next state */
  *nxt_mod_req = table->reqtbl[req_type].sttbl[cur_state].nxt_mod_req; /* REQ-type to next module */
  *req_sz = table->reqtbl[req_type].sttbl[cur_state].req_sz; /* size of that REQ */
  *rep_sz = table->reqtbl[req_type].sttbl[cur_state].rep_sz; /* expected possible reply size */
  *nxt_req = table->reqtbl[req_type].sttbl[cur_state].nxt_req; /* REQ-type to module above */
  *nxt_req_sz = table->reqtbl[req_type].sttbl[cur_state].nxt_req_sz; /* size of that module */
  *allocate = (cur_state != INVALID) || allo_type || !table->reqtbl[req_type].sttbl[cur_state].noalloc; /* does this access allocate? */
}
