/*
  cpu.c
  
  This file contains the addrinsert function, which is used to start
  REQUESTs in the memory system simulator. This file also includes some
  functions for the PROCESSOR data structure
  
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
#include "MemSys/tr.cpu.h"
#include "MemSys/cpu.h"
#include "MemSys/misc.h"
#include "MemSys/module.h"
#include "MemSys/req.h"
#include "MemSys/arch.h"
#include "MemSys/net.h"
#include "MemSys/cache.h"
#include "Processor/memprocess.h"
#include "Processor/simio.h"
#include <malloc.h>

int dubref, addr, memacctype, addrinsert_val;
int prcr_index =0;
PROCESSOR *prcr_ptr[MAX_MEMSYS_PROCS];

static double grdfr, gwdfr, grsfr, gwsfr, grmwfr, ghitpr;
static double glat, gutil;
static int gnum_ref;

int *L1Q_FULL;        /* Used to flag L1Q_FULL status */

PROCESSOR *YS__ProcArray[MAX_MEMSYS_PROCS]; /* array containing all PROCESSOR
					     pointers; used in sending out
					     REQUESTs from memprocess.cc */

/*****************************************************************************/
/* addrinsert: This function is called to initiate a memory access from the  */
/* processor into the cache and memory simulator.                            */
/*****************************************************************************/

void addrinsert(struct state *proc, struct instance *inst, int inst_tag,
		unsigned addr, int memacctype, int dubref, int flagvar,
		int addrinsert_val, PROCESSOR *prptr, double memstarttime, double activestarttime)
{
  int node;
  REQ *req;
  int oport_num;

  node = prptr->id; /* which node is calling addrinsert */

  req = (REQ*)YS__PoolGetObj(&YS__ReqPool); /* Get new request and initialize */
  req->id = YS__idctr ++;	/* Unique id for request */

  /* set up statistics related fields */
  req->mem_start_time=memstarttime;
  req->issue_time = YS__Simtime;
  req->active_start_time = activestarttime;
  req->miss_type = mtUNK; /* type of miss incurred by this access */

  req->priority = 0; /* priority is unused in RSIM */

  req->address = addr; /* assign the address of the access */

  /* information to identify this REQUEST to the processor simulator */
  req->s.inst=inst;
  req->s.inst_tag = inst_tag;
  req->s.proc=proc;
  
  req->prcr_req_type = memacctype;  /* access type of reference */
  req->req_type = req->prcr_req_type;
  
  if (memacctype == L1READ_PREFETCH || memacctype == L1WRITE_PREFETCH ||
      memacctype == L2READ_PREFETCH || memacctype == L2WRITE_PREFETCH)
    {
      req->s.prefetch=1; /* note that these are prefetches */
    }
  
  req->address_type = DATA; /* only DATA accesses simulated currently */
  req->dubref = dubref;     /* discussed in memprocess.cc --
			       currently unused */
  req->size_st = WORDSZ +1; /* unused until lower memory levels */
  prcr_stat(prptr, req);    /* stats */
  
  if (dubref)
    {
      fprintf(simout,"dubref field reserved for future expansion");
    }
  else
    req->size_req = WORDSZ; /* unused until lower memory levels */
  dubref = 0;
  req->dubref = 0;


  /* Now initialize request structures for routing and other
     simulation details */
  req->src_node = prptr->node_num;
  req->s.dir = REQ_FWD;	/* send this request down */
  req->s.ret = RET;
  req->s.route = BLW;
  req->s.type = REQUEST;
  req->s.nc = 0;
  req->s.dirdone = 0;
#ifdef DEBUG_SZ
  if (YS__Simtime > DEBUG_TIME)
    fprintf(simout,"%s\t%x\t%d\t%d\n",prptr->name, req, req->tag, req->size_st);
#endif

  /* Get out port number from routing routine -- routing1 for CPU */
  oport_num = prptr->routing((SMMODULE *)prptr, req);
  if (oport_num == -1)
    {
      fprintf(simout,"addrinsert(): Request leaving processor module \"%s\" cannot be routed\n",
	      prptr->name);
      YS__errmsg("addrinsert(): Routing function is unable to route this request");
    }

  /* an assertion to check */
  if(L1Q_FULL[prptr->node_num]){
    fprintf(simerr,"We should have already checked for this condtion!\n");
    YS__errmsg("Should not have called addrinsert without space in L1Q.\n");
  }

  /* new_add_req adds it to REQUEST input queue of L1 cache, which
     must then know that there is something on its input queue so as
     to be activated by cycle-by-cycle simulator. If this access fills
     up the port, the L1Q_FULL variable is set so that the processor
     does not issue any more memory references until the port clears
     up. */
  if(!new_add_req(prptr->out_port_ptr[oport_num], req)){ /* ov_req is now set */
    L1Q_FULL[prptr->node_num] = 1; /* can't add anything more now */
  }
}

