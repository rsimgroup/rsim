/*
  module.c

  Provides some generic functions that are used by all modules
  in the memory system simulator.

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
#include "MemSys/module.h"
#include "MemSys/req.h"
#include "MemSys/net.h"
#include "MemSys/misc.h"

#include "MemSys/cpu.h"
#include "MemSys/cache.h"
#include "MemSys/directory.h"
#include "MemSys/bus.h"
#include "MemSys/arch.h"
#include "Processor/simio.h"

#include <malloc.h>

/*****************************************************************************/
/* ModuleInit: Called by all routines initializing a shared memory           */
/* module, in order to initialize parts of the structure that is common      */
/* to all modules.  The bulk of this routine initializes the port data       */
/* structures.                                                               */
/*****************************************************************************/


void ModuleInit (SMMODULE *mptr, int node_num, int stat_level, rtfunc routing, void *Delays, int ports, int q_size)
#if 0
SMMODULE *mptr;			/* Pointer to module to be initialized       */
int node_num;
int stat_level;                 /* Statistics level of this module           */
rtfunc routing;			/* Pointer to routing function               */
struct Delays *Delays;		/* Pointer to user-defined structure for delays*/
int ports;                      /* Number of ports */
int q_size;                     /* Size of queues in each port */
#endif
{
  int i;

  mptr->state = WAIT_INQ_EMPTY; /* start it out waiting for new input */
  mptr->num_ports = ports; /* set number of ports */
  mptr->node_num = node_num; /* assign node number */
  mptr->stat_level = stat_level; 

  /* Allocate port arrays */
  mptr->in_port_ptr = (SMPORT **)malloc(sizeof(SMPORT *)*(ports));
  mptr->out_port_ptr = (SMPORT **)malloc(sizeof(SMPORT *)*(ports));

  if (mptr->in_port_ptr == NULL || mptr->out_port_ptr == NULL)
    YS__errmsg("ModuleInit(): Malloc failed");


  /* Allocate and initialize  each new port */
  for (i=0; i< ports; i++) {
    mptr->in_port_ptr[i] = NULL;
    mptr->out_port_ptr[i] = (SMPORT *)YS__PoolGetObj(&YS__SMPortPool);
    mptr->out_port_ptr[i]->port_num = i;
    mptr->out_port_ptr[i]->mptr = mptr; /* associate queue with this module */
    mptr->out_port_ptr[i]->width = 0; /* width specified at connection time */
    mptr->out_port_ptr[i]->q_sz_tot = q_size; /* maximum number of entries */
    mptr->out_port_ptr[i]->q_size = 0;  /* queues initially empty */ 
    mptr->out_port_ptr[i]->head = NULL; 
    mptr->out_port_ptr[i]->tail = NULL;
    mptr->out_port_ptr[i]->ov_req = NULL; /* One overflow request for each queue */
  }

  /* set routing function and Delays for module */
  mptr->routing = routing; 
  mptr->Delays = Delays;
  
  /* Initialize module statistics */
  mptr->num_ref = 0;
  mptr->num_miss = 0;
  mptr->latency = 0.0;
  mptr->start_time = 0.0;
  mptr->num_lat = 0;
  mptr->utilization = 0.0;

  /* Set cycle-by-cycle simulation fields */
  mptr->inq_empty = 1; /* currently no incoming requests */
  mptr->pipe_empty = 1; /* currently no requests being processed */
}

/*****************************************************************************/
/* StatReportAll: Prints out statistics collected by the various modules of  */
/* the memory system. Also prints out network statistics.                    */
/*****************************************************************************/

