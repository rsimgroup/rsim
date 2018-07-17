/*
  cache.h

  contains variable and function definitions relating to the cache module

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



#ifndef CACHEH
#define CACHEH

#include <stdio.h>
#include "pipeline.h"
#include "module.h"
#include "stats.h"
#include "typedefs.h"
#include "cohe_types.h"
#include "req.h"
#include <string.h>
struct state;
struct Delays;

extern int L1TAG_DELAY;                   /* L1 cache tag array access times */
extern int L1TAG_PIPES;            /* Number of L1 cache tag array pipelines */
extern int L1TAG_PORTS[];           /* Number of L1 cache ports to tag array */

extern int L2TAG_DELAY;                   /* L2 cache tag array access times */
extern int L2TAG_PIPES;            /* Number of L2 cache tag array pipelines */
extern int L2TAG_PORTS[];           /* Number of L2 cache ports to tag array */

extern int L2DATA_DELAY;                 /* L2 cache data array access times */
extern int L2DATA_PIPES;          /* Number of L2 cache data array pipelines */
extern int L2DATA_PORTS[];         /* Number of L2 cache ports to data array */


extern int L1_MAX_MSHRS;                 /* Number of MSHRs at the L1 cache */
extern int L2_MAX_MSHRS;                 /* Number of MSHRs at the L2 cache */

/* Variables indicating availability of resources */
extern int *MSHRS_FULL;
extern int *L2MSHRS_FULL;
extern int *L1Q_FULL;

/*****************************************************************************\
*                                    DEFINES                                 *
\*****************************************************************************/

#define SUB_SZ 2048   /* cache size allocated initially for infinite caches */
#define INFINITE -1

#define FIRSTLEVEL_WT 1                 /* First level write-through caches */
#define FIRSTLEVEL_WB 2                    /* First level write-back caches */
#define SECONDLEVEL 3             /* Second-level cache (always write-back) */


/*########################### CACHE  DECLARATION ############################*/

#define LINESZ   blocksize
#define NOCHANGE -2
#define FULL_ASS  -1
#define NO_REPLACE 2

/*:::::::::::::::::::::::::::::: Cache types ::::::::::::::::::::::::::::::::*/
#define ALLC     1
#define INSTR   2
#define DATA    3
#define DATA_SH 4
#define DATA_PR 5
#define ALL_PR  6
#define ACQUIRE 7
#define RELEASE 8
#define PVT     9

/*:::::::::::::::::::::::::: Replacement types ::::::::::::::::::::::::::::::*/
#define LRU 1
#define FIFO 2
#define RANDOM 3

/*:::::::::::::::::::::::::::::: Cache Line States ::::::::::::::::::::::::::*/

enum CacheLineState{
  BAD_LINE_STATE = 0,
  INVALID=1,
  PR_CL,                /* private clean */
  SH_CL,                /* shared clean */
  PR_DY,                /* private dirty */
  SH_DY,                /* shared dirty */
  NUM_CACHE_LINE_STATES
};
typedef enum CacheLineState CacheLineState;

/*:::::::::::::::::::::::::: Cache Replacement Hints ::::::::::::::::::::::::*/
enum ReplacementHintsType {
  REPLHINTS_NONE,
  REPLHINTS_EXCL,
  REPLHINTS_ALL
};
extern enum ReplacementHintsType ReplacementHintsLevel;

/*:::::::::::::::::::::::::: Cache Coherence Protocol :::::::::::::::::::::::*/
enum CacheCoherenceProtocolType {MESI, MSI};
extern enum CacheCoherenceProtocolType CCProtocol;

/*########################### WRITE BUFFER DECLARATION ######################*/

#define BUS_PC 1
#define DIR_PC 2
#define DIR_WC 3
#define DIR_RC 4

#define Buffer      1
#define BufferReply 2
#define AddHead     3
#define ImmFence    4

/*##################### STATE AND COHERENCE DECLARATIONS ####################*/

extern char *State[];
void setup_tables(void);


/*######################### CACHE  DECLARATION ##############################*/

extern int send_dest[];                  /* destination for "remote-writes" */
extern int wrb_buf_size;           /* number of entries in WRB-buffer at L2 */
extern unsigned int wrb_buf_extra; /* number of extra entries in WRB-buffer
				      (beyond those needed for MSHRS) at L2 */

/*::::::::::::::::::::::: Cache Line Data Structure  ::::::::::::::::::::::::*/

