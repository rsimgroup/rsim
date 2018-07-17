/* cache.c

   This file contains cache intialization and stat routines
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

   
#include "MemSys/simsys.h"
#include "MemSys/cache.h"
#include "MemSys/pipeline.h"
#include "MemSys/mshr.h"
#include "MemSys/req.h"
#include "MemSys/net.h"
#include "MemSys/misc.h"
#include "Processor/capconf.h"
#include "Processor/simio.h"

#include <malloc.h>

extern int MAX_MEM_OPS;

extern ARG *AllL2Caches[];
extern ARG *AllL1Caches[];

/* Statistics -- global variables */
int    cache_index=0;
CACHE  *cache_ptr[256];

/* wrb_buf_extra is used if the user wants more WRB buf space than the
number of requests at a given time -- thus allowing longer REPLY net
stalls before blocking later requests */
unsigned wrb_buf_extra = 0;

int wrb_buf_size; /* gets set by NewCache routine */
int MAX_COALS = 16; /* number of coalesced requests per MSHR or wbuf entry */

int first = 0;

struct state;

/*****************************************************************************/
/* NewCache: initializes and returns a pointer to a cache module             */
/*****************************************************************************/


CACHE *NewCache(char *name, int node_num, int stat_level, rtfunc routing,
		struct Delays *Delays, int ports_abv, int ports_blw, 
		int cache_type, int size, int line_size, int set_size, int cohe_type,
		int allo_type, int cache_type1, int adap_type, int replacement,
		func cohe_rtn, int tagdelay, int tagpipes, int tagwidths[],
		int datadelay, int datapipes, int datawidths[],
		int netsend_port, int netrcv_port, LINKPARA *link_para, int max_mshrs,
		int mynum, int unused)
#if 0
char   *name;		/* Name of the module upto 32 characters long          */
int    node_num;	/* Node number used in dir-based coherence */
int    stat_level;	/* Level of statistics collection for this module      */
rtfunc routing;		/* Routing function of the module                      */
char   *Delays;		/* Pointer to user-defined structure for delays        */
int    ports_abv;	/* Number of ports above this module                   */
int    ports_blw;	/* Number of ports below this module                   */
			/* Cache Attributes                                    */
int    cache_type;
int    size, line_size, set_size;
int    cohe_type, allo_type, cache_type1, adap_type;
int    replacement;
func   cohe_rtn;
int    netsend_port, netrcv_port;

int tagdelay, tagpipes, datadelay, datapipes;
int tagwidths[],datawidths[];