void StatReportAll()
{
  int i;
  double net_util[2], net_util_total;
  int req_buf_reduced = 0, req_oport_reduced = 0, reply_buf_reduced = 0, reply_oport_reduced = 0;

  fprintf(simout,"\nTIME FOR EXECUTION:\t%g\n",YS__Simtime); 

  CacheStatReportAll(); /* prints out all stats for caches */
  fflush(simout);
  WBufferStatReportAll(); /* prints out all stats for write-buffers */
  fflush(simout);

#ifdef DETAILED_STATS_INTERLEAVING
  fprintf(simout,"\n### Memory Interleaving Statistics ###\n\n");
  for (i=0; i< YS__NumNodes; i++)
    {
      StatrecReport(InterleavingStats[i]);
    }  
  fflush(simout);
#endif
    
  BusStatReportAll();  /* prints out all stats for busses */
  fflush(simout);

  if (YS__NumNodes != 1) /* print system and network statistics, if present */
    {
      fprintf(simout,"\n#### General System Statistics ####\n\n");
      StatrecReport(CoheNumInvlHist);
      
      fprintf(simout,"\n#### REQUEST NET STATISTICS ####\n\n");
      if (PktNumHopsHist[REQ_NET])		
	StatrecReport(PktNumHopsHist[REQ_NET]); /* Report statistics for # of hops
						   traveled by packets in REQ_NET */
      
      StatrecReport(PktSzHist[REQ_NET]); /* Report statistics for size of packets in REQ_NET */
      
      fprintf(simout,"\n#### Request Time Statistics ####\n");
      
      if (PktSzTimeTotalMean[REQ_NET]) {
	for (i=0; i<NUM_SIZES; i++) {
	  if (rev_index[i] != -1) {
	    /* statistics for packets of varying sizes */
	    fprintf(simout,"\n\nPacket Size: %d flits\n",rev_index[i]);
	    StatrecReport(PktSzTimeTotalMean[REQ_NET][i]);
	    StatrecReport(PktSzTimeNetMean[REQ_NET][i]);
	    StatrecReport(PktSzTimeBlkMean[REQ_NET][i]);
	  }
	}
      }
      if (PktNumHopsHist) 		
	if (PktHpsTimeTotalMean[REQ_NET]) {
	  /* statistics for packets of varying hops */
	  for (i=0; i<(NUM_HOPS+1); i++) {
	    fprintf(simout,"\n\nNum Hops: %d\n",i);
	    StatrecReport(PktHpsTimeTotalMean[REQ_NET][i]);
	    StatrecReport(PktHpsTimeNetMean[REQ_NET][i]);
	    StatrecReport(PktHpsTimeBlkMean[REQ_NET][i]);
	  }
	}
      /* statistics for times spent by packets in REQ_NET */
      fprintf(simout,"\n\nTotal Time Stats:\n");
      StatrecReport(PktTOTimeTotalMean[REQ_NET]);
      StatrecReport(PktTOTimeNetMean[REQ_NET]);
      StatrecReport(PktTOTimeBlkMean[REQ_NET]);
      
      /* Repeat above actions for REPLY network rather than REQUEST network... */
      
      fprintf(simout,"\n\n#### REPLY NET STATISTICS ####\n\n");
      if (PktNumHopsHist)	
	StatrecReport(PktNumHopsHist[REPLY_NET]);
      StatrecReport(PktSzHist[REPLY_NET]);
      
      fprintf(simout,"\n#### Reply Time Statistics ####\n");
      
      if (PktSzTimeTotalMean[REPLY_NET]) {
	for (i=0; i<NUM_SIZES; i++) {
	  if (rev_index[i] != -1) {
	    fprintf(simout,"\n\nPacket Size: %d flits\n",rev_index[i]);
	    StatrecReport(PktSzTimeTotalMean[REPLY_NET][i]);
	    StatrecReport(PktSzTimeNetMean[REPLY_NET][i]);
	    StatrecReport(PktSzTimeBlkMean[REPLY_NET][i]);
	  }
	}
      }
      
      if (PktNumHopsHist[REPLY_NET]) 		
	if (PktHpsTimeTotalMean[REPLY_NET]) {
	  for (i=0; i<NUM_HOPS; i++) {
	    fprintf(simout,"\n\nNum Hops: %d\n",i);
	    StatrecReport(PktHpsTimeTotalMean[REPLY_NET][i]);
	    StatrecReport(PktHpsTimeNetMean[REPLY_NET][i]);
	    StatrecReport(PktHpsTimeBlkMean[REPLY_NET][i]);
	  }
	}
      fprintf(simout,"\n\nTotal Time Stats:\n");
      StatrecReport(PktTOTimeTotalMean[REPLY_NET]);
      StatrecReport(PktTOTimeNetMean[REPLY_NET]);
      StatrecReport(PktTOTimeBlkMean[REPLY_NET]);
      
      
      if (buf_index[REPLY_NET] || oport_index[REPLY_NET]) {
	/* statistics on utilizations of individual buffers and ports in the
	   network */
	fprintf(simout, "\nBUFFER AND OPORT UTILIZATIONS\n");
	
	net_util[REPLY_NET] = 0.0;
	if (buf_index[REPLY_NET]) 
	  buf_index[REPLY_NET] --;  
	if (oport_index[REPLY_NET])
	  oport_index[REPLY_NET] --;
	for (i=0; i< buf_index[REPLY_NET]; i++) {
	  if (BufTable[REPLY_NET][i]->channel_busy != 0.0)
	    {
	      fprintf(simout, "Utilization of buffer %s in network Reply = %g\n",
		      WhichBuf[REPLY_NET][i], BufTable[REPLY_NET][i]->channel_busy/
		      (YS__Simtime - BufTable[REPLY_NET][i]->time_of_last_clear) );
	      net_util[REPLY_NET] += BufTable[REPLY_NET][i]->channel_busy/
		(YS__Simtime - BufTable[REPLY_NET][i]->time_of_last_clear) ;
	    }
	  else
	    reply_buf_reduced++;
	}
	for (i=0; i< oport_index[REPLY_NET]; i++) {
	  if (OportTable[REPLY_NET][i]->channel_busy != 0.0)
	    {
	      fprintf(simout, "Utilization of oport %d in network Reply = %g\n",
		      i, OportTable[REPLY_NET][i]->channel_busy/
		      (YS__Simtime - OportTable[REPLY_NET][i]->time_of_last_clear)); 
	      net_util[REPLY_NET] +=  OportTable[REPLY_NET][i]->channel_busy /
		(YS__Simtime - OportTable[REPLY_NET][i]->time_of_last_clear) ;
	    }
	  else
	    reply_oport_reduced++;
	}
	
	/* calculate network utilization */
	net_util_total = net_util[REPLY_NET];
	net_util[REPLY_NET] = net_util[REPLY_NET]/(double)(buf_index[REPLY_NET]+oport_index[REPLY_NET]-reply_buf_reduced-reply_oport_reduced); 
	fprintf(simout,"Reply Network Utilization: %g\n",net_util[REPLY_NET]);
      }
      

      /* Repeat above for REQUEST network */
      if (buf_index[REQ_NET] || oport_index[REQ_NET]) {
	net_util[REQ_NET] = 0.0;
	if (buf_index[REQ_NET]) 
	  buf_index[REQ_NET] --;
	if (oport_index[REQ_NET])
	  oport_index[REQ_NET] --;
	for (i=0; i< buf_index[REQ_NET]; i++) {
	  if (BufTable[REQ_NET][i]->channel_busy != 0.0)
	    {
	      fprintf(simout, "Utilization of buffer %s in network Request = %g\n",
		      WhichBuf[REQ_NET][i], BufTable[REQ_NET][i]->channel_busy/
		      (YS__Simtime - BufTable[REQ_NET][i]->time_of_last_clear));
	      net_util[REQ_NET] +=  (BufTable[REQ_NET][i]->channel_busy / 
				     (YS__Simtime - BufTable[REQ_NET][i]->time_of_last_clear)) ;
	    }
	  else
	    req_buf_reduced ++;
	}
	for (i=0; i< oport_index[REQ_NET]; i++) {
	  if (OportTable[REQ_NET][i]->channel_busy != 0.0)
	    {
	      fprintf(simout, "Utilization of oport %d in network Request = %g\n",
		      i, OportTable[REQ_NET][i]->channel_busy/
		      (YS__Simtime - OportTable[REQ_NET][i]->time_of_last_clear) ); 
	      net_util[REQ_NET] +=  (OportTable[REQ_NET][i]->channel_busy / 
				     (YS__Simtime - OportTable[REQ_NET][i]->time_of_last_clear)) ;
	    }
	  else
	    req_oport_reduced++;
	}

	/* calculate network utilization */
	net_util_total += net_util[REQ_NET];
	net_util[REQ_NET] = net_util[REQ_NET]/(double)(buf_index[REQ_NET]+oport_index[REQ_NET]-req_buf_reduced-req_oport_reduced); 
	fprintf(simout,"Req Network Utilization: %g\n",net_util[REQ_NET]);
      }
      if (buf_index[REQ_NET] || oport_index[REQ_NET] || buf_index[REQ_NET] || oport_index[REQ_NET]) {
	net_util_total = net_util_total/(double)(buf_index[REQ_NET]+oport_index[REQ_NET]+buf_index[REPLY_NET]+oport_index[REPLY_NET]-req_buf_reduced-reply_buf_reduced-req_oport_reduced-reply_oport_reduced); 
	fprintf(simout,"Total Network Utilization: %g\n", net_util_total);
      }
      NetworkStatRept(); /* print additional stats for Network, if any */
    }
}