struct cache_data_struct {
  long tag;                                             /* tag of cache line */
  unsigned age;                    /* age of access -- used to implement LRU */
  int dest_node;                                         /* destination node */

  int pref;                                       /* used for prefetch stats */
  long pref_tag_repl;                             /* used for prefetch stats */
  double pref_time;                               /* used for prefetch stats */

  struct {
    CacheLineState st;                                /* state of cache line */
    unsigned cohe_type: 4;      /* coherence type associated with cache line */
    unsigned allo_type: 1;     /* allocation type associated with cache line */
    unsigned mshr_out:1;        /* see if we have an mshr outstanding -- if
				   we do, we're in the middle of an upgrade,
				   so we shouldn't replace this line */
    unsigned cohe_pend:1;    /* are we in the middle of a COHE
			      (between COHE noticed and COHE action)?
			     I f so, don't allow a req to be serviced
			     to  this line. Also, block subsequent COHE's (no
			     deadlock because this is just COHE->COHE_REPLY) */
  }state;
};

/*:::::::::::: Cache Statistics Collection Data Structure  ::::::::::::::::::*/

struct CacheStat {
  /* The use of 3 indicates reads, writes, and RMWs 
     In order to access these arrays, index them with
     req->prcr_req_type - READ */
  int demand_ref[3]; 
  int demand_miss[3][NUM_CACHE_MISS_TYPES];

  /* The use of 2 indicates reads and writes
     In order to access these arrays, index them with
     req->prcr_req_type - L1WRITE_PREFETCH */
  int pref_ref[4];
  int pref_miss[4][NUM_CACHE_MISS_TYPES];


  int pipe_stall_MSHR_WAR;  /* stalls that arise from WAR */
  int pipe_stall_MSHR_FULL; /* stalls that arise from FULL MSHRs */
  int pipe_stall_MSHR_CONF; /* stalls that arise from too many MSHRs to set */
  int pipe_stall_MSHR_COHE; /* stalls from pending COHE messages */
  int pipe_stall_MSHR_COAL; /* stalls from excessive coalescing */
  int pipe_stall_WRB_match; /* stall from REQ matching pending WRB */
  int pipe_stall_WRBBUF_full; /* stall from REQ matching pending WRB */
  int pipe_stall_PEND_COHE; /* stall from REQ to a line that has a cohe
			       pending (only for L2) */
  int cohe_stall_PEND_COHE; /* stall from COHE to a line that has a cohe
			       pending already (only for L2) */
  
  int dropped_pref; /* just count this number */

  /* average latency statistics */
  double avg_write_lat;
  double avg_read_lat;
  double avg_rmw_lat;

  /* coherence request/reply statistics */
  int cohe;
  int cohe_nack;
  int cohe_reply;
  int cohe_reply_merge;
  int cohe_reply_merge_ignore;
  int cohe_reply_nack;
  int cohe_reply_nack_docohe;
  int cohe_reply_nack_mergefail;
  int cohe_reply_nack_pend;
  int cohe_reply_prop_nack_pend;
  int cohe_reply_unsolicited_WRB;

  /* write-back statistics */
  int wb_inclusions_sent;
  int wb_prcl_inclusions_sent;
  int wb_inclusions_race;
  int wb_inclusions_real;
  int wb_repeats;

  /* write-back victim statistics */
  int shcl_victims;
  int prcl_victims;
  int prdy_victims;

  /* cache-to-cache transfer statistics */
  int cohe_cache_to_cache_good;
  int cohe_cache_to_cache_fail;

  int replies_nacked;
  int rars_handled;

  /* prefetch statistics */
  int pref_total;
  int pref_useless; /* if line gets replaced before demand access */
  int pref_useless_cohe; /* if line gets invalidated before demand access */
  int pref_downgraded; /* if a prefetch goes from Exclusive to Shared */
  int pref_useful;
  int pref_useful_upgrade;
  int pref_unnecessary; /* if there is no nxt_mod_req */
  int pref_late; /* if others coalesce into MSHR */
  int pref_damaging; /* if the line pref replaced gets used before
			the line prefetched */
  
};

/*::::::::::::::::::::::::: Cache Module Data Structure :::::::::::::::::::::*/

struct YS__Cache {
  MODULE_FRAMEWORK
  
  CacheStatStruct stat;
  
