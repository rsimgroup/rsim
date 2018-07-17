/*
  req.h

  The "REQ" data structure is the fundamental unit of information exchange
  in the memory system simulator. This file contains declarations and
  prototypes related to this structure. 

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


#ifndef _req_h_
#define _req_h_ 1

#include "typedefs.h"
#include "miss_type.h"

/*****************************************************************************/
/* ReqType: type of transaction carried in a given REQ                       */
/*****************************************************************************/

enum ReqType {BAD_REQ_TYPE = 0,
	      /* First come the processor-only request types */
	      READ=1,WRITE,RMW,
	      L1WRITE_PREFETCH,L1READ_PREFETCH,
	      L2WRITE_PREFETCH,L2READ_PREFETCH,
	      TTS,SPIN,WRITEFLAG,
	      
	      /* Next come system-visible transaction requests
		 (may also be processor requests) */
	      READ_SH,   /* read to share */
	      READ_OWN,  /* read to own */
	      UPGRADE,   /* upgrade from shared to modified */
	      READ_DISC, /* read to discard: a coherent read
			    that will not be put in cache,
			    like on UltraSPARC -- reserved for
			    future expansion */
	      WRITE_INV, /* write and invalidate, like for a
			    block store on UltraSPARC. Not
			    currently implemented */
	      RWRITE,    /* remote writes -- not currently supported */
	      
	      /* Next comes reply types for lines */
	      
	      REPLY_SH, REPLY_EXCL, REPLY_UPGRADE,
	      REPLY_EXCLDY, REPLY_SHDY, /* these are used in $-$,
					   where the line may already
					   be dirty */
	      REPLY_DISC, REPLY_WRITEINV,
	      
	      /* Next come COHE/COHE_REPLY types */
	      
	      COPYBACK,      /* requests a copyback. Used in cache-to-cache
				transfers ending in SH_CL at both caches. A
				copy of the data should be sent to the
				directory */
	      COPYBACK_INVL, /* copyback plus invalidate. Used in
				cache-to-cache transfers where ownership
				is transferred and the requestor ends
				with the line in PR_DY. An acknowledgment
				(without data) should be sent to the
				directory */
	      INVL,          /* just invalidate */
	      COPYBACK_SHDY, /* a cache-to-cache transfer ending up in SH_DY,
				so data does not need to be sent back to
				system */
	      COPYBACK_DISC, /* copyback for a read w/ discard; no state change */
	      PUSH, /* a COHE type assoc'ed with RWRITES */
	      
	      /* victimization messages */
	      WRB,REPL,
	      Req_type_max  /* NOTE: THIS MUST BE LAST! */};
typedef enum ReqType ReqType;


/*****************************************************************************/
/* Codes used in req->s.type, req->s.reply to indicate type of               */
/* transaction/response                                                      */
/*****************************************************************************/
#define REQUEST 5        /* used in s.type to indicate a new request for
			    data or ownership */
#define REPLY 1          /* used in s.type to indicate a response to a
			    REQUEST. used in s.reply to indicate positive
			    acknowledgment */
#define COHE    6        /* used in s.type to indicate a coherence transaction
			    resulting from a REQUEST (includes INVLs and
			    cache to cache transfers) */
#define COHE_REPLY 7     /* used in s.type to indicate a response to a
			    coherence transaction */
#define Req_st_max 8

#define RAR   3          /* used in s.reply to indicate "request a retry".
			    Request could not be processed, so cache should
			    resend it. */
#define NACK  5          /* used in s.reply for a COHE_REPLY to indicate a
			    negative acknowledgment  */
#define NACK_PEND 6      /* used in s.reply for a COHE_REPLY to indicate that
			    the access cannot be processed; sender should
			    reevaluate whether or not COHE was needed
			    and resend it if needed */
			   
#define Reply_type_max 7

/*****************************************************************************/
/* Does the request expect an acknowledgment?                                */
/*****************************************************************************/

#define RET     1		/* Return a reply or an acknowledgement */
#define N_RET   2		/* Do not return a reply */


/*****************************************************************************/
/* Direction of a travel by a REQ                                            */
/*****************************************************************************/

#define REQ_FWD 1		/* Sending a request forward */
#define REQ_BWD 2		/* Routing a reply back to sender */

#define Dir_type_max 4

#define ABV 1			/* toward higher levels of hierarchy */
#define BLW 2			/* toward lower levels of hierarchy */

/*****************************************************************************/
/* Statistics on how this REQUEST was processed                              */
/*****************************************************************************/

enum ReqStatType { /* statistics type on how the REQUEST was processed */
  reqUNKNOWN,             /* set at initialization -- later filled in */
  reqL1HIT,               /* hit in L1 cache */
  reqL1COAL,              /* coalesced with L1 MSHR */
  reqWBCOAL,              /* coalesced in write-buffer */
  reqL2HIT,               /* hit in L2 cache */
  reqL2COAL,              /* coalesced with L2 MSHR */
  reqDIR_LH_NOCOHE,       /* sent to local directory, needed no COHEs */
  reqDIR_LH_RCOHE,        /* sent to local directory, needed remote COHE */
  reqDIR_RH_NOCOHE,       /* sent to remote directory, needed no COHEs */
  reqDIR_RH_LCOHE,        /* sent to remote directory, needed COHE to same
			     node as directory (local COHE) */
  reqDIR_RH_RCOHE,        /* sent to remote directory, needed remote COHEs
			     (different nodes than directory itself */
  reqCACHE_TO_CACHE,      /* handled by cache-to-cache transfer */
  reqNUM_REQ_STAT_TYPE    /* NOTE: THIS MUST BE LAST! */
};