/*****************************************************************************/
/* StatClearAll: Resets collection of statistics collected by the various    */
/* modules of the memory system and network statistics.                      */
/*****************************************************************************/

void StatClearAll()
{
  int i; 
  extern int YS__NumNodes;
  
  CacheStatClearAll();
  WBufferStatClearAll();
  BusStatClearAll();
  DirStatClearAll();
  SmnetStatClearAll();

  if (YS__NumNodes != 1)
    {
      StatrecReset(CoheNumInvlHist);
      
      if (PktNumHopsHist[REQ_NET])		
	StatrecReset(PktNumHopsHist[REQ_NET]);
      StatrecReset(PktSzHist[REQ_NET]);
      
      if (PktSzTimeTotalMean[REQ_NET]) {
	for (i=0; i<NUM_SIZES; i++) {
	  if (rev_index[i] != -1) {
	    StatrecReset(PktSzTimeTotalMean[REQ_NET][i]);
	    StatrecReset(PktSzTimeNetMean[REQ_NET][i]);
	    StatrecReset(PktSzTimeBlkMean[REQ_NET][i]);
	  }
	}
      }
      if (PktNumHopsHist) 		
	if (PktHpsTimeTotalMean[REQ_NET]) {
	  for (i=0; i<(NUM_HOPS+1); i++) {
	    StatrecReset(PktHpsTimeTotalMean[REQ_NET][i]);
	    StatrecReset(PktHpsTimeNetMean[REQ_NET][i]);
	    StatrecReset(PktHpsTimeBlkMean[REQ_NET][i]);
	  }
	}
      StatrecReset(PktTOTimeTotalMean[REQ_NET]);
      StatrecReset(PktTOTimeNetMean[REQ_NET]);
      StatrecReset(PktTOTimeBlkMean[REQ_NET]);
      
      if (PktNumHopsHist)	
	StatrecReset(PktNumHopsHist[REPLY_NET]);
      StatrecReset(PktSzHist[REPLY_NET]);
      
      if (PktSzTimeTotalMean[REPLY_NET]) {
	for (i=0; i<NUM_SIZES; i++) {
	  if (rev_index[i] != -1) {
	    StatrecReset(PktSzTimeTotalMean[REPLY_NET][i]);
	    StatrecReset(PktSzTimeNetMean[REPLY_NET][i]);
	    StatrecReset(PktSzTimeBlkMean[REPLY_NET][i]);
	  }
	}
      }
      
      if (PktNumHopsHist[REPLY_NET]) 		
	if (PktHpsTimeTotalMean[REPLY_NET]) {
	  for (i=0; i<NUM_HOPS; i++) {
	    StatrecReset(PktHpsTimeTotalMean[REPLY_NET][i]);
	    StatrecReset(PktHpsTimeNetMean[REPLY_NET][i]);
	    StatrecReset(PktHpsTimeBlkMean[REPLY_NET][i]);
	  }
	}
      StatrecReset(PktTOTimeTotalMean[REPLY_NET]);
      StatrecReset(PktTOTimeNetMean[REPLY_NET]);
      StatrecReset(PktTOTimeBlkMean[REPLY_NET]);
      
    }
  TotNumSamples = 0;
  TotBlkTime = 0.0;
  /*** Req stat records not reset yet ***/


  if (buf_index[REPLY_NET] || oport_index[REPLY_NET]) {
    for (i=0; i< buf_index[REPLY_NET]; i++) {
      BufTable[REPLY_NET][i]->channel_busy = 0.0;
      BufTable[REPLY_NET][i]->time_of_last_clear = YS__Simtime;
    }
    for (i=0; i< oport_index[REPLY_NET]; i++) {
      OportTable[REPLY_NET][i]->channel_busy = 0.0;
      OportTable[REPLY_NET][i]->time_of_last_clear = YS__Simtime;
    }
  }

  if (buf_index[REQ_NET] || oport_index[REQ_NET]) {
    for (i=0; i< buf_index[REQ_NET]; i++) {
      BufTable[REQ_NET][i]->channel_busy = 0.0;
      BufTable[REQ_NET][i]->time_of_last_clear = YS__Simtime;
    }
    for (i=0; i< oport_index[REQ_NET]; i++) {
      OportTable[REQ_NET][i]->channel_busy = 0.0;
      OportTable[REQ_NET][i]->time_of_last_clear = YS__Simtime;
    }
  }
  



  
}