  /* data structures and attributes */
  int cache_level_type;                         /* Cache level count */
  int     cache_type;                          /* Type of this cache */
  int     size;                                      /* cache size */
  int     linesz;                               /* cache line size */
  int     setsz;                                  /* associativity */
  struct {
    unsigned cohe_type: 4;                   /* cohe type of cache */
    unsigned allo_type: 1;                   /* allocation type */
    unsigned adap_type:1;                    /* adaptivity -- not used */
    unsigned replacement:4;                  /* replacement policy */
  } types;
  func    cohe_rtn;                 /* coherence routine to invoke */
  func    prnt_rtn;                  /* printing routine to invoke */
  
  Cachest ** data;             /* actual cache line information */
  int num_lines;               /* number of cache lines */
  int block_bits;              /* number of bits in address removed to generate block # */
  int set_bits;                /* number of bits in address used to specify set */
  int non_tag_bits;            /* remaining bits in address */
  
  /* Mshr-related data structures */
  MSHRITEM *mshrs;             /* MSHR structures */
  int mshr_count;              /* number of MSHRs in use */
  int reqmshr_count;           /* number of MSHRs used by REQUESTs */
  int reqs_at_mshr_count;      /* number of REQUESTs at MSHRs
				  (including coalesced) */
  int max_mshrs;               /* total number of MSHRs in cache */

  /* smart MSHR queue. Used to simulate activites being held in some
     cache resource (such as an MSHR) waiting to be sent to another
     module. Discussed further in l1cache.c */
  struct YS__SmartMSHRlist *SmartMSHRHead, *SmartMSHRTail;

  /* Pipelined cache structure */
  struct YS__Pipeline **pipe;      /* For the tag array and data RAM array
				      pipes */
  int num_in_pipes;                /* number of accesses in pipes */

  struct YS__WrbBufEntry **wrb_buf;
  /* this contains the tags of outstanding WRB/INVL requests at L2 cache.
     These are outstanding only until the requests issue to the system;
     after that, the entry can be freed up, as these do not expect further
     REPLYs. Note that an entry must be reserved before issuing a REQUEST
     from the L2 cache, as the incoming REPLY may need an entry. */
  int wrb_buf_used; /* number of entries used in wrb-buffer */

  struct CapConfDetector *ccd;        /* Capacity-conflict detector */

  /* Statistics on REQUEST types */
  STATREC *net_demand_miss[3]; /* demand latencies beyond L2 cache */
  STATREC *net_pref_miss[4];   /* prefetch latencies beyond L2 cache */
  STATREC *mshr_occ;           /* MSHR occupancy */
  STATREC *mshr_req_count;     /* Number of requests in MSHRs */
  STATREC *pref_lateness;      /* extent to which late prefetches are late */
  STATREC *pref_earlyness;     /* length of time useful prefetches sit in
				  cache before demand access */
};

/* Cache-related functions -- more documentation in .c files */
/* Allocate and initialize a cache */
CACHE *NewCache(char *name, int node_num, int stat_level, rtfunc routing,
		struct Delays *Delays, int ports_abv, int ports_blw, 
		int cache_type, int size, int line_size, int set_size,
		int cohe_type,
		int allo_type, int cache_type1, int adap_type, int replacement,
		func cohe_rtn, int tagdelay, int tagpipes, int tagwidths[],
		int datadelay, int datapipes, int datawidths[],
		int netsend_port, int netrcv_port, LINKPARA *link_para,
		int max_mshrs,	int procnum, int unused);
void init_cache(CACHE *); /* set up cache fields and statistics */

/* Statistics functions */
void CacheStatReportAll(void);
void CacheStatClearAll(void);
int CacheStatReport (CACHE *);
void CacheStatClear (CACHE *);

/* Coherence functions -- implement cache-coherence protocol of system */
void cohe_sl(int req, CacheLineState cur_state, int allo_type, ReqType req_type,  int cohe_type, int dubref, CacheLineState *nxt_st, ReqType *nxt_mod_req, int *req_sz, int *rep_sz, ReqType *nxt_req, int *bus_chng_req, int *allocate);
void cohe_pr(int req, CacheLineState cur_state, int allo_type, ReqType req_type,  int cohe_type, int dubref, CacheLineState *nxt_st, ReqType *nxt_mod_req, int *req_sz, int *rep_sz, ReqType *nxt_req, int *bus_chng_req, int *allocate);

/* Determine the coherence/allocation/etc policies of the cache */
void cache_get_cohe_type (CACHE *,int,int,long,int *, int *, int *);

