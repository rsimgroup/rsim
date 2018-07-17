/*
  mshr.h

  Declarations of structures and functions related to cache MSHRs
  
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


#ifndef _mshr_h_
#define _mshr_h_ 1

#include "module.h"
#include "miss_type.h"

struct YS__MSHR /* MSHR data structure */
{
  int valid;             /* Is this MSHR entry valid */
  int setnum;            /* Corresponding set number for the MSHR */
  REQ *mainreq;          /* The first request that was sent out */
  int counter;           /* The number of coalesced accesses */
  REQ *coal_req[MAX_MAX_COALS]; /* array of coalesced accesses */
  enum ReqType pend_cohe_type; /* Coherence messages that came while the request was outstanding */
  int stall_WAR; /* there's a write after read stall in effect */
  double demand; /* time of first demand access -- used for computing
		    lateness of a late prefetch */
  int only_prefs;     /* Does this MSHR include only prefetches? */
  int writes_present; /* this will say if we need to go to PR_DY or if
			 EXCL (PR_CL) is sufficient */
  MSHRITEM *next;
};
  
/* these are the return values from notpres_mshr */

enum MSHR_Response {MSHR_COAL,       /* REQUESTS: coalesce into an old MSHR
				        COHES: either coalesce, NACK, or
					       NACK_PEND according to type
					       of COHE (discussed in mshr.c
					       and l1cache.c)      */
		    MSHR_NEW,        /* get and need a new MSHR */
		    MSHR_FWD,        /* REQUESTS: get and need a new MSHR for
					          an UPGRADE.
					COHE: either coalesce, NACK, or
					      NACK_PEND according to type of
					      COHE (discussed in mshr.c and
					      l1cache.c)          */
		    MSHR_STALL_WAR,  /* stall because of a WAR */
		    MSHR_STALL_COHE, /* stall because of a merged MSHR cohe */
		    MSHR_STALL_COAL, /* stall because of excessive
					coalescing */
		    MSHR_STALL_WRB,  /* stall because conflicts with a
					current WRB */
		    MSHR_USELESS_FETCH_IN_PROGRESS,
		                     /* prefetch absolutely useless; line fetch
					already in progress */
		    NOMSHR_STALL,    /* stall because no MSHRs left */
		    NOMSHR_STALL_CONF, /* stall because too many MSHRs out
					  for this set already (unused) */
		    NOMSHR_STALL_COHE, /* can't be serviced because cohe_pend
					  bit is set */
		    NOMSHR_STALL_WRBBUF_FULL,  /* stall because not enough
						  space in the WRB buf for
						  potential victimization on
						  reply */
		    NOMSHR_FWD,      /* does not need an MSHR, send it down */
		    NOMSHR_PFBUF,     /* a buffered prefetch */
		    NOMSHR,           /* no MSHR involved or needed */
		    MAX_MSHR_TYPES
};
				      

/* notpres_mshr checks if access is present in any MSHR of if any
   MSHR is involved or needed (and, in the case of REQUEST, if line
   present in cache) */
enum MSHR_Response notpres_mshr(struct YS__Cache *,struct YS__Req *);
int MSHRIterateUncoalesce(struct YS__Cache *,int mshr_num, int (*)(struct YS__Cache *,struct YS__Req *), enum MISS_TYPE);

/* Free up an MSHR or substitute a new REQUEST into an MSHR */
int RemoveMSHR(struct YS__Cache *, int mshr_num, REQ *subst);

/* return the MSHR number for a given access */
int FindInMshrEntries(struct YS__Cache *,struct YS__Req *);

/* Find the type of COHE merged with MSHR, if any */
enum ReqType GetCoheReq(struct YS__Cache *,struct YS__Req *, int mshr_num);

extern int DISCRIMINATE_PREFETCH; /* drop prefetches if stalled for MSHRs
				     full or other unacceptable condition? */

extern char *MSHRret[]; /* names of MSHR responses */

#endif
