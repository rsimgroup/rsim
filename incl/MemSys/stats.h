/*
  stats.h

  Used for calculating statistics on types of references seen at each cache

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


#ifndef _stats_h_
#define _stats_h_ 1

#include "misc.h"

enum CacheMissType {CACHE_HIT,       /* cache hit */
		    CACHE_MISS_COLD, /* REPLY reveals that this was a cold
					miss (never before accessed by this
					node) */
		    CACHE_MISS_CONF, /* access considered by CapConfDetector
					to be a conflict miss */
		    CACHE_MISS_CAP,  /* access considered by CapConfDetector
					to be a capacity miss */
		    CACHE_MISS_COHE, /* a coherence-type miss (noted for
					present misses) */
		    CACHE_MISS_UPGR, /* upgrade miss */
		    CACHE_MISS_WT,   /* a write-through */
		    CACHE_MISS_COAL, /* coalesced into an MSHR */
		    NUM_CACHE_MISS_TYPES /* NOTE: THIS MUST BE LAST! */
};

extern char *CacheMissTypes[]; /* names for miss types */

/* StatSet sets the cache statistics for the given miss type */
/* do StatSet as soon as miss type above is resolved */
#define StatSet(captr,req,type) {if (MemsimStatOn) {			\
  int t=(int)req->prcr_req_type-(req->s.prefetch?(int)L1WRITE_PREFETCH : (int)READ);	\
  if (!req->s.prefetch)						\
    {									\
      captr->stat.demand_ref[t]++;					\
      captr->stat.demand_miss[t][type]++;				\
    }									\
  else									\
    {									\
      captr->stat.pref_ref[t]++;					\
      captr->stat.pref_miss[t][type]++;					\
    }                                                                   \
  captr->num_ref++;                                                     \
  if (type != CACHE_HIT && type != CACHE_MISS_COAL )                    \
    captr->num_miss++;                                                  \
}}

#endif