/*****************************************************************************/
/* PROCESSOR Operations: create CPU modules and collect statistics for them  */
/*****************************************************************************/

PROCESSOR *NewProcessor(pname, id, slice, noiports, nooports, routingfn,
			sm_flag, sm_node_num, sm_stat_level, sm_routing, 
			sm_delays, sm_ports)
                                /* Creates a new processor                            */
    char *pname;                    /* User assigned name                                 */
    int id;                         /* user assigned id for the processor                 */
    double slice;                   /* Time slice for multiprogramming                    */
    int noiports;                   /* Number of network input ports from this processor  */
    int nooports;                   /* Number of network output ports to this processor   */
    int (*routingfn)(int,int,int);               /* Packet routing function; only used if noiports > 1 */
    int sm_flag;		    /* Set if this processor is used in MEMSIM            */
    int sm_node_num;  /* This node's number; Used for routing in dir. based architectures */
    int sm_stat_level;		/* Stat level for collecting stats for this module    */
    rtfunc sm_routing;		/* Routing routine for MEMSIM                         */
    char *sm_delays;		/* Ptr to structure that specifies delays to be used  */
    int  sm_ports;    		/* Number of shared memory ports for this module      */
{
    PROCESSOR *prptr;
    int i;
    
    
    prptr = (PROCESSOR*)YS__PoolGetObj(&YS__PrcrPool);
    strncpy(prptr->name,pname,31);
    prptr->name[31] = '\0';
    prptr->id = id;
    YS__ProcArray[id]=prptr;       /* For interfacing PROCESSOR state to MEMSYS processor */
    
    prptr->next = NULL;           /* For Pool organization */
    prptr->status = IDLE;         /* Idle or busy? */
    
    /* changed scheduling method from RRPRWP to FCFSPRWP */
    /* in order to implement active_message support            */

    if (id >= 0){
      send_dest[id] = -1; /* for WriteSend, defaults to owner node */
    }
    
    if (noiports < 0 || noiports > MAXFANOUT || nooports < 0 || nooports > MAXFANOUT)
	YS__errmsg("Number of processor ports exceeds maximum allowed");
    prptr->noiports = noiports;
    prptr->nooports = nooports;
    if (noiports > 1)
	prptr->router = routingfn;   /* Message routing function for the processor */
    else
	prptr->router = NULL;

    /* Event for sending local messaages */
    for (i=0; i<noiports; i++)
	{
	    prptr->iports[i] = NULL;
	}
    
    for (i=0; i<nooports; i++){
      prptr->oports[i] = NULL;
    }

    /* Queue of messages delivered to the processor */
    prptr->MsgList = YS__NewQueue ("ProcessorMsgList");

    /* Queue of processes waiting for messages */
    prptr->WaitingProcesses = YS__NewQueue ("ProcessorWaitingProcesses");
    
    TRACE_PROCESSOR_processor;
    YS__TotalPrcrs++;
    YS__QueuePutTail(YS__ActPrcr,(QE*)prptr);
    if (sm_flag)			/* If this module is used with MEMSIM */
	SMModuleInit(prptr, sm_node_num, sm_stat_level, sm_routing, sm_delays, sm_ports); /* Intialize shared memory data structures -- specifically wakeup, RMQ, handshake, Sim, stats */
    
    return prptr;
}

