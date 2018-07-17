/* architecture.c
   This file contains the routines for initializing the architecture.

   SystemInit -- The main routine to initialize the architecture
                  (from all the parameters that were read in with the
		  configuration file)

   dir_net_init -- Sets up the architecture as a directory based shared-memory
                   MP system with a mesh network

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
#include "MemSys/arch.h"
#include "MemSys/misc.h"
#include "MemSys/net.h"
#include "MemSys/cache.h"
#include "MemSys/directory.h"
#include "MemSys/bus.h"
#include "Processor/simio.h"
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct state;

static void dir_net_init (int, int, int,int, int, int, int,int, int, int, int, int, int,int, int, int,int,int, int, int, int, double, int);
static void QueueSizeCorrect(SMMODULE *, int, int);

/*****************************************************************************/
/* Basic configuration parameters and default values.                        */
/*****************************************************************************/

int INTERCONNECTION_WIDTH_BYTES=16; /* width of ports */
int INTERCONNECTION_WIDTH_BYTES_DIR=16; /* width of ports to directory */

int MeshRoute(int *,int *,int); /*dimension-order routing function for mesh */
extern OPORT *cpu_oport[256]; /* OPORTs of network */
extern IPORT *cpu_iport[256]; /* IPORTs of network */

PROCESSOR **cpu; /* array of CPU pointers */
DEMUX **demux_table; /* array of demuxes */
int NCPU_NODE; /* numbers of CPUs per node. Fixed at 1 */
STATREC **InterleavingStats; /* statistics on memory interleaving */

int MESH_MODE = 1; /* Use a mesh network */

enum ReplacementHintsType ReplacementHintsLevel = REPLHINTS_EXCL; /* protocol implemented with replacement hints on replacement of EXCL clean */
enum CacheCoherenceProtocolType CCProtocol = MESI;

/* Set default parameters for cache and memory */
int MEMORY_LATENCY = 18;
double DIRCYCLE = 3.0;
int DIR_PKTCREATE_TIME = 12, DIR_PKTCREATE_TIME_ADDTL = 6;

int L1TYPE; /* FIRSTLEVEL_WT or FIRSTLEVEL_WB */
int lvl2_linesz; /* coherence granularity of system = cache size of L2 */
int YS__NumNodes; /* number of nodes in system */

#define MIN_CACHE_LINESZ 16 /* minimum acceptable line size */


/* Parameters as specified in manual for configuration */
int ARCH_numnodes=16;
int ARCH_l1type=PR_WT;
int ARCH_cacsz1=16, ARCH_setsz1=1;
int ARCH_cacsz2=64, ARCH_setsz2=4, ARCH_linesz=64;
int ARCH_wbufsz=8, ARCH_dirbufsz=64;
int ARCH_nettype = MESH_NET, ARCH_NetBufsz=64;
int ARCH_NetPortsz=64, ARCH_flitsz=8;
int ARCH_flitd=4, ARCH_arbdelay=4;
int ARCH_pipelinedsw=2;
static int ARB_DELAY; /* network arb delay */

int REQ_SZ = 16; /* request header size -- 8 for address, 2 for from,
		    2 for command, 2 for to, 2 bytes reserved for
		    future use */

int portszl1wbreq=L1_DEFAULT_NUM_PORTS, portszwbl1rep=1;
int portszwbl2req=1, portszl2wbrep=1;
int portszl1l2req=L1_DEFAULT_NUM_PORTS, portszl2l1rep=1;
int portszl2l1cohe=1, portszl1l2cr=1;
int portszl2busreq=8, portszbusl2rep=2;
int portszbusl2cohe=2, portszl2buscr=8;
int portszbusother=16, portszdir=64;

/**************************************************************************************/