/*****************************************************************************/
/* Names for the definitions above                                           */
/*****************************************************************************/
extern char *Req_Type[];
extern char *Reply_st[];
extern char *Request_st[];
extern char *Reply[];
extern char *ReqDir[];
extern char *Req_stat_type[];


/*****************************************************************************/
/* The REQ data structure: This contains a variety of fields used in each    */
/* of the modules of the memory system simulator                             */
/*****************************************************************************/

struct YS__Req { /* REQ data structure */
  char   *pnxt;                /* Next pointer for Pools      */
  char    *pfnxt;              /* Free list pointer for pools */
  REQ     *next;               /* Used for building linked-lists of REQs
				  in directory, ports, etc. */
  char    name[32];            /* name of request */
  int     id;                  /* request identifier */
  int     priority;            /* priority at ports, etc. Unused in RSIM */
  int     in_port_num;         /* port number from which this REQ entered
				  this module */
  int out_port_num;            /* This indicates the output port to
				  which this request is headed */
  long    address;             /* address requested */
  ReqType     req_type;        /* transaction type */
  ReqType     prcr_req_type;   /* transaction type as seen at the processor */
  long    tag;                 /* specifies the cache line to which the
				  action in question applies */
  int     address_type;	       /* type of address region being accessed --
				  currently only DATA is supported */
  int     dubref;              /* double reference -- for future expansion */
  int     size_st;		/* size of transaction being sent */
  int     size_req;		/* size of data requested */
  struct {
    unsigned reply: 4;		/* REPLY, RAR, PEND */
    unsigned dir  : 4;		/* REQ_FWD, REQ_BWD */
    unsigned route: 4;		/* ABV, BLW */
    unsigned ret  : 4;		/* RET, N_RET */
    unsigned type : 4;		/* REQUEST, COHE, REPLY */
    unsigned nc   : 1;              /* not cacheable */
    unsigned dirdone: 2;            /* access processed at directory */
    unsigned flag_var: 1;           /* this is a flag var - unused */
    unsigned rw_flags: 8;           /* these are the remote write flags */

    /* fields used to identify this REQ to the processor simulator */
    struct instance *inst;
    int inst_tag;
    struct state *proc;

    
    unsigned prefetch:2; /* prefetch access? */
    
    int nack_st; /*NACK_OK or NACK_NOK on COHE messages from directory */
    int preprocessed; /* Indicates if a request has already been
			 processed, and hence has to be just bounced
			 back without doing anything */
    int l1nack:1; /* Indicates if it came from L1 to L2 as a nacked reply --
		     used for stats */
    unsigned prclwrb:1; /* Used in L2 victimization case to avoid accessing
			   L2 data array on PR_CL replacement */
  }s;
  int     cohe_type;           /* coherence type of access */
  int     allo_type;           /* allocation type of access */
  int     linesz;		/* line-size in bytes */
  
  Dirst   *dir_item; /* Directory line data structure corresponding to this
			REQ */
  int     src_node;  /* source node of REQ */
  int     dest_node; /* destination node */
  
  /* Statistics for this request */
  double  start_time;  /* used for network stats */
  double  blktime;     /* used for network stats */
  enum ReqStatType handled; /* how was this REQ processed */
  enum MISS_TYPE miss_type; /* what type of miss, if any? */
  unsigned prefetched_late:1; /* was this an access that to a line that
				 suffered from a late prefetch? */
  
  int line_cold:1; /* a bit to say whether this request ended up cold */

  /* stats for latencies calculated from different points in
     progress of this access */
  double mem_start_time,mem_end_time;
  double active_start_time, issue_time;
  double net_start_time;

  int coal_count; /* Number of accesses coalesced with this request */
  REQ **coal_req_array; /* array REQ pointers for accesses coalesced
			   with this request */
  int progress;             /* Indicates the stage of progress for this
			       REQUEST while being processed in one of the
			       RAM arrays */
  int mshr_num;             /* mshr number used by this REQUEST */
  
  REQ *invl_req; /* invalidation request for subset-enforcement sent as
		    a result of this REQ */
  REQ *wrb_req;  /* WRB to system or for subset-enforcement sent as a
		    result of this REQ */
  int absorb_at_l2; /* Should this REQ be absorbed at the L2 cache without
		       further processing? */
  int read_with_write; /* flag for accesses that coalesce together in
			  later levels of the memory hierarchy although one
			  type (i.e. writes with a non-write allocate L1 or L2
			  prefs) doesn't allocate in the L1 cache. */

  int     forward_to; /* send cache-to-cache transfer to this requestor node */
  int     push_dest;           /* destination for pushes into caches - */
  int inuse; /* for debugging Pools */
};


/************** COMMON DECLARATIONS ************/

#define WORDSZ 4   /* word size =  4 bytes */
extern int REQ_SZ; /* Request header size.
		      default is 16 -- 8 for address, 2 for from,
		      2 for command, 2 for to, 2 bytes reserved for
		      future use */

/* COHE messages from directory to cache */
#define NACK_OK 1  /* no data copyback or transfer sought; INVL */
#define NACK_NOK 2 /* data copyback or transfer demanded; COPYBACK/
		      COPYBACK_INVL */

#endif

