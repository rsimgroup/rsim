/*
  bus.c

  Functions for simulating the functionality of a local node bus to
  connect the caches, directory, and network interface

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

  
#include "MemSys/typedefs.h" 
#include "MemSys/simsys.h"
#include "MemSys/cache.h"
#include "MemSys/net.h"
#include "MemSys/bus.h"
#include "MemSys/arch.h"
#include "Processor/simio.h"
#include <malloc.h>

static int bus_index=0;
static BUS *bus_ptr[MAX_MEMSYS_PROCS];
int BUSWIDTH = 32;
double BUSCYCLE = 3.0;
double ARBDELAY = 1.0;

/*****************************************************************************/
/* NewBus: Creates and initializes a new bus module.                         */
/*****************************************************************************/


BUS *NewBus(name, node_num, stat_level, routing, Delays, ports_abv, ports_blw, 
                bus_type, q_len, cohe_rtn)

char   *name;			/* Name of the module upto 32 characters long */
int    node_num;
int    stat_level;		/* Level of statistics collection for this module      */
rtfunc routing;			/* Routing function of the module            */
char   *Delays;			/* Pointer to user-defined structure for delays*/
int    ports_abv;		/* Number of ports above this module         */
int    ports_blw;		/* Number of ports below this module         */

int    bus_type;		/* Type of bus: SPLIT or N_SPLIT             */
int    q_len;			/* Queue length for output queue if SPLIT    */
cond   cohe_rtn;		/* Pointer to coherence routine used by this module, if any    */
{

  BUS *busptr;
  ARG *arg;
  char evnt_name[32];

  busptr = (BUS *)YS__PoolGetObj(&YS__BusPool); /* allocate the bus */
  if (bus_index < MAX_MEMSYS_PROCS) {
    bus_ptr[bus_index] = busptr;
    bus_index ++;
  }

  busptr->id = YS__idctr++;	/* System assigned unique ID   */
  strncpy(busptr->name, name,31); /* copy in the name */
  busptr->name[31] = '\0';
  busptr->module_type = BUS_MODULE;

  ModuleInit ((SMMODULE *)busptr, node_num, stat_level, routing, Delays, ports_abv + ports_blw, portszbusother-1);
  /* Initialize data structures common to all MEMSIM modules */
  busptr->num_ports_abv = ports_abv; 

  sprintf(evnt_name, "%s_bussim",name);
  if ( bus_type != PIPELINED)				/* Create new event */
      YS__errmsg("NON-PIPELINED type bus not supported!\n");
  else{
    busptr->Sim  = (ACTIVITY *)NewEvent(evnt_name, node_bus, NODELETE, 0);
  }
  
  arg = (ARG *)malloc(sizeof(ARG)); /* Setup arguments for cache event */ 
  arg->mptr = (SMMODULE *)busptr;
  ActivitySetArg(busptr->Sim, (char *)arg, UNKNOWN);

  busptr->req = NULL; /* currently not processing a REQ */
  busptr->in_port_num = -1; /* currently no active input port */
  if (bus_type != PIPELINED)
    YS__errmsg("Non-pipelined bus not supported.");
  else
    busptr->wakeup = wakeup_node_bus;	/* wakeup routine of the bus module */
  
  busptr->handshake = NULL;
  
  busptr->bus_type = bus_type;
  busptr->cohe_rtn = cohe_rtn; /* not ever called */
  
  busptr->next_port = 0;  /* next port to check for node_bus() */
  
  /* Initialize statistics  */
  busptr->utilization = 0.0;
  busptr->begin_util = 0.0;
  busptr->time_of_last_clear = 0.0;
  return busptr;
}

/*****************************************************************************/
/* node_bus: Simulates the functionality of a bus module. This bus module    */
/* connects the various components of a node together and simulates a        */
/* simple split-transaction bus with contention.  Its main function is to    */
/* direct requests/replies among the various attached modules.  In the       */
/* base case, these are the L2 cache, the network interfaces, and the        */
/* directory/memory modules                                                  */
/*                                                                           */
/* The default delays assume a bus cycle that is 1/3 that of the compute     */
/* processor.  The default width is assumed to be 32 bytes (256-bits).       */
/* Bus latency is calculated as a function of the request/reply size         */
/* traversing it.                                                            */
/*                                                                           */
/* An important function of the bus is to also make sure that an incoming    */
/* message is not removed from its queue unless the destination is able      */
/* to accept it.  Arbitration is performed in a round-robin fashion          */
/* (priority is passed around to the bus agents).                            */
/*****************************************************************************/