void SystemInit()
{
  int linesz1 = ARCH_linesz; /* both caches have same line size */
  int linesz2 = ARCH_linesz;
  int adap1=0,adap2=0;  /* adaptive protocol not supported */
  int cohe1,cohe2,allo1,allo2,wbuf1;
  int wbuftype1;
  int num_hops1;
  int flitd;

  num_hops1 = 1;
  cohe2 = WB_NREF, allo2 = 1; /* L2 is WB, with write-allocate */
  cohe1 = ARCH_l1type;        /* L1 is either PR_WT or PR_WB   */
  wbuftype1 = DIR_RC;         /* WBuf is always coalescing line buffer */
    
  if (ARCH_l1type == PR_WT) /* write-through L1 cache */
    {
      allo1 = 0; /* no write allocate */
      wbuf1 = 1; /* use a write-buffer */
      L1TYPE = FIRSTLEVEL_WT;
    }
  else /* write-back L1 cache */
    {
      allo1 = 1; /* write-allocate */
      wbuf1 = 0; /* no write-buffer */
      L1TYPE = FIRSTLEVEL_WB;
    }

  /* Simulate pipelined switches as described in the manual:
     set the flit delay to the granularity of pipelining and
     increment the arbitration delay of a mux by the
     difference between the specified flit delay and the
     flit delay used */
  if (ARCH_pipelinedsw>0 && ARCH_flitd > ARCH_pipelinedsw)
    {
      ARB_DELAY = ARCH_arbdelay + ARCH_flitd - ARCH_pipelinedsw;
      flitd = ARCH_pipelinedsw;
    }
  else
    {
      ARB_DELAY = ARCH_arbdelay;
      flitd = ARCH_flitd;
    }
  
  if (ARCH_linesz < MIN_CACHE_LINESZ)
    {
      fprintf(simerr,"Cache line sizes must be at least %d. Rounding up.\n",MIN_CACHE_LINESZ);
      linesz1 = linesz2 = MIN_CACHE_LINESZ;
    }

  /* Global parameters that copy these parameters */
  lvl2_linesz = linesz2;
  YS__NumNodes = ARCH_numnodes;
  
  if (ARCH_nettype == MESH_NET) /* call dir_net_init to set up mesh network */
    dir_net_init (ARCH_numnodes, ARCH_cacsz1, ARCH_cacsz2, linesz1, linesz2,
		  ARCH_setsz1, ARCH_setsz2,
		  cohe1, cohe2, allo1, allo2, adap1, adap2, wbuf1, 
		  ARCH_wbufsz, wbuftype1, ARCH_dirbufsz, ARCH_nettype,
		  flitd, ARCH_NetBufsz, 
		  ARCH_NetPortsz, (double)ARCH_flitsz, num_hops1);
  else
    {
      fprintf(stderr,"Invalid network type\n");
      exit(-1);
    }
}



/*****************************************************************************/
/* dir_net_init: Starts up a system with a 2-D mesh and the specified        */
/* caches, write-buffers, degree of interleaving, buses, etc.                */
/*****************************************************************************/