/* Produce requests to send on replacements/victimizations */
REQ *GetReplReq(CACHE *,REQ *,int,ReqType,int,int,int,int,int,int,int);

int notpres (long, long *, int *, int *, CACHE *); /* line present in cache? */

/* Functions to update LRU replacement ages in cache */
void hit_update (unsigned, CACHE *,int, REQ *); /* update on hit */
void premiss_ageupdate (CACHE *,int, REQ *, int); /* update "present miss" */
int miss_ageupdate (REQ *, CACHE *,int *,long, int); /* update and find
							replacement on
							total miss */

/* Simulation functions */
extern void L1CacheOutSim(struct state *proc);
extern void L1CacheInSim(struct state *proc);
extern void L2CacheOutSim(struct state *proc);
extern void L2CacheInSim(struct state *proc);

/*########################## WRITE BUFFER DECLARATION #######################*/

/* Write buffer element item */
struct YS__WbufItem {
  REQ *req;
  int counter;         /* counter of requests in current item */

  REQ *coal_req[MAX_MAX_COALS]; /* coalesced requests */
  
  int fence;           /* flag a fence (currently unused) */
  int tag;             /* tag of pending request */
  int address;         /* address of pending request */
  int pend_chng;       /* change pending bit */
  REQ *rwrite_req;     /* keep track of any remote writes that arrive when
			  the line is pending */
  int dirty;           /* dirty bits - for the wbuffer */
  int inval;           /* added to invalidate a pending line */
  WBUFITEM *next;      /* used for linked list */
};

/* Write buffer module */
struct YS__WBuffer {
  MODULE_FRAMEWORK

  int     wbuf_type;
  WBUFITEM *write_buffer; /* Outstanding Write Buffer */
  
  int     wbuf_sz_tot; /* maximum wbuffer entries */
  int     wbuf_sz;     /* operations waiting to issue to next level */
  int     counter;	/* A counter to keep track of outstanding requests --
			   for possible expansion */
  func    prnt_rtn;     /* statistics printing routine to invoke */

  /* Statistics collection */
  int stall_wb_full;     /* Number of times requests stalled because WB full */
  int stall_coal_max;    /* Number of times requests stalled because too many
			    writes coalesced into a single line              */
  int stall_read_match;  /* Number of times read requests stalled because
			    of match with prior write                        */
  int coals;             /* Number of writes coalesced with prior writes     */
};

/* Initialize write-buffer */
WBUFFER *NewWBuffer(char *name, int node_num, int stat_level, rtfunc routing,
		    struct Delays *Delays, int ports_abv, int ports_blw, 
		    int size, int wbuf_type, func prnt_rtn);

/* Statistics functions */
void WBufferStatReportAll(void);
void WBufferStatReport(WBUFFER *);
void WBufferStatClearAll(void);
void WBufferStatClear(WBUFFER *);

extern void WBSim(struct state *proc, int type); /* Responsible for simulating
						    write-buffer actions */
/* ########## CACHE HELPER FUNCTIONS -- discussed in cachehelp.c ######### */

int AddReqToOutQ(CACHE *,REQ *);
int GlobalPerformAllCoalesced(CACHE *, REQ *);
int GlobalPerformAndHeapInsertAllCoalesced(CACHE *, REQ *);
int GlobalPerformAndHeapInsertAllCoalescedWritesOnly(CACHE *, REQ *);
int GlobalPerformAndHeapInsertAllCoalescedL2PrefsOnly(CACHE *, REQ *);
int ProcessFromSmartMSHRList(CACHE *, int);
void NackUpgradeConflictReply(CACHE *, REQ *);
void AddToSmartMSHRList(CACHE *, REQ *, int, int (*)(CACHE *,int,REQ*));
REQ *MakeForwardAck(REQ *, CacheLineState);
void MakeForward(CACHE *,REQ *,CacheLineState);

#define L1_DEFAULT_NUM_PORTS  2  /* default number of ports */
#define L2_DEFAULT_NUM_PORTS  1  /* default number of ports */
#define CACHE_PORTSENTRY 0 /* position of NUM_PORTS in tag pipecount array  */
#define MAX_REQ_MSHR_HISTOGRAM 32
extern int L1_NUM_PORTS;
extern int L2_NUM_PORTS;

/* arguments sent to WBSim to indicate which cache is calling it */
#define L1WB 0 /* called by L1 cache */
#define L2WB 1 /* called by L2 cache */

#endif