void node_bus()
{
  BUS *bptr;
  REQ *req, *req1;
  ARG *argptr;
  int case_num;
  int oport_num;
  int index;
  double delay = 0.0;
  int loop_count = 0;
  int done = FALSE;
  
  argptr = (ARG *)ActivityGetArg(ME);
  bptr = (BUS *)argptr->mptr;	/* Get pointer to bus module */
  req = bptr->req; /* currently processing a request? */
  if (req)
    index = req->in_port_num; /* remember the input port number */
  case_num = EventGetState(); /* start off in last state */

  /* This function is split up into several stages according
     to the progress of a request on the bus.  */
  
  while(!done) {
    switch(case_num) {
    case BUSSTART:

      /* In the BUSSTART stage, the bus has not started processing a
	 transaction. In this case, the bus peeks at the ports
	 round-robin (starting with the port after the one last
	 accessed) for a REQ. If none is found, the bus goes to sleep
	 until woken up by a new operation. If a transaction is
	 available, though, the bus moves to the SERVICE stage. */
				     
      req = (REQ *) peekQ(bptr->in_port_ptr[bptr->next_port]);
      if (!req) { /* queue is empty, check next one */
	case_num = BUSSTART;
	if (++bptr->next_port == bptr->num_ports) bptr->next_port = 0;
	loop_count++;
	if (loop_count==bptr->num_ports) { /* looped through all ports */
	  bptr->Sim = (ACTIVITY *)YS__ActEvnt;
	  bptr->state = WAIT_INQ_EMPTY; /* sleep waiting for a new REQ */
	  EventSetState(ME, BUSSTART);  /* wakeup in BUSSTART */
	  done = TRUE;
	}
	break;
      }
      else {
	req->in_port_num = bptr->next_port; /* this is important for proper routing */
	
	case_num = SERVICE;     /* We have the ARBDELAY after the SERVICE phase */
	if (MemsimStatOn) {
	  bptr->begin_util = YS__Simtime; /* calculate utilization stats */
	}
	bptr->req = req;
	break;
      }
      YS__errmsg("BUS: this is supposed to be unreachable code\n");
      
    case SERVICE:
      /* In the SERVICE stage, the routing function is called to
	 determine the output port for this REQ. If that port is not
	 available, the bus sleeps for a bus cycle to return to
	 BUSSTART, where it will try to find a different transaction
	 or keep trying this transaction until the output becomes
	 available. If the port is available, however, the bus
	 transitions to the BUSDELIVER stage after stalling for the
	 latency of the transfer (based on the message size, the bus
	 width, and the bus cycle). */

      oport_num = bptr->routing((SMMODULE *)bptr, req); /* Find out port number */
#ifdef DEBUG_BUS
      if (YS__Simtime > DEBUG_TIME)
	fprintf(simout,"BUS: PKT\t%s\t\t&:%ld\tTG:%ld\t%s\t src: %d dest: %d route: %d dir: %d @%1.0f Size:%d out Port:%d from port:%d\n",
		bptr->name, req->address, req->tag, Req_Type[req->req_type], req->src_node, req->dest_node,
		req->s.route, req->s.dir, YS__Simtime, req->size_st, oport_num, req->in_port_num);
#endif

      if (MemsimStatOn) {
	bptr->num_ref++;
      }
      if (checkQ(bptr->out_port_ptr[oport_num]) == -1) { /* port not available */
	if (MemsimStatOn) {
	  bptr->wait++;
	  bptr->utilization += YS__Simtime - bptr->begin_util + (BUSCYCLE*FASTER_PROC);
	  bptr->begin_util = YS__Simtime + (BUSCYCLE*FASTER_PROC); /* reset this */
	}

	/* sleep for a bus cycle to return to BUSSTART, where bus will
	   either try to find a different transaction or keep trying
	   this transaction until the output becomes available.  */
	
	EventSetState(ME, BUSSTART);
	ActivitySchedTime(ME, BUSCYCLE * FASTER_PROC, INDEPENDENT);
	req = bptr->req = NULL;
	if (++bptr->next_port == bptr->num_ports) bptr->next_port = 0;
	loop_count++;
	done = TRUE;
	break;
      }
      if (MemsimStatOn) bptr->num_lat++;

      /* commit the request for sure now, since it can be sent out */
      req1 = (REQ *) bus_get_next_req(bptr, req->in_port_num);
      if (req != req1)
	{
	  YS__errmsg("Bus get next request gives unexpected value -- we lost a req?");
	}

      /* Since the port is available, the bus transitions to the
	 BUSDELIVER stage after stalling for the latency of the
	 transfer (based on the message size, the bus width, and the
	 bus cycle). */
      
      bptr->req = req;
      EventSetState(ME, BUSDELIVER);
      delay = (double) ((req->size_st/BUSWIDTH)*BUSCYCLE + ( req->size_st % BUSWIDTH ? BUSCYCLE : 0.0));
      ActivitySchedTime(ME, delay * FASTER_PROC, INDEPENDENT);
      if (MemsimStatOn) {
	bptr->utilization += YS__Simtime - bptr->begin_util + (delay*FASTER_PROC);
	bptr->begin_util = YS__Simtime + (delay*FASTER_PROC); /* reset this */
      }
      done = TRUE;
      break;
	    
    case BUSDELIVER:

      /* In the BUSDELIVER stage, the bus moves the transaction into
	 the desired output port, after which it will stall for an
	 arbitration delay before allowing BUSSTART to continue
	 processing new requests. */

      if (!req) YS__errmsg("Request not available in BUSDELIVER");
      oport_num = bptr->routing((SMMODULE *)bptr, req); /* Find out port number */
#ifdef DEBUG_BUS
      if (YS__Simtime > DEBUG_TIME)
	fprintf(simout,"BUS: DELIVER\t%s\t\t&:%ld\tinst_tag:%d\tTG:%ld\t%s\t src: %d dest: %d route: %d dir: %d @%1.0f Size:%d out PORT:%d From port:%d\n",
		bptr->name, req->address, req->s.inst_tag, req->tag, Req_Type[req->req_type], req->src_node, req->dest_node,
		req->s.route, req->s.dir, YS__Simtime, req->size_st, oport_num, req->in_port_num);
#endif
      
      
      if (MemsimStatOn) { /* calculate utilization stats */
	bptr->utilization += YS__Simtime - bptr->begin_util + (ARBDELAY*FASTER_PROC);
      }
      add_req(bptr->out_port_ptr[oport_num], req); /* dispatch REQ */
      EventSetState(ME, BUSSTART); /* go back to BUSSTART */
      ActivitySchedTime(ME, ARBDELAY * FASTER_PROC, INDEPENDENT); /* stall for arbitration delay */
      req = bptr->req = NULL;
      if (++bptr->next_port == bptr->num_ports) bptr->next_port = 0; /* do round-robin */
      loop_count++;
      done = TRUE;
      break;
      
	    
    default:
      YS__errmsg("node_bus(): Unknown event state!\n");
    }
  }
    
}