int    max_mshrs; 
LINKPARA *link_para;
#endif
{
  CACHE *captr;
  ARG *arg;
  char evnt_name[32];
  int temp;
  int i ;
  
  /* Set global block size and block bits variables */
  /* assumes all caches have same line size */
  blocksize = line_size;	/* Initialize global line size field */
  block_bits = 0; 
  temp = blocksize;
  while (temp) {	/* check that block size is a power of two */
    temp = temp >> 1;
    block_bits++;
  }
  block_bits--;
  
  
  if (!first && (cohe_rtn == cohe_sl || cohe_rtn == cohe_pr)) {
    first = 1;
    setup_tables();  /* set up tables used by the above mentioned coherence routines */
  }
  
  captr = (CACHE *)YS__PoolGetObj(&YS__CachePool);
  if (cache_index < 256) {
    cache_ptr[cache_index] = captr;
    cache_index ++;
  }
  
  captr->id = YS__idctr++;	/* System assigned unique ID   */
  strncpy(captr->name, name,31); /* Copy module name */
  captr->name[31] = '\0';
  captr->module_type = CAC_MODULE; /* Initialize module type as a cache */
  
  /* ModuleInit
     (mptr, node_num, stat_level, routing, Delays, ports, q_size) */
  ModuleInit ((SMMODULE *)captr, node_num, stat_level, routing, Delays,
	      ports_abv + ports_blw, DEFAULT_Q_SZ);
  /* Initialize data structures comman to all modules    */

  captr->num_ports_abv = ports_abv; /* Number of ports above */
  /* Create new event */
  arg = (ARG *)malloc(sizeof(ARG)); /* Setup arguments for cache event */ 
  arg->mptr = (SMMODULE *)captr; 

  sprintf(evnt_name, "%s_cachesim",name);
  /* "_cachesim" is appended to 10 characters of module name */

  /* Fill in the cache level type and the appropriate entry in the
     array of cache pointers */
  if (cache_type == RC_DIR_ARCH){
    captr->cache_level_type = FIRSTLEVEL_WT;
    AllL1Caches[mynum] = arg;
  }
  else if (cache_type == RC_DIR_ARCH_WB){
    captr->cache_level_type = FIRSTLEVEL_WB;
    AllL1Caches[mynum] = arg;
  }
  else if (cache_type == RC_LOCKUP_FREE){
    captr->cache_level_type = SECONDLEVEL;
    AllL2Caches[mynum] = arg;
  }
  else {
    fprintf(simout,"ERROR: NewCache unknown architecture type\n");
    YS__errmsg("NewCache unknown architecture type\n");
  }

  /* Used for round-robin port scheduling */
  captr->rm_q = (char *)malloc(sizeof(RMQ));
  if (captr->rm_q == NULL)
    YS__errmsg("NewCache(): malloc failed");
  ((RMQ *)(captr->rm_q))->u1.abv = 0; /* Used for round-robin scheduling of ports */
  if (cache_type == UNI_ARCH || cache_type == BUS_ARCH || netsend_port >= 0)
    ((RMQ *)(captr->rm_q))->u2.blw = 0;
  else {
    ((RMQ *)(captr->rm_q))->u2.blw_req = 2;
    ((RMQ *)(captr->rm_q))->u3.blw_rep = 4;
  }


  captr->req = NULL; /* no req currently being processed */
  captr->in_port_num = -1; /* no active input port */
  if (link_para)
    {
      YS__errmsg("Unknown cache type!\n");
    }
  else if ((cache_type == RC_DIR_ARCH) || (cache_type == RC_LOCKUP_FREE) || (cache_type == RC_DIR_ARCH_WB))
      captr->wakeup = NULL; /* caches do not use wakeup functions */
  else{
    YS__errmsg("Unknown cache type!\n");
  }
  captr->handshake = NULL; /* caches do not use handshake functions */
  
  captr->cache_type = cache_type1; /* Cache parameters */

  /* Set cache size, line size, set associativity parameters */
  captr->size = size;
  captr->linesz = line_size;
  captr->setsz = set_size;
  
  captr->types.cohe_type = cohe_type; /* Coherence parameters */
  captr->types.allo_type = allo_type; /* allocation type */
  captr->types.adap_type = adap_type; /* adaptivity? (not supported in RSIM) */
  captr->types.replacement = replacement; /* replacement policy */
  
  captr->cohe_rtn = cohe_rtn; /* cohe_rtn to call: cohe_pr for L1 cache,
				                   cohe_sl for L2 cache */

  captr->max_mshrs = max_mshrs; /* # of MSHRs: this is used in init_cache */
  init_cache(captr);		/*Initialize cache data structures */

  /* set network ports, if any */
  captr->netsend_port = netsend_port;
  captr->netrcv_port = netrcv_port;
  
  /* Create new mshrs and intialize them */
  captr->mshrs = malloc(sizeof(MSHRITEM) * max_mshrs);
  for (i=0; i<captr->max_mshrs; i++)
    {
      captr->mshrs[i].valid=0;
    }
  captr->mshr_count=0; /* counts WRBs in MSHRs, etc */
  captr->reqmshr_count=0; /* only counting data requests */
  captr->reqs_at_mshr_count=0;

  /* Create and initialize the pipes */
  captr->pipe = (Pipeline **)malloc(sizeof(Pipeline *) * (datapipes + tagpipes));
  for (i=0; i< datapipes; i++)
    {
      captr->pipe[i] = NewPipeline(datawidths[i],datadelay);
    }
  for (i=0; i< tagpipes; i++)
    {
      captr->pipe[i+datapipes] = NewPipeline(tagwidths[i],tagdelay);
    }

  /* Create and initialize the write-back buffer */
  if (captr->cache_level_type == SECONDLEVEL)
    {
      wrb_buf_size = max_mshrs + wrb_buf_extra;
      if (wrb_buf_size == 0)
	{
	  captr->wrb_buf = NULL;
	}
      else
	{
	  captr->wrb_buf = malloc(wrb_buf_size * sizeof (long));
	  for (i=0; i <wrb_buf_size; i++)
	    {
	      captr->wrb_buf[i] = NULL;
	    }
	}
    }
  else
    {
      captr->wrb_buf = NULL; /* shouldn't be used in 1st level anyway */
    }

  captr->wrb_buf_used = 0;

  return captr;
}