/* collect statistics on a reference */
void prcr_stat(PROCESSOR *prptr, REQ *req)
{
    double time;
    int type = req->req_type;
    int dubref = req->dubref;
    if (MemsimStatOn)
	{				/* stats */
	    prptr->num_ref ++;
	    prptr->num_miss ++;
	    time = GetSimTime()+YS__Cycles;
	    prptr->utilization += time - prptr->start_time;
	    req->start_time = time;
#ifdef DEBUG_LAT_CPU
	    if (DEBUG_TIME < YS__Simtime)
		fprintf(simout,"start:\t %g\t%ld\t%s\n",time, req->address, prptr->name);
#endif
	    if (prptr->stat_level > 1)
		{
		    if (type == READ && dubref)
			prptr->stat.read_dbl ++;
		    else if (type == WRITE && dubref)
			prptr->stat.write_dbl ++;
		    else if (type == READ)
			prptr->stat.read_sgl ++;
		    else if (type == WRITE)
			prptr->stat.write_sgl ++;
		    else if (type == RMW)
			prptr->stat.rmw ++;
		}
	}
}

/* Reports statistics of all processor modules */
void PrcrStatReportAll(void)
{
    int i;
    if (prcr_index)
	{
	    if (prcr_index == MAX_MEMSYS_PROCS) 
		YS__warnmsg("Greater than MAX_MEMSYS_PROCS processors created; statistics for the first MAX_MEMSYS_PROCS will be reported by PrcrStatReportAll");
	    fprintf(simout,"\n##### Processor Statistics #####\n");
	    
	    grdfr=0.0;
	    gwdfr=0.0;
	    grsfr=0.0;
	    gwsfr=0.0;
	    grmwfr=0.0;
	    ghitpr=0.0;
	    glat=0.0;
	    gutil=0.0;
	    gnum_ref=0;
	    
	    for (i=0; i< prcr_index; i++) 
		PrcrStatReport(prcr_ptr[i]);
	    
	    grdfr = grdfr /(double) prcr_index;
	    gwdfr = gwdfr /(double)prcr_index;  
	    grsfr = grsfr /(double)prcr_index;  
	    gwsfr = gwsfr /(double)prcr_index; 
	    grmwfr = grmwfr /(double)prcr_index;  
	    ghitpr = ghitpr /(double)prcr_index;  
	    glat = glat /(double)prcr_index;  
	    gutil = gutil /(double) prcr_index;
	    gnum_ref = gnum_ref / prcr_index;
	    
	    fprintf(simout,"\nName          Num_Ref          Hit_Rate          Miss_latency     Utilization\n");
	    fprintf(simout,"Prcr_Avg  %10d          %5.2g%%           %6.4g             %6.4g\n", 
		   gnum_ref, ghitpr, glat, gutil);
	    fprintf(simout,"\t      Read_Double      Read_Single\n");
	    fprintf(simout,"\t      %6.4g            %6.4g \n", grdfr, grsfr);
	    fprintf(simout,"\t      Write_Double     Write_Single       Read-M-Write\n");
	    fprintf(simout,"\t      %6.4g            %6.4g              %6.4g \n", gwdfr, gwsfr,  grmwfr);
	}
}

/* Clears statistics of all processor modules */
void PrcrStatClearAll(void)
{
    int i;
    if (prcr_index)
	{
	    for (i=0; i< prcr_index; i++) 
		PrcrStatClear(prcr_ptr[i]);
	}
}


