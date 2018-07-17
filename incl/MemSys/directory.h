/*
  directory.h

  Defines variables and functions relating to the directory module

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



#ifndef _directory_h_
#define _directory_h_ 1

#include "typedefs.h"
#include "module.h"


/*############################# DIRECTORY  DECLARATION ######################*/

/* Maximum sizes allowed by directory.
   NOTE: Any update to MAX_MEMSYS_PROC may also need to be reflected here. */
#define VECTOR_SIZE 4        /* number of integers to be used in bit vector */
#define MAX_NUM_NODES 4*sizeof(int)*8	/* maximum number of nodes allowed  */

/* Information related to a single line held in the directory */
/* Directory's view of line state */
#define UNCACHED 0           /* Line is thought to be uncached by all nodes */
#define DIR_SHARED 1         /* Line is thought to be held in shared mode by
				one or more nodes */
#define DIR_PRIVATE 2        /* Line is thought to be held in exclusive
				(PR_CL or PR_DY) mode by one node */

/* structure used to represent status of a given line in the directory */
struct YS__DirLine {
   char   *pnxt;                /* "next" pointer used in pool management */
  char  *pfnxt;                 /* "next free" pointer used in pool
				   management */
  int   tag;                    /* tag identifying the line in question */
  int   vector[VECTOR_SIZE];	/* Bit vector that specifies all caches with
				   a copy of the line */
  int   cold_vec[VECTOR_SIZE];  /* Bit vector that specifies which caches
				   have ever seen the line -- identifies cold
				   misses */
  int   state :4;               /* state of line -- specified above */
  int   pend :1;                /* Does this line have a pending transaction
				   outstanding? */
  int   numsharers;             /* Number of sharers for the current line --
				   this is redundant, as it can be calculated
				   from the bit vector; provided for ease
				   of simulation */
  DirEP *extra;                 /* extra information associated with the
				   processing of the line when there is an
				   outstanding transaction. Specified below */
  Dirst *next;                  /* "next pointer" for the chained hash table
				   used among lines in the directory */
};

/* Extra information structure to specify processing information */
struct YS__DirEP {
  char   *pnxt;                 /* "next" pointer used in pool management */
  char  *pfnxt;                 /* "next free" pointer used in pool
				   management */
  REQ   *pend_req;              /* pending request to either restart when all
				   COHE_REPLYs have been received or to retry
				   in the case of certain NACK types */
  int   counter;                /* number of COHEs sent for this line that
				   still haven't come back */
  int   num_left;               /* number of COHE_REPLYs not yet sent out */
  int   ret_st;                 /* identifies how to handle incoming
				   COHE_REPLYs for this line */
  int   nack_st;                /* specifies whether a NACK is an acceptable
				   response for this line */
  int   waiting;                /* used if this line is expecting a WRB
				   or replacement message before being allowed
				   to process a certain request (see
				   WAITFORWRB below) */
  int  WaitingFor;              /* Node from which to expect a WRB or
				   replacement message */
  int  WasHere;                 /* Node which has provided a WRB or
				   replacement message -- used in certain
				   races involving NACKs */
  int node_ary[MAX_NUM_NODES];  /* array of nodes which require COHE
				   messages */
  int size_st;                  /* Size of message being sent */
  int size_req;                 /* Size of expected reply */
  int next_req_type;            /* Specifies COHE type being sent as a result
				   of recent REQUEST */
};

/* Response types from Dir_Cohe function -- indicate how to process
   a transaction to a specific line */
#define WAIT_CNT 1          /* transaction requires additional COHE
                 	       messages to be sent and will continue only
			       after all such messages have received replies */
#define WAIT_PEND 2         /* transaction cannot be processed immediately,
			       as the line already has other actions
			       oustanding */
#define VISIT_MEM 3         /* data can be sent immediately */
#define DIR_REPLY 4         /* acknowledgment can be sent immediately. Also
			       used for "ret_st" for certain types of WAIT_CNT
			       [when the line will be sent in shared mode] */  
#define SPECIAL_REPLY 7     /* the value specified for "ret_st" for
			       certain types of WAIT_CNT [when the line will
			       sent in exclusive state] */
#define FORWARD_REQUEST 8   /* data will be provided through cache-to-cache
			       transfer. This message will be forwarded to
			       the cache that owns the line */
#define ACK_REPLY 9         /* the value specified for "ret_st" when
			       involved in a cache-to-cache transfer. In
			       these cases, the COHE_REPLY should simply
			       free up its pending buffer, as the data will
			       have been provided directly by the former
			       owner */
#define WAITFORWRB 10       /* This line is involved in a race; it has
			       requested the line even though the directory
			       still thinks that it has the line in exclusive
			       state. The directory assumes that the cache
			       has a WRB or replacement hint in flight and that
			       the request bypassed the replacement message
			       at some point. The directory waits for
			       the WRB or replacement hint to return before
			       processing this REQUEST */