/*****************************************************************************/
/* SMModuleStatReport: reports statistics of common module fields            */
/*****************************************************************************/

void SMModuleStatReport (mptr, hitpr, lat, util)
SMMODULE *mptr;
double *hitpr, *lat, *util;
{
  int num_hit, l, i;
  char s[18];

  num_hit = mptr->num_ref - mptr->num_miss;
  if (mptr->num_ref) *hitpr = (double)num_hit/(double)mptr->num_ref * 100.0; 
  else *hitpr = 0.0;
  if (mptr->num_miss == 0)
    *lat = 0.0;
  else {
    if (mptr->num_lat)
      *lat = (double)mptr->latency / (double)mptr->num_lat;
    else
      *lat = (double)mptr->latency / (double)mptr->num_miss;
  }
  *util = mptr->utilization / GetSimTime();

  l = strlen(mptr->name);
  l = 10 - l;
  for (i=0; i<l; i++) 
    s[i] = ' ';
  s[i] = '\0';
  fprintf(simout,"\nName          Num_Ref          Num_Hit;Hit_Rate   Miss_latency     Utilization\n");
  fprintf(simout,"%s%s%10d       %10d(%5.2g%%)     %6.4g             %6.4g\n", 
	  mptr->name, s,mptr->num_ref, num_hit, *hitpr, *lat, *util);
}

/*****************************************************************************/
/* SMModuleStatClear: reports statistics of common module fields             */
/*****************************************************************************/

void SMModuleStatClear (mptr)
SMMODULE *mptr;
{
  mptr->num_ref  = 0;
  mptr->num_miss = 0;
  mptr->num_lat = 0;
  mptr->latency  = 0.0;
  mptr->utilization = 0.0;

}