/*****************************************************************************/
/* Cache statistics functions                                                */
/*****************************************************************************/

void CacheStatReportAll() /* report stats for all caches */
{
  int i;
  int caches=0;
  if (cache_index) {
    if (cache_index == 256) 
      YS__warnmsg("Greater than 256 caches created; statistics for the first 100 will be reported by CacheStatReportAll");

    fprintf(simout,"\n##### Cache Statistics #####\n");


    for (i=0; i< cache_index; i++) 
      caches  += CacheStatReport (cache_ptr[i]); 

  }
}

void CacheStatClearAll() /* clear all cache stats */
{
  int i;
  if (cache_index) {
      for (i=0; i< cache_index; i++) 
	CacheStatClear (cache_ptr[i]); 
  }
}

int CacheStatReport (CACHE *captr) /* report stats for this cache */
{
  FILE *out;
  double hitpr, lat, util;

  int i,j;

  if (captr->num_ref == 0) /* an unused cache */
    return 0; /* nothing to report */
  
  out=simout;

  SMModuleStatReport(captr , &hitpr, &lat, &util); /* First report basic module stats */

  /* Hit rate and miss rate */
  fprintf(out,"Num_hit: %d  Num_miss: %d Num_lat: %d\n",(captr->num_ref-captr->num_miss), captr->num_miss, captr->num_lat);

  /* Now split up the hits and misses into each different miss type
   for each different type of demand access (READ, WRITE, RMW, each
   prefetch type) */
  for (j=0; j<= RMW-READ; j++)
    {
      for (i=CACHE_HIT; i < NUM_CACHE_MISS_TYPES; i++)
	{
	  fprintf(out,"DEMAND %s\t%s: %-8d (%.3f,%.3f)\n",Req_Type[j+READ],
		  CacheMissTypes[i],captr->stat.demand_miss[j][i],
		  (double)captr->stat.demand_miss[j][i]/(double)captr->stat.demand_ref[j],
		  (double)captr->stat.demand_miss[j][i]/(double)captr->num_ref);
	}
    }
  
  for (j=0; j<= L2READ_PREFETCH-L1WRITE_PREFETCH; j++)
    {
      for (i=CACHE_HIT; i < NUM_CACHE_MISS_TYPES; i++)
	{
	  fprintf(out,"PREF   %s\t%s: %-8d (%.3f,%.3f)\n",Req_Type[j+L1WRITE_PREFETCH],
		  CacheMissTypes[i],captr->stat.pref_miss[j][i],
		  (double)captr->stat.pref_miss[j][i]/(double)captr->stat.pref_ref[j],
		  (double)captr->stat.pref_miss[j][i]/(double)captr->num_ref);
	}
    }

  /* Also give the network latencies seen for each of the miss types.
     Meaningful only at L2 cache (really bus+dir+network...) */
  for (j=0; j<= RMW-READ; j++)
    {
      fprintf(out,"DEMAND network miss %s\t: %d @ %.3f [stddev %.3f]\n",
	      Req_Type[j+READ],
	      StatrecSamples(captr->net_demand_miss[j]),
	      StatrecMean(captr->net_demand_miss[j]),
	      StatrecSdv(captr->net_demand_miss[j]));
    }
  
  for (j=0; j<= L2READ_PREFETCH-L1WRITE_PREFETCH; j++)
    {
      fprintf(out,"PREF network miss %s\t: %d @ %.3f [stddev %.3f]\n",
	      Req_Type[j+L1WRITE_PREFETCH],
	      StatrecSamples(captr->net_pref_miss[j]),
	      StatrecMean(captr->net_pref_miss[j]),
	      StatrecSdv(captr->net_pref_miss[j]));
    }


  /* Report statistics on MSHR occupancy */
  fprintf(out, "\nMean MSHR occupancy: %.3f [stddev %.3f]\n",StatrecMean(captr->mshr_occ),
	  StatrecSdv(captr->mshr_occ));
  StatrecReport(captr->mshr_occ);
  fprintf(out, "\nMean MSHR request occupancy: %.3f [stddev %.3f]\n",StatrecMean(captr->mshr_req_count),
	  StatrecSdv(captr->mshr_req_count));
  StatrecReport(captr->mshr_req_count);

  /* Prefetch-related statistics for lateness and earlyness: how late
     were late prefetches, and how early were useful prefetches */
  fprintf(out, "\nMean lateness: %.3f [stddev %.3f]\n",StatrecMean(captr->pref_lateness),
	  StatrecSdv(captr->pref_lateness));
  StatrecReport(captr->pref_lateness);
  fprintf(out, "\nMean earlyness: %.3f [stddev %.3f]\n",StatrecMean(captr->pref_earlyness),
	  StatrecSdv(captr->pref_earlyness));
  StatrecReport(captr->pref_earlyness);

  /* Report stalls for various conditions on incoming requests (mostly
     MSHR related, but not always) */
  fprintf(out, "MSHR PIPE STALLS: WAR (%d)\tMSHR_COHE (%d)\tPEND_COHE (%d)\tMAX_COAL (%d)\tCONF (%d)\tFULL (%d)\n",
	  captr->stat.pipe_stall_MSHR_WAR,
	  captr->stat.pipe_stall_MSHR_COHE,
	  captr->stat.pipe_stall_PEND_COHE,
	  captr->stat.pipe_stall_MSHR_COAL,
	  captr->stat.pipe_stall_MSHR_CONF,
	  captr->stat.pipe_stall_MSHR_FULL);


  /* Report counts of various COHE message types and
     the manner in which CACHE handles them. */
  fprintf(out,"\nCohes:\t%d\n",captr->stat.cohe);
  fprintf(out,"Cohes nacked:\t%d\n",captr->stat.cohe_nack);
  fprintf(out,"Cohes stalled:\t%d\n",captr->stat.cohe_stall_PEND_COHE);
  fprintf(out,"Cohe-replys:\t%d\n",captr->stat.cohe_reply);
  fprintf(out,"Cohe-replys merged:\t%d\n",captr->stat.cohe_reply_merge);
  fprintf(out,"Cohe-replys merged but ignored for PR:\t%d\n",captr->stat.cohe_reply_merge_ignore);
  fprintf(out,"Cohe-replys nacked:\t%d\n",captr->stat.cohe_reply_nack);
  fprintf(out,"Cohe-replys nacked with docohe:\t%d\n",captr->stat.cohe_reply_nack_docohe);
  fprintf(out,"Cohe-replys nacked with failed merge:\t%d\n",captr->stat.cohe_reply_nack_mergefail);
  fprintf(out,"Cohe-replys nack-pended:\t%d\n",captr->stat.cohe_reply_nack_pend);
  fprintf(out,"Cohe-reply  nack-pends propagated:\t%d\n",captr->stat.cohe_reply_prop_nack_pend);
  fprintf(out,"Cohe-reply  unsolicited WRBs:\t%d\n",captr->stat.cohe_reply_unsolicited_WRB);

  fprintf(out,"Cohe cache-to-cache requests:\t%d\n",captr->stat.cohe_cache_to_cache_good+captr->stat.cohe_cache_to_cache_fail);
  fprintf(out,"Cohe cache-to-cache failures:\t%d\n",captr->stat.cohe_cache_to_cache_fail);

  /* How many replies were bounched back because of Upgrade conflicts? */
  fprintf(out,"\nReplies nacked: %d\n",captr->stat.replies_nacked);

  /* How many RARs were processed here? */
  fprintf(out,"RARs handled: %d\n\n",captr->stat.rars_handled);

  /* How many WRBs were sent to a higher level cache for subset enforcement? */
  fprintf(out,"WRB inclusions sent: %d\tfrom L2,E: %d\treal: %d\trace: %d\trepeats: %d\n",captr->stat.wb_inclusions_sent,captr->stat.wb_prcl_inclusions_sent,
	  captr->stat.wb_inclusions_real,captr->stat.wb_inclusions_race,captr->stat.wb_repeats);
  fprintf(out,"Victims: %s: %d\t%s: %d\t%s: %d\n",
	  State[SH_CL],captr->stat.shcl_victims,
	  State[PR_CL],captr->stat.prcl_victims,
	  State[PR_DY],captr->stat.prdy_victims);

  /* How often did a REQ stall because it matched an outstanding WRB in
     WRB-buf? How often because there was no space in the WRB-buf for
     the REQUEST to book? */
  
  fprintf(out,"Pipe stalls for REQs matching WRB replacement: %d\tfor WRBBUF full: %d\n\n",captr->stat.pipe_stall_WRB_match,captr->stat.pipe_stall_WRBBUF_full);

  /* Prefetching statistics: basic types (dropped, unnecessary, late, etc.) */
  fprintf(out,"Pref_Total: %d\nPref_Dropped: %d ( %.2f %%)\nPref_Unnecessary: %d ( %.2f %%)\n",
	  captr->stat.pref_total,
	  captr->stat.dropped_pref,100.0 *captr->stat.dropped_pref/captr->stat.pref_total,
	  captr->stat.pref_unnecessary, 100.0 *captr->stat.pref_unnecessary/captr->stat.pref_total );
  fprintf(out,"Pref_Late: %d ( %.2f %%)\nPref_Useful: %d ( %.2f %%)\nPref_Upgrade: %d ( %.2f %%)\nPref_Useless: %d ( %.2f %%)\nPref_Invalidated: %d ( %.2f %%)\nPref_Downgraded: %d ( %.2f %%)\nPref_Damaging: %d ( %.2f %%)\n\n",
	  captr->stat.pref_late,100.0*captr->stat.pref_late/captr->stat.pref_total,
	  captr->stat.pref_useful,100.0*captr->stat.pref_useful/captr->stat.pref_total,
	  captr->stat.pref_useful_upgrade,100.0*captr->stat.pref_useful_upgrade/captr->stat.pref_total,
	  captr->stat.pref_useless,100.0*captr->stat.pref_useless/captr->stat.pref_total,
	  captr->stat.pref_useless_cohe,100.0*captr->stat.pref_useless_cohe/captr->stat.pref_total,
	  captr->stat.pref_downgraded,100.0*captr->stat.pref_downgraded/captr->stat.pref_total,
	  captr->stat.pref_damaging,100.0*captr->stat.pref_damaging/captr->stat.pref_total);

  
  return 1;
}

void CacheStatClear (CACHE *captr) /* clear out all statistics */
{
  int i;
  SMModuleStatClear(captr);
  
  captr->num_ref  = 0;
  captr->num_miss = 0;
  captr->utilization = 0.0;
  memset(&captr->stat,0,sizeof(CacheStatStruct)); /* zero this whole structure out */

  for (i=0; i < 3; i++)
    StatrecReset(captr->net_demand_miss[i]);
  for (i=0; i < 4; i++)
    StatrecReset(captr->net_pref_miss[i]);
  
  StatrecReset(captr->mshr_occ);
  StatrecReset(captr->mshr_req_count);
  StatrecReset(captr->pref_lateness);
  StatrecReset(captr->pref_earlyness);
  
}