#define Directory_rtn_max 11 /* used in names.c to specify number
				of response types */
extern char *DirRtnStatus[]; /* array of response names -- used in names.c
				(for printing out trace/debug information) */

/*:::::::::::::::::: Directory Module Data Structure  :::::::::::::::::::::::*/

/* Directory type specifier */
#define CNTRL_FULL_MAP 1     /* Directory used is always full-mapped */
#define HashIdxMask (0x3FF)	/* 1's in ten bits; used by dir hash table */

struct YS__Directory {
  MODULE_FRAMEWORK

  int     dir_type;                             /* Type of directory */
  int     line_size;                          /* directory line size */
  int     num_nodes;                    /* number of nodes in system */
  int     vec_index;           /* number of ints used in bit vectors */
  int     reqport;                      /* port from which directory
				           receives REQUESTs         */
  int     next_port;           /* next port to process -- used in RR */
  struct DirQueue *OutboundReqs, *ReadyReqs; /* requests which were
						waiting, but are now
						ready to be sent out
						or processed         */
  REQ     *req_partial;            /* partially processed REQUEST --
				      stalled while sending out
				      COHE messages                  */
  int     wasPending;              /* REQUEST being processed was
				      formerly pending and is held
				      in pending buffer              */

  int     buftotsz;                /* current size of pending buffer */
  int     bufmaxsz;                /* maximum size of pending buffer */
  REQ     *BufHead;                /* head element shown in pending
				      buffer -- only "WAIT_PEND"s are
				      actually added here; others
				      are just accounted for without
				      actually being added           */
  REQ     *BufTail;                /* tail element of pending buffer */
  int     bufsz_rdy;               /* number of formerly pending
				      requests that are now ready
				      to be processed                */
  int     outboundsz;              /* number of requests that are
				      waiting to be sent out         */
  int     wait_cntsz;              /* number of COHEs in flight that
				      have not yet returned          */

  DirEP   *extra;                  /* used for building up per-line
				      "extra" items discussed above  */
  Dirst    *data_hash[HashIdxMask+1];  /* chain-hash table for lines  */
  cond    cohe_rtn;                /* coherence routine to call (only
				      Dir_Cohe available now)        */

  /* Directory statistics */
  int num_read; /* nuber of read requests reaching the directory                   */
  int num_cohe; /* number of requests requiring coherence messages (excluding C2C) */
  int num_clean;/* number of requests not requiring coherence actions              */ 
  int num_write;/* number of read requests                                         */
  int num_writeback; /* number of write-backs reaching the directory               */
  int num_local;     /* number of requests from local nodes                        */
  int num_remote;    /* number of requests from remote nodes                       */
  int num_buf_RAR;   /* number of retries sent because of buffer-full              */
  int num_race_RAR;  /* number of retries sent due to race conditions              */
  int num_c2c;       /* number of cache to cache transfers                         */
  STATREC *CoheNumInvlMeans; /* average number of invalidations per request        */
  STATREC *BufTotSzMeans;    /* average number of entries in the directory buffer  */
  double time_of_last_clear; /* time of the last clearstat, for util calc.         */
 };

struct DirDelays; /* Specifies delays associated with various directory
		     actions */

/* Initialization function */
DIR *NewDir(char *name, int node_num, int stat_level, rtfunc routing,
	    struct DirDelays *Delays, int ports_abv, int ports_blw, 
	    int line_size, int dir_type, int num_nodes, int bufsz,
	    cond cohe_rtn, int netsend_port, int netrcv_port, LINKPARA *link_para);
/* Coherence function -- specifies actions taken for each type of
   new incoming transaction */
int Dir_Cohe(DIR *dirptr, REQ *req, Dirst **dir_item, Dirst *copy, DirEP *extra);
void DirStatReportAll(void);     /* prints statistics for all directories */
void DirStatReport(DIR *dirptr);  /* prints statistics for this directory */
void DirStatClearAll(void);     /* clears statistics for all directories */
void DirStatClear(DIR *dirptr);   /* clears statistics for this directory */
void DirHandshakeQ();      /* handshake function used by routing routines */

void DirSim();            /* responsible for simulating directory actions */
void dir_stat();        /* controls certain types of directory statistics */

Dirst *DirHashLookup_FM();   /* function controlling directory hash-table */
void DirHashList();          /* prints directory hash table */

extern int INTERLEAVING_FACTOR;           /* degree of memory interleaving */
extern STATREC **InterleavingStats;        /* stats associated with memory
				              interleaving -- may be used
				              to detect hot-spotting       */

extern double DIRCYCLE;            /* length of time for minimum directory
				      access (non-data COHE_REPLY)         */

/* delays needed for sending out COHE messages from directory */
extern int DIR_PKTCREATE_TIME, DIR_PKTCREATE_TIME_ADDTL;

#endif