/* Reports statistics of a processor module.  */
void PrcrStatReport(PROCESSOR *prptr)
{
    double rdfr, wdfr, rsfr, wsfr, rmwfr;
    double  hitpr, util, lat;
    
    prptr->utilization += GetSimTime() - prptr->start_time;
    prptr->start_time = GetSimTime();
    SMModuleStatReport(prptr , &hitpr, &lat, &util);
    
    gnum_ref += prptr->num_ref;
    ghitpr += hitpr;
    glat+=lat;
    gutil += util;
    
    if (prptr->stat_level > 1)
	{
	    if (prptr->num_ref)
		{
		    rdfr = (double)prptr->stat.read_dbl/(double)prptr->num_ref;
		    grdfr += rdfr;
		    wdfr = (double)prptr->stat.write_dbl/(double)prptr->num_ref;
		    gwdfr += wdfr;
		    rsfr = (double)prptr->stat.read_sgl/(double)prptr->num_ref;
		    grsfr += rsfr;
		    wsfr = (double)prptr->stat.write_sgl/(double)prptr->num_ref;
		    gwsfr += wsfr;
		    rmwfr = (double)prptr->stat.rmw/(double)prptr->num_ref;
		    grmwfr += rmwfr;
		}
	    else
		{
		    rdfr = 0.0;
		    wdfr = 0.0;
		    rsfr = 0.0;
		    wsfr = 0.0;
		    rmwfr = 0.0;
		}
	    fprintf(simout,"\t      Read_Double      Read_Single\n");
	    fprintf(simout,"\t %10d(%6.4g) %10d(%6.4g) \n", 
		    prptr->stat.read_dbl, rdfr, prptr->stat.read_sgl, rsfr);
	    
	    fprintf(simout,"\t      Write_Double     Write_Single       Read-M-Write\n");
	    fprintf(simout,"\t   %8d(%6.4g)   %8d(%6.4g) %6d(%6.4g) \n", 
		    prptr->stat.write_dbl, wdfr, prptr->stat.write_sgl, wsfr, prptr->stat.rmw, rmwfr);
	}
    
}

/* Clears statistics of a processor module. */
void PrcrStatClear(PROCESSOR *prptr)
{
    
    SMModuleStatClear(prptr);
    
    if (prptr->stat_level > 1)
	{
	    if (prptr->num_ref)
		{
		    prptr->stat.read_dbl = 0;
		    prptr->stat.write_dbl = 0;
		    prptr->stat.read_sgl = 0;
		    prptr->stat.write_sgl= 0;
		    prptr->stat.rmw = 0;
		}
	}
}

/*****************************************************************************/
/* SMModuleInit: called by NewProcessor to initialize the shared memory part */
/* of the processor module                                                   */
/*****************************************************************************/

void SMModuleInit(PROCESSOR *prptr, int node_num, int stat_level, rtfunc routing, struct Delays *Delays, int ports)
#if 0
    PROCESSOR  *prptr;		/* Pointer to the processor module                    */
    int node_num;    /* This node's number; Used for routing in directory based architectures */
    int stat_level;			/* Statistics level of this module                    */
    rtfunc routing;			/* Pointer to routing functions                       */
    struct Delays *Delays;		/* Pointer to user-defined structure for the delays   */
    int  ports;
#endif
{
    /* For stats purposes */
    if (prcr_index < MAX_MEMSYS_PROCS){
      prcr_ptr[prcr_index] = (PROCESSOR *)prptr;
      prcr_index ++;
    }

    /* Set up the processor module */
    /* ModuleInit (mptr, node_num, stat_level, routing, Delays, ports, q_size) */

    prptr->module_type = PROC_MODULE;

    ModuleInit ((SMMODULE *)prptr,  node_num, stat_level, routing, Delays, ports, DEFAULT_Q_SZ);
    /* Initialize data structures common to all shared memory modules
     The oport pointers get set here */
    
    prptr->num_ports_abv = 0;

    prptr->Sim = NULL;

    prptr->rm_q = (char *)malloc(sizeof(RMQ));
    if (prptr->rm_q == NULL)
	YS__errmsg("NewProcessor(): malloc failed");
    ((RMQ *)(prptr->rm_q))->u1.abv = 0; /* Used for round robin scheduling of ports */
    ((RMQ *)(prptr->rm_q))->u2.blw = 0;
    
    prptr->req = NULL;
    prptr->in_port_num = -1;
    prptr->wakeup = wakeup;	/* generic wakeup routine for this module */
                               
    prptr->handshake = NULL;
    
    prptr->stat.read_dbl = 0;	/* Initialize statistics data structures */
    prptr->stat.write_dbl = 0;
    prptr->stat.read_sgl = 0;
    prptr->stat.write_sgl = 0;
    prptr->stat.rmw = 0;
    
}