/*****************************************************************************/
/* Bus statistics functions                                                  */
/*****************************************************************************/

void BusStatReportAll()
{
  int i;

  if (bus_index) { 
    if (bus_index == MAX_MEMSYS_PROCS) 
      YS__warnmsg("Greater than MAX_MEMSYS_PROCS buses created; statistics for the first MAX_MEMSYS_PROCS will be reported by BusStatReportAll");
    fprintf(simout,"\n##### Bus Statistics #####\n");
    for (i=0; i< bus_index; i++) 
      BusStatReport(bus_ptr[i]);
    fprintf(simout,"\n\n");
  }
}

void BusStatClearAll()
{
  int i;

  if (bus_index) { 
    for (i=0; i< bus_index; i++) 
      BusStatClear(bus_ptr[i]);
  }
}


void BusStatReport (bptr) /* only stats currently given are utilization */
BUS *bptr;
{
  fprintf(simout,"%s: Bus Utilization (time spent delivering pkts) = %3.4f%%\n", bptr->name, (bptr->utilization/ (YS__Simtime - bptr->time_of_last_clear))*100);
  
}

void BusStatClear (bptr)
BUS *bptr;
{

  SMModuleStatClear(bptr);
  if (bptr->stat_level > 1) {
    bptr->time_of_last_clear = YS__Simtime;
  }   
}