static void dir_net_init (int nodes, int cacsz, int cacsz2,
	      int linesz, int linesz2, int setsz, int setsz2,
	      int cohe, int cohe2, int allo, int allo2, int adap, int adap2,
	      int wbuf, int wbufsz, int wbuftype, 
	      int dbufsz,
	      int nettype, int flitd, int NetBufsz, int NetPortsz, double flitsz, int num_hops)
{
   
    CACHE **cache;
    WBUFFER **wbufptr;
    DIR **dir;
    BUS **bus;
    SMNET **smnet_send;
    SMNET **smnet_rcv;
    IPORT **iport_ptr;
    OPORT **oport_ptr;
    
    double temp1;
    int dim_mesh[2];
    char name[32];
    struct Delays *DelayCPU;
    struct Delays *L1Delays;
    struct Delays *Zero_Delay;
    struct Delays *L2Delays;
    struct DirDelays *DirDelays;
    int i;
    int leaf;
    
    fprintf(simout,"\nNetwork Parameters:\n");
    if (nodes == 1)
      fprintf(simout,"Uniprocessor system -- no network to speak of!\n");
    else if (nodes < 4)
      YS__errmsg("Cannot build network with less than 4 nodes");

    if (nodes != 1)
      {
	/* First, dir_net_init constructs the multiprocessor
	   interconnection network. This is a 2-dimensional mesh
	   network with a square number of nodes (if a non-square
	   system is desired, the user should either round up to the
	   nearest square or some other appropriate square
	   configuration size).  dir_net_init calls CreateMESH to
	   construct the request and reply networks. */
	
	iport_ptr = (IPORT **)malloc(sizeof(IPORT *) * nodes*2);  
	oport_ptr = (OPORT **)malloc(sizeof(OPORT *) * nodes*2);  
	
	
	if(nettype != MESH_NET)
	  YS__errmsg("This version of RSIM supports only the mesh network.\n");
	
	/****** NETWORK *******/
	
	/* Set up the mesh network */
	temp1 = sqrt((double)nodes);
	dim_mesh[0] = (int)temp1;
	if (dim_mesh[0] != temp1)
	  YS__errmsg("For a mesh network number of  nodes must be a square\n");
	dim_mesh[1] = dim_mesh[0];
	fprintf(simout,"MESH:\tsize: %dx%d\tSpeed: %d (x prcr speed)\n",
	       dim_mesh[0], dim_mesh[1], flitd * FASTER_PROC );
	fprintf(simout,"\tWidth of links: %g (in bytes)\tSwitch Buffer Size: %d (in flits; %s)\n",
	       flitsz, NetBufsz, NetBufsz == (int)((double)linesz/flitsz) ? "=line size":"");
	
	/* Call to CreateMesh(dim, mesh_size, bufferszinnetwork,port_size, iport*,
	   oport*, router_function, mesh_num) */
	/* We create a request net and a reply net for worm hole routing */
	
	CreateMESH (2, dim_mesh, NetBufsz, 1, iport_ptr, oport_ptr, MeshRoute, REPLY_NET);
	/* Reply network */
	CreateMESH (2, dim_mesh, NetBufsz, 1, iport_ptr+nodes, oport_ptr+nodes,
		    MeshRoute, REQ_NET); /* Request network */
	
	
	NetworkSetFlitDelay (flitd * FASTER_PROC); /* Set flit delay to flitd cycles -- in net.c */
	NetworkSetArbDelay(ARB_DELAY * FASTER_PROC);       /* this is time for a mux to arbitrate between two flits that come at the same time */
      }
    
    /* Next, dir_net_init creates the Delays data structures for many
       of the individual modules. Each modules has parameters for
       access time, initial transfer time, and flit transfer
       time. Additionally, the directory module has parameters for the
       first packet creation time associated with a request and each
       subsequent packet creation time, which are used to effect
       delays when sending coherence messages from a directory.  */
    
    /* ***** Processor delay ******** */
    DelayCPU = (struct Delays *)malloc(sizeof(struct Delays));
    if (DelayCPU == NULL) {
	fprintf(simout,"UserMain(): malloc failed\n");
	exit(-1);
    }
    DelayCPU->access_time = 0; /* These are 0 as this is virtual CPU */
    DelayCPU->init_tfr_time = 0;
    DelayCPU->flit_tfr_time = 0;
    
    /* ***** L1 cache delay ******** : L1 and L2 Cache delays are present for historical reasons, but are not used*/
    L1Delays = (struct Delays *)malloc(sizeof(struct Delays));
    if (L1Delays == NULL) {
	fprintf(simout,"UserMain(): malloc failed\n");
	exit(-1);
    }
    L1Delays->access_time = 0; /* this field not used */
    L1Delays->init_tfr_time = 0;
    L1Delays->flit_tfr_time = 0;

    
    /* ***** L2 cache delay ******** */
    L2Delays = (struct Delays *)malloc(sizeof(struct Delays));
    if (L2Delays == NULL) {
	fprintf(simout,"UserMain(): malloc failed\n");
	exit(-1);
    }
    L2Delays->access_time = 0; /* this field not used */
    L2Delays->init_tfr_time = 0;
    L2Delays->flit_tfr_time = 1*FASTER_PROC;

    /* Write buffer delays */
    Zero_Delay = (struct Delays *)malloc(sizeof(struct Delays));
    if (Zero_Delay == NULL) {
	fprintf(simout,"UserMain(): malloc failed\n");
	exit(-1);
    }
    Zero_Delay->access_time = 0;
    Zero_Delay->init_tfr_time = 0;
    Zero_Delay->flit_tfr_time = 0;

    /* Directory delays */
    DirDelays = (struct DirDelays *)malloc(sizeof(struct DirDelays));
    if (DirDelays == NULL) {
	fprintf(simout,"UserMain(): malloc failed\n");
	exit(-1);
    }
    DirDelays->access_time = MEMORY_LATENCY * FASTER_PROC;
    DirDelays->init_tfr_time = 0  * FASTER_PROC;
    DirDelays->flit_tfr_time = 1  * FASTER_PROC;
    DirDelays->pkt_create_time = DIR_PKTCREATE_TIME  * FASTER_PROC;
    DirDelays->addtl_pkt_crtime = DIR_PKTCREATE_TIME_ADDTL  * FASTER_PROC;
    DIRCYCLE *= FASTER_PROC;
    
    
    /* Initialize all the modules' data structures */

    NCPU_NODE = 1;
    cpu = (PROCESSOR **)malloc(sizeof(PROCESSOR *) * nodes * NCPU_NODE);
    cache = (CACHE **)malloc(sizeof(CACHE *) * nodes * (NCPU_NODE + 1));
    wbufptr = (WBUFFER **)malloc(sizeof(WBUFFER *) * nodes * NCPU_NODE);
    smnet_send = (SMNET **)malloc(sizeof(SMNET *) * nodes);
    smnet_rcv = (SMNET **)malloc(sizeof(SMNET *) * nodes);
    bus = (BUS **)malloc(sizeof(BUS *) * nodes);

    InterleavingStats = (STATREC **)malloc(sizeof(STATREC *)*nodes);
    dir = (DIR **)malloc(sizeof(DIR *) * nodes * INTERLEAVING_FACTOR);
    
    /* Needed for structural hazard identiication */
    L1Q_FULL = (int *)malloc(sizeof(int) * nodes * NCPU_NODE);
    
    if (cpu == NULL || cache == NULL || wbufptr == NULL || smnet_send == NULL || 
	smnet_rcv == NULL || dir == NULL /* || memory == NULL */ || L1Q_FULL == NULL) {
	fprintf(simout,"UserMain(): malloc failed\n");
	exit(-1);
    }

    /* Now dir_net_init makes all the various modules at each of the nodes. */
    
    for (i=0; i<nodes; i++) { /* loop through the nodes */
      if (wbuf) { 	/* system has write buffers */
	
	/*            Processor with DelayCPU            */
	sprintf(name,"cpu%d",i);
	cpu[i] = NewProcessor(name, i, 50.0, 1, 1, NULL, 1, i, 2, routing1, DelayCPU, 
			      1 /* PrcrStatReport */);
	
	
	/*            L1 cache with L1Delays             */
	sprintf(name,"cache%d",i);
	L1TYPE = FIRSTLEVEL_WT;
	/* CACHE *NewCache(char *name, int node_num, int stat_level,
		rtfunc routing, struct Delays *Delays, int ports_abv,
		int ports_blw, int cache_type, int size, int
		line_size, int set_size, int cohe_type, int allo_type,
		int cache_type1, int adap_type, int replacement, func
		cohe_rtn, int tagdelay, int tagpipes, int tagwidths[],
		int datadelay, int datapipes, int datawidths[], int
		netsend_port, int netrcv_port, LINKPARA *link_para,
		int max_mshrs, int procnum, int unused); */
	cache[i] = NewCache(name, i, 3,  routing_L1, L1Delays, 1, 2,
			    RC_DIR_ARCH, cacsz,
			    linesz, setsz, cohe, allo, ALL, adap, LRU, cohe_pr,
			    L1TAG_DELAY,L1TAG_PIPES,L1TAG_PORTS,
			    0,0,NULL,
			    -1,-1, NULL, L1_MAX_MSHRS,
			    i, 0  );
	
	L1Q_FULL[i]=0;
	
	/*           Write buffer with Zero_Delay       */
	/* Parameters  : name, node_num, stat_level, routing, delays, ports_abv, */
	/*   ports_blw, size, wbuf_type (only DIR_RC), stats_print_routine */
	sprintf(name,"wbuf%d",i);
	wbufptr[i] = NewWBuffer(name, i, 3, routing_WB, Zero_Delay, 1,1, wbufsz, wbuftype, 
				WBufferStatReport);
      }
      else{ /* no write-buffer */
	/* First level write back cache */
	
	sprintf(name,"cpu%d",i);
	cpu[i] = NewProcessor(name, i, 50.0, 1, 1, NULL, 1, i, 2, routing1, DelayCPU, 
			      1);
	
	sprintf(name,"cache%d",i);
	L1TYPE = FIRSTLEVEL_WB;
	cache[i] = NewCache(name, i, 3, routing_L1, L1Delays, 1,  2,
			    RC_DIR_ARCH_WB, cacsz,
			    linesz, setsz, cohe, allo, ALL, adap, LRU, cohe_pr,
			    L1TAG_DELAY,L1TAG_PIPES,L1TAG_PORTS,
			    0,0,NULL, -1,-1, NULL, L1_MAX_MSHRS,
			    i, 0);
	
	L1Q_FULL[i]=0;
      }
      
      sprintf(name,"L2cache%d",i);
      cache[i+NCPU_NODE * nodes] =
	NewCache(name, i, 3, routing_L2, L2Delays, NCPU_NODE*2 /* 2 above */, 2,
		 RC_LOCKUP_FREE,
		 cacsz2, linesz2, setsz2, cohe2, allo2, ALL, adap2,
		 LRU, cohe_sl,
		 L2TAG_DELAY,L2TAG_PIPES,L2TAG_PORTS,
		 L2DATA_DELAY,L2DATA_PIPES,L2DATA_PORTS,
		 NCPU_NODE, NCPU_NODE+1, NULL, L2_MAX_MSHRS,
		 i, 0);
      /* L2 has 2 above ports -- for the cohe and reply messages */

      if (YS__NumNodes != 1)
	{
	  /* SMNET (network interface) connections -- send and recieve */
	  /* SMNET Send */
	  /* Parameters : name, node_num, stat_level, routing, delay,
	     ports_abv, ports_blw, smnet_type, iport_reply, iport_req,
	     portq_sz, req_flitsz, reply_flitiz, nettype,
	     num_hps */
	  sprintf(name, "smnet_send%d",i);
	  smnet_send[i] = NewSmnetSend(name,i, 4,routing6, Zero_Delay, 1,1,
				       0, iport_ptr[i],iport_ptr[nodes+i],
				       NetPortsz, flitsz, flitsz,
				       nettype, num_hops);
	  
	  /* SMNET Receive */
	  /* Parameters : name, node_num, stat_level, routing, Delays,
	   ports_abv, ports_blw, smnet_type, oport_reply, oport_req,
	   portq_size) */
	  sprintf(name, "smnet_rcv%d",i);
	  smnet_rcv[i] = NewSmnetRcv(name,i, 4, routing_SMNET  , Zero_Delay, 1,1 , 0, oport_ptr[i], 
				     oport_ptr[nodes+i],NetPortsz);

	}
      
      /* Bus module */
      /* NewBus(name, node_num, stat_level, routing, Delays, ports_abv,
	 ports_blw, bus_type, q_len, cohe_rtn) */
      sprintf(name,"bus%d",i);

      bus[i] = NewBus(name,i,3, routing_bus, Zero_Delay /* dummy */, 2,
		     4+(INTERLEAVING_FACTOR*2), PIPELINED
		     /* new type */,
		     portszbusother-1, NULL);  
 
      /* The total number of input ports is 8, 2 of them are not used (from the smnet send module) */
      
      /* Connections */
     
      if (YS__NumNodes != 1)
	{
	  /* if we're a uniprocessor, we never want to do this,
	     since this will peek at non-existent network! */

	  ActivitySchedTime((ACTIVITY *)smnet_rcv[i]->EvntReply, 0.0, INDEPENDENT);
	  smnet_rcv[i]->EvntReply = NULL;
	  ActivitySchedTime((ACTIVITY *)smnet_rcv[i]->EvntReq, 0.0, INDEPENDENT);
	  smnet_rcv[i]->EvntReq = NULL;
	}

      for(leaf=0;leaf< INTERLEAVING_FACTOR;leaf++){ /* create as many directories per node as specified with INTERLEAVING_FACTOR */
	sprintf(name,"dir%d_%d",i,leaf);
	dir[(i*INTERLEAVING_FACTOR)+leaf] = NewDir(name,i, 3, /* routing5*/ routing_dir,
						   DirDelays, 2, 0 /*1 */,linesz, 
						   CNTRL_FULL_MAP, nodes, dbufsz,
						   Dir_Cohe, 0,1,NULL/* DirStatReport */);
      }
      InterleavingStats[i] = NewStatrec("interleaving",POINT,NOMEANS,HIST,
					(INTERLEAVING_FACTOR-1),0.0,
					(double)(INTERLEAVING_FACTOR-1));

      /* Now, let us connect them all together using
	 void ModuleConnect(one, two, port_one, port_two, width) */
      /* We will also make sure that the ports connecting the modules
	 have the desired length using QueueSizeCorrect(Module,
	 request_port, request_queue_sz) : This causes output port
	 #request_port of the module to have a queue of the specified
	 size. Note that the sizes specified will be 1 less
	 than the actual number of requests, since each port has an
	 "overflow" request available (except for the smnet case,
	 which does not use the overflow) */

      ModuleConnect(cpu[i], cache[i], 0, 0,INTERCONNECTION_WIDTH_BYTES);
      QueueSizeCorrect((SMMODULE *)cpu[i],0,L1_NUM_PORTS-1);

      if (wbuf)
	{
	  ModuleConnect(cache[i], wbufptr[i], 1,0, INTERCONNECTION_WIDTH_BYTES);
	  QueueSizeCorrect((SMMODULE *)cache[i],1,portszl1wbreq-1);
	  QueueSizeCorrect((SMMODULE *)wbufptr[i],0,portszwbl1rep-1);
	  ModuleConnect(wbufptr[i], cache[i+nodes],1, 0,INTERCONNECTION_WIDTH_BYTES);
	  QueueSizeCorrect((SMMODULE *)wbufptr[i],1,portszwbl2req-1);
	  QueueSizeCorrect((SMMODULE *)cache[i+nodes],0,portszl2wbrep-1);
	}
      else
	{
	  ModuleConnect(cache[i], cache[i+nodes], 1, 0, INTERCONNECTION_WIDTH_BYTES);
	  QueueSizeCorrect((SMMODULE *)cache[i],1,portszl1l2req-1);
	  QueueSizeCorrect((SMMODULE *)cache[i+nodes],0,portszl2l1rep-1);
	}
	
      /* Here is where we have connections for the cohe and cohe_reply ports */
      /* These are from the L1 to the L2 directly */
      ModuleConnect(cache[i], cache[i+nodes], 2, 3, INTERCONNECTION_WIDTH_BYTES);
      QueueSizeCorrect((SMMODULE *)cache[i],2,portszl1l2cr-1);
      QueueSizeCorrect((SMMODULE *)cache[i+nodes],3,portszl2l1cohe-1);

      /* Connect the L2 cache to the bus */
      ModuleConnect(cache[i+nodes], bus[i], 1, 0, INTERCONNECTION_WIDTH_BYTES);
      ModuleConnect(cache[i+nodes], bus[i], 2, 1, INTERCONNECTION_WIDTH_BYTES);

      /* the BUS->L2 connections can be short because the gap between
	 successive bus messages to L2 is likely to be of same order
	 as or larger than L2 access time. Conversely, the L2->BUS
	 connections should be large because the bus may be saturated
	 by other agents, may be slower, etc. The default sizes
	 are chosen in this way, but can be overridden. */

      QueueSizeCorrect((SMMODULE *)bus[i],0,portszbusl2rep-1);
      QueueSizeCorrect((SMMODULE *)bus[i],1,portszbusl2cohe-1);
      QueueSizeCorrect((SMMODULE *)cache[i+nodes],1,portszl2busreq-1);
      QueueSizeCorrect((SMMODULE *)cache[i+nodes],2,portszl2buscr-1);
      /* COHE-REPLY/WRB port can actually use many entries, but we limit
	 it to this for now since each entry may include a full cache
	 line. */
      
      if (YS__NumNodes != 1)
	{
	  ModuleConnect(bus[i], smnet_send[i], 2, 0, INTERCONNECTION_WIDTH_BYTES);
	  ModuleConnect(bus[i], smnet_send[i], 3, 1, INTERCONNECTION_WIDTH_BYTES);
	  ModuleConnect(bus[i], smnet_rcv[i], 4, 0, INTERCONNECTION_WIDTH_BYTES);
	  ModuleConnect(bus[i], smnet_rcv[i], 5, 1, INTERCONNECTION_WIDTH_BYTES);
	}
      else
	{
	  /*  put in fake connections, as the system expects network interfaces
	      to be there when counting for memory banks */
	  ModuleConnect(bus[i], bus[i], 2, 3, INTERCONNECTION_WIDTH_BYTES);
	  ModuleConnect(bus[i], bus[i], 4, 5, INTERCONNECTION_WIDTH_BYTES);
	}
      for(leaf = 0;leaf< INTERLEAVING_FACTOR; leaf++){
	ModuleConnect(bus[i], dir[(i*INTERLEAVING_FACTOR)+leaf],6+(2*leaf) , 0, INTERCONNECTION_WIDTH_BYTES);
	ModuleConnect(bus[i], dir[(i*INTERLEAVING_FACTOR)+leaf],7+(2*leaf) , 1, INTERCONNECTION_WIDTH_BYTES);
      }


    }

    if (cache[0]->size == INFINITE)
      fprintf(simout,"\n\nPrimary Cache Size %s, ","INFINITE");
    else
      fprintf(simout,"\n\nPrimary Cache Size %dKB, ", cache[0]->size);
    fprintf(simout,"Line Size: %dB, Set Size: %d, Coherence: %s, Adaptive: %s\n",
	   cache[0]->linesz, cache[0]->setsz, Cohe[cache[0]->types.cohe_type], 
	   cache[0]->types.adap_type == 1 ? "TRUE" : "FALSE");
    
    if (cache[nodes]->size == INFINITE)
      fprintf(simout,"\n\nSecondary Cache Size %s, ","INFINITE");
    else
      fprintf(simout,"\n\nSecondary Cache Size %dKB, ", cache[nodes]->size);
    fprintf(simout,"Line Size: %dB, Set Size: %d, Coherence: %s, Adaptive: %s\n",
	   cache[nodes]->linesz, cache[nodes]->setsz, Cohe[cache[nodes]->types.cohe_type], 
	   cache[nodes]->types.adap_type == 1 ? "TRUE" : "FALSE");
    
    
    fprintf(simerr,"\nRunning simulation on *** %s ***\n",getenv("HOST"));
    fprintf(simout,"\nRunning simulation on *** %s ***\n",getenv("HOST"));
}

/* Change the size of an output port of some module */
static void QueueSizeCorrect(SMMODULE *mptr, int port, int q_sz)
{
  if (port < mptr->num_ports && mptr->out_port_ptr[port] != NULL)
    {
      mptr->out_port_ptr[port]->q_sz_tot = q_sz;
    }
  else
    {
      YS__errmsg("Correcting invalid queue");
    }
}
