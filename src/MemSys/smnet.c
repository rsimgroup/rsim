/*
  smnet.c

  This file provides functions that implement the network interface
  modules (Smnet for Shared Memory NETwork interfaces). There are two
  separate network interface modules in each node: one dedicated for
  sending (SmnetSend), and another for receiving (SmnetRcv).

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
#include "MemSys/req.h"
#include "MemSys/module.h"
#include "MemSys/net.h"
#include "MemSys/misc.h"
#include "MemSys/arch.h"
#include "Processor/simio.h"
#include <malloc.h>
#include <string.h>

static int smnet_index=0;
static SMNET *smnet_ptr[150];        /* keep track of all the smnet pointers */
FILE *fileptr[16];

/* Statistics variables */
static double gszfr[2][NUM_SIZES],greqfr[2],grepfr[2],gcohefr[2],gcohe_refr[2];
static double ghitpr[2], glat[2], gutil[2];
static int gnum_ref[2];
STATREC reqQLStat[2];
STATREC replyQLStat[2];
STATREC reqQTStat[2];
STATREC replyQTStat[2];


/****************************************************************************
  NOTE: The smnet module is connected to the iport and oport modules defined
  as part of NetSim (see net.c). These port modules are the ones that are
  directly connected to the network internals (the multiplexer and the
  demultiplexer modules that are used to build the network).
  ***************************************************************************/

/*****************************************************************************/
/*  This routine initializes and returns a pointer to a smnet send module.   */
/*****************************************************************************/

SMNET *NewSmnetSend(char *name, int node_num, int stat_level, rtfunc routing,
		    struct Delays *Delays, int ports_abv, int ports_blw,
		    int smnet_type, IPORT *iport_reply, IPORT *iport_req,
		    int portq_size, double req_flitsz, double reply_flitsz,
		    int nettype, int num_hps)
#if 0
     char   *name;	     /* Name of the module upto 32 characters long  */
     int    node_num;        /* the node number                             */
     int    stat_level;	     /* Level of statistics collection for module   */
     rtfunc routing;	     /* Routing function of the module              */
     struct Delays *Delays;  /* Pointer to user-defined structure for delays*/
     int    ports_abv;       /* Number of ports above this module           */
     int    ports_blw;	     /* Number of ports below this module           */
     int    smnet_type;       
     IPORT  *iport_reply;    /* connections to reply iports                 */
     IPORT  *iport_req;	     /* connections to request iports               */
     double    req_flitsz;   /* in bytes: can also be non-integer           */
     double    reply_flitsz; /* in bytes: can also be non-integer           */
     int    num_hps;	     /* Average number of hops: Used for xbar       */
#endif
{
  int i;
  SMNET *smnetptr;
  ARG *arg;
  char evnt_name[32];
  
  smnetptr = (SMNET *)malloc(sizeof(SMNET));
  if (smnet_index < 150) 
    smnet_ptr[smnet_index ++] = smnetptr;
  
  smnetptr->id = YS__idctr++;	          /* System assigned unique ID   */
  
  strncpy(smnetptr->name, name,31);
  smnetptr->name[31] = '\0';
  
  smnetptr->module_type = SMNET_SEND_MODULE;

  /* Perform the generic module initialization */
  ModuleInit ((SMMODULE *)smnetptr, node_num, stat_level, routing, Delays,
	      ports_abv + ports_blw, portq_size);
  
  /* Initialize the data structures common to all shared smnet modules */
  smnetptr->num_ports_abv = ports_abv;
  
  sprintf(evnt_name, "%s_SmnetSend",name);
  smnetptr->Sim  = (ACTIVITY *)NewEvent(evnt_name, SmnetSend, NODELETE, 0);

  /* Create New event for this module */
  arg = (ARG *)malloc(sizeof(ARG));
  arg->mptr = (SMMODULE *)smnetptr; /* Initialize arg pointer for this event */
  ActivitySetArg(smnetptr->Sim, (char *)arg, UNKNOWN);
  
  smnetptr->rm_q = (char *)malloc(sizeof(RMQ));
  if (smnetptr->rm_q == NULL)
    YS__errmsg("NewSmnet(): malloc falied");
  ((RMQ *)(smnetptr->rm_q))->u1.abv = 0;
  /* These are used for round-robin scheduling of ports */
  ((RMQ *)(smnetptr->rm_q))->u2.blw = 0;
  
  smnetptr->req = NULL;
  smnetptr->in_port_num = -1;
  smnetptr->wakeup = wakeup;	/* Wakeup routine for this module:
				 used to wakeup sleeping process when
				 a new request is there to be processed */
  smnetptr->handshake = NULL;   /* Smnet send does not need handshakes */

  /* Smnet send handles only input ports to the network, SmnetRcv handles
     output ports from network */
  smnetptr->iport_reply = iport_reply;
  smnetptr->iport_req = iport_req;
  smnetptr->oport_reply = NULL;
  smnetptr->oport_req = NULL;

  /* The smnet module uses two events to manipulate two semaphores to handle
     inputs to the iports */
  
  /************ Create event to manipulate reply queue *************/
  sprintf (evnt_name, "%s_send_reply", name); 
  smnetptr->EvntReply  = NewEvent(evnt_name, ReplySendSemaWait, NODELETE, 0);
  arg = (ARG *)malloc(sizeof(ARG));
  arg->mptr = (SMMODULE *)smnetptr; /* Initialize arg pointer for this event */
  ActivitySetArg((ACTIVITY *)smnetptr->EvntReply, arg, UNKNOWN);

  /************ Create event to manipulate request queue *************/
  sprintf(evnt_name, "%s_send_req",name);
  smnetptr->EvntReq  = NewEvent(evnt_name, ReqSendSemaWait, NODELETE, 0);
  arg = (ARG *)malloc(sizeof(ARG));
  arg->mptr = (SMMODULE *)smnetptr; /* Initialize arg pointer for this event */
  ActivitySetArg((ACTIVITY *)smnetptr->EvntReq, arg, UNKNOWN);

  /* Smnet maintains two internal buffers for the requests and replies
     that are pending on the semaphores for the request and reply ports.
     The following code initializes these buffers */
  smnetptr->Q_totsz = portq_size/2; 
  if (smnetptr->Q_totsz == 0)
    smnetptr->Q_totsz = 1;    /* the smnet needs some internal buffering */
  smnetptr->reqQHead = NULL;  /* The requestQ and replyQ are implemented as */
  smnetptr->reqQTail = NULL;  /* linked lists */
  smnetptr->reqQsz =  0;
  smnetptr->replyQHead = NULL;
  smnetptr->replyQTail = NULL;
  smnetptr->replyQsz = 0;
  smnetptr->next_port = 0;

  /* Initialize the flit size */
  smnetptr->req_flitsz = req_flitsz;
  smnetptr->reply_flitsz = req_flitsz;
  /* NOTE: ************** req_flitsz is always same as rep_flitsz *********/

  /* other miscellaneous variables */
  smnetptr->nettype = nettype;
  smnetptr->num_hps = num_hps;
  smnetptr->prnt_rtn = SmnetSendStatReport;
  smnetptr->num_req = 0;
  smnetptr->num_rep = 0;
  smnetptr->num_cohe = 0;
  smnetptr->num_cohe_rep = 0;
  for (i=0; i<NUM_SIZES; i++)
    smnetptr->num_sz_ref[i] = 0;
  if (stat_level > 2) {
    sprintf(name,"ReqQLen_%d",node_num);
    smnetptr->reqQLStat = NewStatrec(name, INTERVAL, MEANS, NOHIST, 0,0,0);
    sprintf(name,"ReplyQLen_%d",node_num);
    smnetptr->replyQLStat = NewStatrec(name, INTERVAL, MEANS, NOHIST, 0,0,0);
    
    sprintf(name,"ReqQTim_%d",node_num);
    smnetptr->reqQTStat = NewStatrec(name, POINT, MEANS, NOHIST, 0,0,0);
    sprintf(name,"ReplyQTim_%d",node_num);
    smnetptr->replyQTStat = NewStatrec(name, POINT, MEANS, NOHIST, 0,0,0);
  }
  return smnetptr;
}


/*****************************************************************************/
/* SmnetSend:                                                                */
/*   This function simulates the functionality of a smnet send module        */
/*****************************************************************************/
void SmnetSend() 
{
  SMNET *smnetptr;
  REQ *req, *req1;
  ARG *argptr;
  ACTIVITY *sim;
  SMMODULE *mptr;
  int case_num;
  int temp, i,addQ, j;
  double delay = 0.0;
  int done = FALSE, loop_count=0;

  /* As with all other modules in YACSIM, use the ActivityArgument
     to get the value of the pointer to the module */
  argptr = (ARG *)ActivityGetArg(ME);
  smnetptr = (SMNET *)argptr->mptr;
  req = smnetptr->req;
  case_num = EventGetState();
  
  while(!done) {
    switch(case_num) {

      /* Handle new requests from the bus module */
    case 0:
      if (loop_count == smnetptr->num_ports){
	/* We have already gone through all the ports in round-robin fashion
	   without any success, make the module go to sleep */
	smnetptr->Sim = (ACTIVITY *)YS__ActEvnt;
	smnetptr->req = NULL;
	smnetptr->state = WAIT_INQ_EMPTY;
	EventSetState(ME, 0);
	done = TRUE;
	break;
      }

      /* Look for request in first port; next_port contains the
	 last port where we left off in the round-robin scheduling */
      req = (REQ *) peekQ(smnetptr->in_port_ptr[smnetptr->next_port]);
      if (req == NULL) {                /* queue is empty, check next one */
	case_num = 0;
	if (++smnetptr->next_port == smnetptr->num_ports)
	  smnetptr->next_port = 0;/* go to the next port round-robin fashion */
	loop_count++;
	break;
      }
      
      req->in_port_num = smnetptr->next_port;
      if (++smnetptr->next_port == smnetptr->num_ports)
	smnetptr->next_port = 0;
      loop_count=0;

      /* at this point, we've peeked a new request, let us see start the
	 request processing the request */
      smnetptr->in_port_num = -1;
      smnetptr->req = req;
      smnetptr->Sim = NULL;
      smnetptr->state = PROCESSING;

      /**** Check for presence of enough buffers *************/

      /* First the reply buffers */
      if (req->s.type == REPLY || req->s.type == COHE_REPLY){
	if (smnetptr->replyQsz == smnetptr->Q_totsz)  {
	  /* in this case, we can't possibly process this request, so just
	     delay ourselves and come back to this later */
	  EventSetState(ME,0);
	  delay = (double)((struct Delays *)(smnetptr->Delays))->init_tfr_time;
	  if (delay < FASTER_PROC)
	    delay = FASTER_PROC;
	  ActivitySchedTime(ME, delay, INDEPENDENT);
	  done = TRUE;
	  break;
	}
	/* For REPLY network requests, we need to swap these -- this is to
	   faciliate ease of handling at the cache modules later */
	/* However, don't flip for cohe_replies if it's a WRB/REPL
	   request (which is sent as a COHE_REPLY), as these are
	   REPLYs that don't originate from another source. */
	if ((req->req_type != WRB && req->req_type != REPL)
	    || req->s.type != COHE_REPLY){
	  temp = req->src_node;
	  req->src_node = req->dest_node;
	  req->dest_node = temp;
	}
	case_num = 2;       /* REPLY handling */
      }
      /* The request buffers */
      else {
	if (smnetptr->reqQsz == smnetptr->Q_totsz)   {
	  /* in this case, we can't possibly process this request, so just
	     delay ourselves and come back to this later */
	  EventSetState(ME,0);
	  delay = (double)((struct Delays *)(smnetptr->Delays))->init_tfr_time;
	  if (delay < FASTER_PROC)
	    delay = FASTER_PROC;
	  ActivitySchedTime(ME, delay, INDEPENDENT);
	  done = TRUE;
	  break;
	}
	case_num = 1;            /* REQUEST handling */
      }

      /** So far, we only "peeked". now we can commit the request **/
      req1 = get_next_req_NOPR(smnetptr,req->in_port_num);
      if (req != req1){
	YS__errmsg("bus_get_next_req gives unexpected value: we lost a req?");
      }      
      
      if (req->src_node != smnetptr->node_num) {
	fprintf(simout,"****** smnet_send() error on inst_tag %d *********\n",
		req->s.inst_tag);
	fprintf(simout,"******** %d != %d ***************\n",
		req->src_node, smnetptr->node_num);
	YS__errmsg("SmnetSend(): Request src node number not right");
      }
      /* update stats */
      smnet_stat(smnetptr, req, req->s.type,0,0,0,0);
      req->blktime -= YS__Simtime; /* Stats */
      
#ifdef DEBUG_SMNET
      if (DEBUG_TIME < YS__Simtime){
	fprintf(simout,"SEND\t%s\t%ld\t%ld\t%s\tdest: %d\t%s @:%1.0f Size:%d\n",
		smnetptr->name, req->address,req->tag, Req_Type[req->req_type],
		req->dest_node, Request_st[req->s.type], YS__Simtime,
		req->size_st);
      }
#endif
      delay = (double)(((struct Delays *)(smnetptr->Delays))->init_tfr_time +
		       ((req->size_st/
			 (smnetptr->out_port_ptr[req->in_port_num]->width)) *
			(((struct Delays *)
			  (smnetptr->Delays))->flit_tfr_time)));
      /* transfer time = init_tfr_time + num_flits * flit_tfr_time */
      delay += (double)((struct Delays *)(smnetptr->Delays))->access_time;
      if (delay) {	        	/* if delay, reschedule event */
	EventSetState(ME, case_num);
	ActivitySchedTime(ME, delay, INDEPENDENT);
	done = TRUE;
	break;
      }
      break;
      
    case 1:			 /* requests (REQUEST and COHE types) */
      
      req->start_time = GetSimTime();
      addQ = ADDQ;
      if (req->src_node == req->dest_node) {
	YS__errmsg("Smnet: src node should never equal dest node");
      }
      if (smnetptr->reqQHead == NULL) {    /* add to request queue */
	/* If first element, schedule the ReqSendSemaWait routine */
	if (smnetptr->EvntReq) {
	  sim = (ACTIVITY *)smnetptr->EvntReq;
	  smnetptr->EvntReq = NULL;
	  ActivitySchedTime(sim, 0.0, INDEPENDENT);
	}
	smnetptr->reqQHead = req;
	smnetptr->reqQTail = req;
      }
      else {			
	smnetptr->reqQTail->next = req;
	smnetptr->reqQTail = req;
      }
      req->next = NULL;
      smnetptr->reqQsz ++;
      if (smnetptr->stat_level > 2 && MemsimStatOn)
	StatrecUpdate(smnetptr->reqQLStat, (double)smnetptr->reqQsz,
		      YS__Simtime);             /* update statistics */
      case_num = 0;                             /* goto next request */
      break;
      
    case 2:			/* replies (REPLY and COHE_REPLY types) */

      req->start_time = GetSimTime();
      if (req->src_node == req->dest_node) {
	YS__errmsg("Smnet: src node should never equal dest node");
      }

      /* Similar to requests, add to the reply q and wake up ReplySendSemaWait
	 if this is the first element to be added to the reply queue */
      if (smnetptr->replyQHead == NULL) {
	if (smnetptr->EvntReply) {
	  sim = (ACTIVITY *)smnetptr->EvntReply;
	  smnetptr->EvntReply = NULL; 
	  ActivitySchedTime(sim, 0.0, INDEPENDENT);
	}
	smnetptr->replyQHead = req;
	smnetptr->replyQTail = req;
      }
      else {
	smnetptr->replyQTail->next = req;
	smnetptr->replyQTail = req;
      }
      req->next = NULL;
      smnetptr->replyQsz ++;
      if (smnetptr->stat_level > 2 && MemsimStatOn)
	StatrecUpdate(smnetptr->replyQLStat, (double)smnetptr->replyQsz,
		      YS__Simtime);              /* update statistics */
      case_num = 0;                              /* goto next request */
      break;
      
    default:
      YS__errmsg("Unknown case in SmnetSned\n");
    }
  }
}

/*****************************************************************************/
/* ReqSendSemaWait():                                                        */
/*      Event body to wait on request port semaphore of smnet send           */
/*****************************************************************************/

void ReqSendSemaWait()
{
  SMNET *smnetptr;
  REQ *req;
  PACKET *pkt;
  MESSAGE *mesg;
  ARG *argptr;
  int case_num, seqno;
  int i, pkt_size;
  double delay = 0.0;
  int done = FALSE;

  /* As with all other yacsim modules, use ArgPointer to get the pointer
     to the module */
  argptr = (ARG *)ActivityGetArg(ME);
  smnetptr = (SMNET *)argptr->mptr;
  req = smnetptr->req;
  case_num = EventGetState();

  /* This procedure is called with value 0 when first called, in which
     case it goes to sleep on the pertinent IPort Semaphore. The Semaphore
     function wakes up this module with the right case number 1 */
  while(!done) {
    switch(case_num) {
    case 0:
      EventReschedSema(IPortSemaphore(smnetptr->iport_req),1);
      done = TRUE;		/* Wait on port semaphore if portq is full */
      break;
      
    case 1:			/* Space in port queue */
      req = smnetptr->reqQHead;	/* Get request from head of request queue */
      
      if (req) {
	smnetptr->reqQHead = req->next;
	if (smnetptr->reqQHead == NULL)
	  smnetptr->reqQTail = NULL;
	req->next = NULL;
	smnetptr->reqQsz --;
	
	if (smnetptr->stat_level > 2 && MemsimStatOn) {
	  StatrecUpdate(smnetptr->reqQLStat,
			(double)smnetptr->reqQsz, YS__Simtime); /* STATS */
	  StatrecUpdate(smnetptr->reqQTStat,
			YS__Simtime - req->start_time, 1.0);    /* STATS */
	}
	if (req->src_node == req->dest_node) {
	  YS__errmsg("Smnet: src_node should never equal dest_node");
	}

	/****************************************************
	  If there is space in the IPort, occupy it, else go
	  back to sleep once again on the IPort semaphore
	  ****************************************************/
	if (IPortSpace(smnetptr->iport_req)) {
	  /* Divide number of bytes sent by flit size and do a ceiling to
	     calculate number of flits in packet */
	  pkt_size = (int)((double)(req->size_st)/smnetptr->req_flitsz) + 
	    ((int)smnetptr->req_flitsz ?
	     (((req->size_st)%(int)smnetptr->req_flitsz)?1:0) : 0);

	  /************* Create a packet to be sent across the network ******/
	  if (smnetptr->nettype == XBAR_NET)
	    pkt_size += smnetptr->num_hps;
	  mesg = (MESSAGE *)YS__PoolGetObj(&YS__MsgPool);
	  mesg->bufptr = (char *)req;
	  mesg->msgsize = pkt_size;
	  mesg->blockflag = NOBLOCK;
	  seqno = (req->tag << 12)|(req->src_node << 6)|(req->dest_node);
	  pkt = NewPacket(seqno, mesg, pkt_size);	/* Create a packet */
	  req->blktime += YS__Simtime;                  /* Stats */

	  /* The PacketSend routines are all defined as part of NetSim */
	  delay = PacketSend(pkt, smnetptr->iport_req, req->src_node,
			     req->dest_node);
	  /* Send packet */
	  if (delay == -1.0) 
	    YS__errmsg("IPortSpace was not 0; port queue should not be full");
	  else if (delay) {
	    EventSetState(ME, 0);
	    ActivitySchedTime(ME, delay, INDEPENDENT);
	    done = TRUE;
	    break;
	  }
	  /* No space, go to sleep on this semaphore */
	  case_num = 0;
	  break;
	} 
	else
	  YS__errmsg("ReqSendSemaWait(): Just got out of semaphore; must have\
                       space in queue");
      }
      else {
	/* Request queue empty; Event goes to sleep */
	SemaphoreSignal(IPortSemaphore(smnetptr->iport_req));
	smnetptr->EvntReq = YS__ActEvnt;
	EventSetState(ME, 0);
	done = TRUE;
	break;
      }
    }  /* End of switch statement */
  } /* end of while(done) statement */
}

/*****************************************************************************/
/* ReplySendSemaWait():                                                      */
/*   Event that waits on reply queue of smnet send module                    */
/*****************************************************************************/

/* This routine is very similar to the RequestSendSemaWait, except that
   this handles the ReplyQ buffers. For more detailed comments, see above */

void ReplySendSemaWait()
{
  SMNET *smnetptr;
  REQ *req;
  PACKET *pkt;
  MESSAGE *mesg;
  ARG *argptr;
  int case_num, seqno;
  int i, pkt_size;
  double delay = 0.0;
  int done = FALSE;

  /* Get module pointer */
  argptr = (ARG *)ActivityGetArg(ME);
  smnetptr = (SMNET *)argptr->mptr;
  req = smnetptr->req;
  case_num = EventGetState();

  /* Case 0 is when we need to go to sleep, case 1 is when we have
     a chance to grab the IPort */
  while(!done) {
    switch(case_num) {
    case 0:
      EventReschedSema(IPortSemaphore(smnetptr->iport_reply),1);
      done = TRUE;		/* Wait on port semaphore if portq is full */
      break;
      
    case 1:			   /* Space in port queue */
      req = smnetptr->replyQHead;   /** <-- Look at replies this time **/
      if (req) 	{
	smnetptr->replyQHead = req->next;
	if (smnetptr->replyQHead == NULL)
	  smnetptr->replyQTail = NULL;
	req->next = NULL;
	smnetptr->replyQsz --;

	if (smnetptr->stat_level > 2 && MemsimStatOn){
	  StatrecUpdate(smnetptr->replyQLStat, (double)smnetptr->replyQsz,
			YS__Simtime);                          /* STATS */
	  StatrecUpdate(smnetptr->replyQTStat,
			YS__Simtime - req->start_time, 1.0); /* STATS */ 
	}
	if (req->src_node == req->dest_node) {
	  YS__errmsg("Smnet: src_node should never equal dest_node");
	}
	if (IPortSpace(smnetptr->iport_reply)) {
	  /* Divide the size sent by flit size and take the ceiling to get
	     packet size in flits */
	  pkt_size = (int)((double)(req->size_st)/smnetptr->reply_flitsz) + 
	    ((int)smnetptr->reply_flitsz
	     ? (((req->size_st)%(int)smnetptr->reply_flitsz)?1:0) : 0);

	  /***** Create packet to send out to the network *****/
	  if (smnetptr->nettype == XBAR_NET)
	    pkt_size += smnetptr->num_hps;
	  mesg = (MESSAGE *)YS__PoolGetObj(&YS__MsgPool);
	  mesg->bufptr = (char *)req;
	  mesg->msgsize = pkt_size;
	  mesg->blockflag = NOBLOCK;
	  seqno = (req->tag << 12)|(req->src_node << 6)|(req->dest_node);
	  pkt = NewPacket(seqno, mesg, pkt_size);	 /* Packet Create */
	  req->blktime += YS__Simtime;                   /* Stats */
	  
	  delay = PacketSend(pkt, smnetptr->iport_reply,
			     req->src_node, req->dest_node);
	  if (delay == -1.0)	/* Packet Send */
	    YS__errmsg("IPortSpace was not 0; port queue should not be full");
	  else if (delay) {
	    EventSetState(ME, 0);
	    ActivitySchedTime(ME, delay, INDEPENDENT);
	    done = TRUE;
	    /* Schedule event for delay to send packet out of port */
	    break;
	  }
	  case_num = 0;
	  break;
	} 
	else {
	  fprintf(simout,"smnet: %d\t",smnetptr->node_num);
	  YS__errmsg("ReplySendSemaWait(): Just got out of semaphore; must have space in queue");
	}
      }
      else {
	SemaphoreSignal(IPortSemaphore(smnetptr->iport_reply));
	smnetptr->EvntReply = YS__ActEvnt;
	EventSetState(ME, 0);
	done = TRUE;		/* Reply queue is empty; event goes to sleep */
	break;
      }
    }
  }
}

/*****************************************************************************/
/* NewSmnetRcv :                                                             */
/*   This routine initializes and returns a pointer to a smnet module.       */
/*****************************************************************************/

/**************************************************************************
  NOTE:

  The SmnetRcv module is implemented in a fashion very similar to that
  of the SmnetSend module. The key differences are that the SmnetRcv module
  interfaces to the output port modules instead of the input port modules.
  *************************************************************************/

SMNET *NewSmnetRcv(name, node_num, stat_level, routing, Delays, ports_abv,
		   ports_blw, smnet_type,  oport_reply, oport_req, portq_size)
     char   *name;	/* Name of the module upto 32 characters long        */
     int    node_num;   /* the node number of this module */
     int    stat_level;	/* Level of statistics collection for this module  */
     rtfunc routing;	/* Routing function of the module                */
     struct Delays *Delays; /* Pointer to user-defined structure for delays */
     int    ports_abv;		/* Number of ports above this module */
     int    ports_blw;		/* Number of ports below this module */
     int    smnet_type;         /* type of network interface */
     OPORT  *oport_reply;       /* the reply output port from the network */
     OPORT  *oport_req;         /* the request output port from the network */
     int    portq_size;         /* Size of buffering */
{
  
  SMNET *smnetptr;
  ARG *arg;
  char evnt_name[32];

  /* Initialize the data structures common to all shared smnet modules */
  smnetptr = (SMNET *)malloc(sizeof(SMNET));
  if (smnet_index < 150) 
    smnet_ptr[smnet_index ++] = smnetptr;
  smnetptr->id = YS__idctr++;	/* System assigned unique ID   */
  strncpy(smnetptr->name, name,31);
  smnetptr->name[31] = '\0';
  smnetptr->module_type = SMNET_RCV_MODULE;
  ModuleInit ((SMMODULE *)smnetptr, node_num, stat_level, routing, Delays,
	      ports_abv + ports_blw, portq_size);
  smnetptr->num_ports_abv = ports_abv;
  smnetptr->Sim  = NULL;
  smnetptr->rm_q = (char *)malloc(sizeof(RMQ));
  if (smnetptr->rm_q == NULL)
    YS__errmsg("NewSmnet(): malloc failed");
  /* These are used for round-robin scheduling of ports */
  ((RMQ *)(smnetptr->rm_q))->u1.abv = 0;
  ((RMQ *)(smnetptr->rm_q))->u2.blw = 0;
  
  smnetptr->req = NULL;
  smnetptr->in_port_num = -1;
  smnetptr->wakeup = wakeup_smnetrcv;	/* Wakeup routine for this module */
  smnetptr->handshake = SmnetRcvHandshake;
  /* The handshake routine does takes care of the actual movement of the
     packets from the reqQ and replyQ onto the bus ports */  
  smnetptr->oport_reply = oport_reply;
  smnetptr->oport_req = oport_req;
  smnetptr->iport_reply = NULL;     /* Smnet send had iports, */
  smnetptr->iport_req = NULL;       /* Smnet receive has oports */

  /* As before, Create event for manipulation of reply and request queues */
  sprintf(evnt_name, "%s_rcv_rep",name);
  smnetptr->EvntReply  = NewEvent(evnt_name, ReplyRcvSemaWait,  NODELETE, 0);
  arg = (ARG *)malloc(sizeof(ARG));
  arg->mptr = (SMMODULE *)smnetptr; /* Initialize arg pointer for this event */
  ActivitySetArg((ACTIVITY *)smnetptr->EvntReply, arg, UNKNOWN);
  
  sprintf(evnt_name, "%s_rcv_req",name);
  smnetptr->EvntReq  = NewEvent(evnt_name, ReqRcvSemaWait,  NODELETE, 0);
  arg = (ARG *)malloc(sizeof(ARG));
  arg->mptr = (SMMODULE *)smnetptr; /* Initialize arg pointer for this event */
  ActivitySetArg((ACTIVITY *)smnetptr->EvntReq, arg, UNKNOWN);

  /* Intialize the buffers */
  smnetptr->Q_totsz = portq_size/2;
  if (smnetptr->Q_totsz == 0)
    smnetptr->Q_totsz = 1;        /* the smnet needs some internal buffering */
  smnetptr->reqQHead = NULL;
  smnetptr->reqQTail = NULL;
  smnetptr->reqQsz =  0;
  smnetptr->replyQHead = NULL;
  smnetptr->replyQTail = NULL;
  smnetptr->replyQsz = 0;
  smnetptr->next_port = 0;
  smnetptr->prnt_rtn = SmnetRcvStatReport;
  
  if (stat_level > 2) {
    sprintf(name,"ReqQLen_%d",node_num);
    smnetptr->reqQLStat = NewStatrec(name, INTERVAL, MEANS, NOHIST, 0,0,0);
    sprintf(name,"ReplyQLen_%d",node_num);
    smnetptr->replyQLStat = NewStatrec(name, INTERVAL, MEANS, NOHIST, 0,0,0);
    
    sprintf(name,"ReqQTim_%d",node_num);
    smnetptr->reqQTStat = NewStatrec(name, POINT, MEANS, NOHIST, 0,0,0);
    sprintf(name,"ReplyQTim_%d",node_num);
    smnetptr->replyQTStat = NewStatrec(name, POINT, MEANS, NOHIST, 0,0,0);
  }
  return smnetptr;
}

/* ReqRcvSemaWait and ReplyRcvSemaWait are implemented very analagous to
   their counter parts in SmnetSend */

/*****************************************************************************/
/* ReqRcvSemaWait() :                                                        */
/*    This is the event that waits on the semaphore of request queue to      */
/*    receive a request                                                      */
/*****************************************************************************/

void ReqRcvSemaWait()
{
  SMNET *smnetptr;
  REQ *req;
  PACKET *pkt;
  PKTDATA *pktdata;
  ARG *argptr;
  int case_num, add=FALSE, oport_num;
  int done = FALSE;

  /* Get module pointer */
  argptr = (ARG *)ActivityGetArg(ME);
  smnetptr = (SMNET *)argptr->mptr;
  req = smnetptr->req;
  case_num = EventGetState();
  
  while(!done) {
    switch(case_num) {
    case 0:
      /************ Check for packets waiting to be processed ********/
      if (!OPortPackets(smnetptr->oport_req)) {
	EventReschedSema(OPortSemaphore(smnetptr->oport_req), 0);
	done = TRUE;
	break;
      }
      else {
	/****** Check for buffering ********/
	if (smnetptr->reqQsz == smnetptr->Q_totsz) {
	  /* If request queue of module is full */
	  smnetptr->EvntReq = YS__ActEvnt;
	  done = TRUE;
	  break;
	}

	/*********** Receive packet from nework *********/
	pkt = PacketReceive(smnetptr->oport_req);
	if (pkt == NULL)
	  YS__errmsg("OPortPackets did not return 0; Must have packet in out port");
	pktdata = PacketGetData(pkt);
	req = (REQ *)(pktdata->mesgptr->bufptr);
	req->start_time = YS__Simtime;
	if (req->dest_node != smnetptr->node_num)
	  YS__errmsg("ReqRcvSemaWait(): Request dest node number not right");
	
	smnet_stat(smnetptr, req, req->s.type,1,pktdata, REQ_NET);  /* STATS */
	
#ifdef DEBUG_SMNET
	if (DEBUG_TIME < YS__Simtime)
	 fprintf(simout,"RCV\t%s\t%ld\t%ld\t%s\tsrc: %d\t%s @:%1.0f Size:%d\n",
		 smnetptr->name, req->address,
		 req->tag, Req_Type[req->req_type], req->src_node,
		 Request_st[req->s.type], YS__Simtime, req->size_st);
#endif
	/* Return packet related data structures back to the pool */
	YS__PoolReturnObj(&YS__MsgPool, pktdata->mesgptr);
	YS__PoolReturnObj(&YS__PktPool, pkt);
	
	oport_num = smnetptr->routing((SMMODULE *)smnetptr, req); 
	if (oport_num == -1) {
	  fprintf(simout,"ReqRcvSemaWAit(): Request leaving smnet module \"%s\" cannot be routed\n",
		  smnetptr->name);
	  YS__errmsg("ReqRcvSemaWAit(): Routing function is unable to route this request");
	}
	
	add = TRUE;
	if (add && smnetptr->out_port_ptr[oport_num]->q_size == smnetptr->out_port_ptr[oport_num]->q_sz_tot)  {
#ifdef DEBUG_SMNET
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simerr,"stalling in smnet reqrcv for fullness...\n");
#endif
	  add = FALSE;
	}
	
	if (add) {
	  if (smnetptr->stat_level > 2 && MemsimStatOn)
	    StatrecUpdate(smnetptr->reqQTStat, 0.0, 1.0);          /* STATS */
	  if (!add_req(smnetptr->out_port_ptr[oport_num], req)) 
	    /* add request to output queue; returns 0, if queue is full */
	    YS__errmsg("ReqRcvSemaWait(): Out queue should not be full");
	}
	else {
	  if (smnetptr->reqQHead == NULL) { /* add to request queue */
	    smnetptr->reqQHead = req;
	    smnetptr->reqQTail = req;
	  }
	  else {			
	    smnetptr->reqQTail->next = req;
	    smnetptr->reqQTail = req;
	  }	   
	  req->next = NULL;
	  smnetptr->reqQsz ++;
	  if (smnetptr->stat_level > 2 && MemsimStatOn)
	    StatrecUpdate(smnetptr->reqQLStat, (double)smnetptr->reqQsz,
			  YS__Simtime);                           /* STATS */
	}
	case_num = 0;
	break;
      }
    default:
      YS__errmsg("ReqRcvSemaWait(): Unkown case number");
    }
  }
}

/*****************************************************************************/
/* ReplyRcvSemaWait() :                                                      */
/*   This is the event that waits on the semaphore of reply queue to receive */
/*   requests                                                                */
/*****************************************************************************/

void ReplyRcvSemaWait()
{
  SMNET *smnetptr;
  REQ *req;
  PACKET *pkt;
  PKTDATA *pktdata;
  ARG *argptr;
  int case_num, add=FALSE, oport_num;
  int done = FALSE;

  /* Get module pointer */
  argptr = (ARG *)ActivityGetArg(ME);
  smnetptr = (SMNET *)argptr->mptr;
  req = smnetptr->req;
  case_num = EventGetState();
  
  while(!done) {
    switch(case_num) {
    case 0:
      /* Check for packets waiting to be processed */
      if (!OPortPackets(smnetptr->oport_reply)) {
	EventReschedSema(OPortSemaphore(smnetptr->oport_reply), 0);
	done = TRUE;
	break;
      }
      else {
	if (smnetptr->replyQsz == smnetptr->Q_totsz) {
	  smnetptr->EvntReply = YS__ActEvnt;
	  done = TRUE;
	  break;
	}
	
	pkt = PacketReceive(smnetptr->oport_reply);
	if (pkt == NULL)
	  YS__errmsg("OPortPackets did not return 0; Must have packet in out port");
	pktdata = PacketGetData(pkt);
	req = (REQ *)(pktdata->mesgptr->bufptr);
	req->start_time = YS__Simtime;
	if (req->dest_node != smnetptr->node_num)
	  YS__errmsg("ReplyRcvSemaWAit(): Request dest node number not right");	
	smnet_stat(smnetptr, req, req->s.type,1,pktdata, REPLY_NET);/* STATS */
	
#ifdef DEBUG_SMNET
	if (DEBUG_TIME < YS__Simtime)
	fprintf(simout,"RCV\t%s\t%ld\t%ld\t%s\tsrc: %d\t%s @:%1.0f Size:%d\n",
		smnetptr->name, req->address,
	       req->tag, Req_Type[req->req_type], req->src_node,
		Request_st[req->s.type], YS__Simtime, req->size_st);
#endif

	/* Return packet related structures to the pool */
	YS__PoolReturnObj(&YS__MsgPool, pktdata->mesgptr);
	YS__PoolReturnObj(&YS__PktPool, pkt);
	
	oport_num = smnetptr->routing((SMMODULE *)smnetptr, req); 
	if (oport_num == -1) {
	  fprintf(simout,"ReplyRcvSemaWAit(): Request leaving smnet module \"%s\" cannot be routed\n",
		 smnetptr->name);
	  YS__errmsg("ReplyRcvSemaWAit(): Routing function is unable to route this request");
	}
	add = TRUE;
	if (add && smnetptr->out_port_ptr[oport_num]->q_size
	    == smnetptr->out_port_ptr[oport_num]->q_sz_tot)  {
#ifdef DEBUG_SMNET
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simerr,"stalling in smnet replyrcv for fullness...\n");
#endif
	  add = FALSE;
	}
	
	if (add) {
	  if (smnetptr->stat_level > 2 && MemsimStatOn)
	    StatrecUpdate(smnetptr->replyQTStat, 0.0, 1.0); /* STATS */
	  if (!add_req_head(smnetptr->out_port_ptr[oport_num], req)) 
	    /* add request to output queue; returns 0, if queue is full */
	    YS__errmsg("ReplyRcvSemaWait(): Out queue should not be full");
	}
	else {
	  if (smnetptr->replyQHead == NULL) { /* add to request queue */
	    smnetptr->replyQHead = req;
	    smnetptr->replyQTail = req;
	  }
	  else {			
	    smnetptr->replyQTail->next = req;
	    smnetptr->replyQTail = req;
	  }	   
	  req->next = NULL;
	  smnetptr->replyQsz ++;
	  if (smnetptr->stat_level > 2 && MemsimStatOn)
	    StatrecUpdate(smnetptr->replyQLStat, (double)smnetptr->replyQsz,
			  YS__Simtime);                      /* STATS */
	}	
	case_num = 0;
	break;
      }
    default:
      YS__errmsg("ReplyRcvSemaWait(): Unkown case number");
    }
  }
}

/****************************************************************************/
/* smnet_stat:                                                              */
/* This routine is called by the smnet module in the beginning              */
/* to keep track of the statistics                                          */
/****************************************************************************/
void smnet_stat(smnetptr, req, type, pktstat, pktdata, mesh_num)
     SMNET *smnetptr;
     REQ *req;
     int type, pktstat, mesh_num;
     PKTDATA *pktdata;
{
  double total, net, blk;
  int num_hops, pktsize, reqsz;
  if (MemsimStatOn ) {
    smnetptr->num_ref ++;	/* Number of references to cache module */
    if (smnetptr->stat_level > 1) {
      if (type == REQUEST)
	smnetptr->num_req ++;
      else if (type == REPLY)
	smnetptr->num_rep ++;
      else if (type == COHE)
	smnetptr->num_cohe ++;
      else if (type == COHE_REPLY)
	smnetptr->num_cohe_rep ++;
      
      if (smnetptr->stat_level > 2) {
	if (req->size_st == WORDSZ || req->size_st == (WORDSZ-1)
	    || req->size_st == (WORDSZ+1))
	  reqsz = SZ_WORD;
	else if (req->size_st == WORDSZ*2 || req->size_st == (WORDSZ*2-1)
		 || req->size_st == (WORDSZ*2+1))
	  reqsz = SZ_DBL;
	else if (req->size_st == blocksize || req->size_st == (blocksize-1)
		 || req->size_st == (blocksize+1))
	  reqsz = SZ_BLK;
	else reqsz = SZ_OTHER;
	
	smnetptr->num_sz_ref[reqsz]++;
      }
      if (smnetptr->stat_level > 3) {
	if (pktstat) {
	  total = GetSimTime() - pktdata->createtime + pktdata->recvtime;
	  net = pktdata->nettime;
	  blk = pktdata->blktime;
	  num_hops = pktdata->num_hops-1;
	  /* This is added because we have an extra demux on the REPLY_NET */
	  if (mesh_num == REPLY_NET)
	      num_hops--;
	  req->blktime += blk;
	  req->blktime -= YS__Simtime;
	  
	  pktsize = pktdata->pktsize;
	  StatrecUpdate(PktSzHist[mesh_num], (double)pktsize, 1.0);
	  if (PktNumHopsHist[mesh_num])	
	    StatrecUpdate(PktNumHopsHist[mesh_num], (double)num_hops, 1.0);
	  
	  StatrecUpdate(PktTOTimeTotalMean[mesh_num], total, 1.0);
	  StatrecUpdate(PktTOTimeNetMean[mesh_num], net, 1.0);
	  StatrecUpdate(PktTOTimeBlkMean[mesh_num], blk, 1.0);
	  
	  if (net_index[pktsize] != -2) {
	    if (net_index[pktsize] == -1)
	      if (cur_index < (NUM_SIZES-1)){
		net_index[pktsize] = cur_index;
		rev_index[cur_index] = pktsize;
		cur_index ++;
	      }
	      else {
		YS__warnmsg("Cannot collect stats for this packet size; ALready collecting stats for NUM_SIZES packets");
		net_index[pktsize] = -2;
	      }
	    StatrecUpdate(PktSzTimeTotalMean[mesh_num][net_index[pktsize]], total, 1.0);
	    StatrecUpdate(PktSzTimeNetMean[mesh_num][net_index[pktsize]], net, 1.0);
	    StatrecUpdate(PktSzTimeBlkMean[mesh_num][net_index[pktsize]], blk, 1.0);
	  }
	  if (PktNumHopsHist[mesh_num]) {	
	    StatrecUpdate(PktHpsTimeTotalMean[mesh_num][num_hops], total, 1.0);
	    StatrecUpdate(PktHpsTimeNetMean[mesh_num][num_hops], net, 1.0);
	    StatrecUpdate(PktHpsTimeBlkMean[mesh_num][num_hops], blk, 1.0);
	  }
	}
      }
    }
  }
}
/*****************************************************************************/
/* SmnetStatReportAll :                                                      */
/* Reports statistics of all smnet modules                                   */
/*****************************************************************************/
void SmnetStatReportAll()
{
  int i;
  int temp;

  if (smnet_index) {
    if (smnet_index == 150) 
      YS__warnmsg("Greater than 150 memories created; statistics for the first 150 will be reported by SmnetStatReportAll");
    fprintf(simout,"\n##### Smnet Statistics #####\n");
    strncpy(reqQLStat[0].name, "Send_Req_Q_Len_Avg", 32);
    reqQLStat[0].minval = 10000000.0; 
    reqQLStat[0].maxval = 0.0; 
    reqQLStat[0].samples = 0; reqQLStat[0].sum = 0.0;

    strncpy(replyQLStat[0].name, "Send_Reply_Q_Len_Avg", 32);
    replyQLStat[0].minval = 10000000.0; replyQLStat[0].maxval = 0.0; 
    replyQLStat[0].samples = 0; replyQLStat[0].sum = 0.0;

    strncpy(reqQTStat[0].name, "Send_Req_Q_Time_Avg", 32);
    reqQTStat[0].minval = 10000000.0; reqQTStat[0].maxval = 0.0; 
    reqQTStat[0].samples = 0; reqQTStat[0].sum = 0.0;

    strncpy(replyQTStat[0].name, "Send_Reply_Q_Time_Avg", 32);
    replyQTStat[0].minval = 10000000.0; replyQTStat[0].maxval = 0.0; 
    replyQTStat[0].samples = 0; replyQTStat[0].sum = 0.0;

    strncpy(reqQLStat[1].name, "Rcv_Req_Q_Len_Avg", 32);
    reqQLStat[1].minval = 10000000.0; reqQLStat[1].maxval = 0.0; 
    reqQLStat[1].samples = 0; reqQLStat[1].sum = 0.0;

    strncpy(replyQLStat[1].name, "Rcv_Reply_Q_Len_Avg", 32);
    replyQLStat[1].minval = 10000000.0; replyQLStat[1].maxval = 0.0; 
    replyQLStat[1].samples = 0; replyQLStat[1].sum = 0.0;

    strncpy(reqQTStat[1].name, "Rcv_Req_Q_Time_Avg", 32);
    reqQTStat[1].minval = 10000000.0; reqQTStat[1].maxval = 0.0; 
    reqQTStat[1].samples = 0; reqQTStat[1].sum = 0.0;

    strncpy(replyQTStat[1].name, "Rcv_Reply_Q_Time_Avg", 32);
    replyQTStat[1].minval = 10000000.0; replyQTStat[1].maxval = 0.0; 
    replyQTStat[1].samples = 0; replyQTStat[1].sum = 0.0;

    greqfr[0] = 0.0; grepfr[0] = 0.0; gcohefr[0] = 0.0; 
    gcohe_refr[0] = 0.0; ghitpr[0] = 0.0; glat[0] = 0.0; 
    gutil[0] = 0.0; gnum_ref[0] = 0.0;
    for (i=0; i<NUM_SIZES; i++)
      gszfr[0][i] = 0.0;

    greqfr[1]  = 0.0; grepfr[1] = 0.0; gcohefr[1] = 0.0; 
    gcohe_refr[1] = 0.0; ghitpr[1] = 0.0;  glat[1] = 0.0;  
    gutil[1] = 0.0; gnum_ref[1] = 0.0;
    for (i=0; i<NUM_SIZES; i++)
      gszfr[1][i] = 0.0;


    for (i=0; i< smnet_index; i++) 
      if (smnet_ptr[i]->prnt_rtn)
	(smnet_ptr[i]->prnt_rtn)(smnet_ptr[i]);

    temp = smnet_index / 2;	/*** Assuming that there are equal number
				  of send and rcv modules */

    greqfr [0] /= (double)temp;
    grepfr [0] /= (double)temp;
    gcohefr [0] /= (double)temp;
    gcohe_refr [0] /= (double)temp;
    ghitpr [0] /= (double)temp;
    glat [0] /= (double)temp;
    gutil [0] /= (double)temp;
    gnum_ref [0] /= temp;

    fprintf(simout,"\nName          Num_Ref          Hit_Rate          Miss_latency     Utilization\n");
    fprintf(simout,"Smnet_Send_Avg  %d           %5.2g%%           %6.4g             %6.4g\n", 
	 gnum_ref[0], ghitpr[0], glat[0], gutil[0]);
    fprintf(simout,"              Request          Reply              Coherence        Cohe Reply\n");
    fprintf(simout,"               %6.4g           %6.4g              %6.4g            %6.4g\n", 
	   greqfr[0], grepfr[0],gcohefr[0], gcohe_refr[0]);
    fprintf(simout,"              Word             Double             Line             Other\n");
    fprintf(simout,"              %6.4g            %6.4g              %6.4g            %6.4g\n",
	   gszfr[0][SZ_WORD]/(double)temp, gszfr[0][SZ_DBL]/(double)temp, 
	   gszfr[0][SZ_BLK]/(double)temp, gszfr[0][SZ_OTHER]/(double)temp);

    fprintf(simout,"\nStatistics Record %s:\n", reqQLStat[0].name);
    fprintf(simout,"   Number of intervals = %d, Max Value = %g, Min Value = %g",
	   reqQLStat[0].samples/temp, reqQLStat[0].maxval, reqQLStat[0].minval);
    fprintf(simout,"   Mean = %g\n",  reqQLStat[0].sum/(double)temp);

    fprintf(simout,"\nStatistics Record %s:\n", replyQLStat[0].name);
    fprintf(simout,"   Number of intervals = %d, Max Value = %g, Min Value = %g",
	   replyQLStat[0].samples/temp, replyQLStat[0].maxval, replyQLStat[0].minval);
    fprintf(simout,"   Mean = %g\n",  replyQLStat[0].sum/(double)temp);

    fprintf(simout,"\nStatistics Record %s:\n", reqQTStat[0].name);
    fprintf(simout,"   Number of intervals = %d, Max Value = %g, Min Value = %g",
	   reqQTStat[0].samples/temp, reqQTStat[0].maxval, reqQTStat[0].minval);
    fprintf(simout,"   Mean = %g\n",  reqQTStat[0].sum/(double)temp);

    fprintf(simout,"\nStatistics Record %s:\n", replyQTStat[0].name);
    fprintf(simout,"   Number of intervals = %d, Max Value = %g, Min Value = %g",
	   replyQTStat[0].samples/temp, replyQTStat[0].maxval, replyQTStat[0].minval);
    fprintf(simout,"   Mean = %g\n",  replyQTStat[0].sum/(double)temp);


    greqfr [1] /= (double)temp;
    grepfr [1] /= (double)temp;
    gcohefr [1] /= (double)temp;
    gcohe_refr [1] /= (double)temp;
    ghitpr [1] /= (double)temp;
    glat [1] /= (double)temp;
    gutil [1] /= (double)temp;
    gnum_ref [1] /= temp;

    fprintf(simout,"\nName          Num_Ref          Hit_Rate          Miss_latency     Utilization\n");
    fprintf(simout,"Smnet_Rcv_Avg  %d           %5.2g%%           %6.4g             %6.4g\n", 
	 gnum_ref[1], ghitpr[1], glat[1], gutil[1]);
    fprintf(simout,"              Request          Reply              Coherence        Cohe Reply\n");
    fprintf(simout,"               %6.4g           %6.4g              %6.4g            %6.4g\n", 
	   greqfr[1], grepfr[1],gcohefr[1], gcohe_refr[1]);
    fprintf(simout,"              Word             Double             Line             Other\n");
    fprintf(simout,"              %6.4g            %6.4g              %6.4g            %6.4g\n", 
	   gszfr[1][SZ_WORD]/(double)temp, gszfr[1][SZ_DBL]/(double)temp, 
	   gszfr[1][SZ_BLK]/(double)temp, gszfr[1][SZ_OTHER]/(double)temp);

    fprintf(simout,"\nStatistics Record %s:\n", reqQLStat[1].name);
    fprintf(simout,"   Number of intervals = %d, Max Value = %g, Min Value = %g",
	   reqQLStat[1].samples/temp, reqQLStat[1].maxval, reqQLStat[1].minval);
    fprintf(simout,"   Mean = %g\n",  (reqQLStat[1].sum/(double)temp));

    fprintf(simout,"\nStatistics Record %s:\n", replyQLStat[1].name);
    fprintf(simout,"   Number of intervals = %d, Max Value = %g, Min Value = %g",
	   replyQLStat[1].samples/temp, replyQLStat[1].maxval, replyQLStat[1].minval);
    fprintf(simout,"   Mean = %g\n",  replyQLStat[1].sum/(double)temp);

    fprintf(simout,"\nStatistics Record %s:\n", reqQTStat[1].name);
    fprintf(simout,"   Number of intervals = %d, Max Value = %g, Min Value = %g",
	   reqQTStat[1].samples/temp, reqQTStat[1].maxval, reqQTStat[1].minval);
    fprintf(simout,"   Mean = %g\n",  reqQTStat[1].sum/(double)temp);

    fprintf(simout,"\nStatistics Record %s:\n", replyQTStat[1].name);
    fprintf(simout,"   Number of intervals = %d, Max Value = %g, Min Value = %g",
	   replyQTStat[1].samples/temp, replyQTStat[1].maxval, replyQTStat[1].minval);
    fprintf(simout,"   Mean = %g\n",  replyQTStat[1].sum/(double)temp);
  }
}

/*****************************************************************************/
/* SmnetStatClearAll() :                                                     */
/* Reports statistics of all smnet modules                                   */
/*****************************************************************************/

void SmnetStatClearAll()
{
  int i;
  if (smnet_index) {
    for (i=0; i< smnet_index; i++) 
      SmnetStatClear(smnet_ptr[i]);
  }
}


/*****************************************************************************/
/* SmnetSendStatReport :                                                     */
/* Reports statistics of a smnet module.                                     */
/*****************************************************************************/

void SmnetSendStatReport (smnetptr)
SMNET *smnetptr;
{
  int i;
  double reqfr, repfr, cohefr, cohe_refr;
  double szfr[NUM_SIZES];
  double  hitpr, util, lat;

  SMModuleStatReport(smnetptr , &hitpr, &lat, &util);
  gnum_ref [0] += smnetptr->num_ref;
  ghitpr [0] += hitpr; glat[0] +=lat; gutil [0] += util;

  if (smnetptr->stat_level > 1) {
    
    if (smnetptr->num_ref) {
      reqfr = (double)smnetptr->num_req/(double)smnetptr->num_ref;
      repfr = (double)smnetptr->num_rep/(double)smnetptr->num_ref;
      cohefr = (double)smnetptr->num_cohe/(double)smnetptr->num_ref;
      cohe_refr = (double)smnetptr->num_cohe_rep/(double)smnetptr->num_ref;
    }
    else {reqfr = 0.0; repfr = 0.0; cohefr = 0.0; cohe_refr = 0.0; }

    if (smnetptr->num_ref) 
      for (i=0; i< NUM_SIZES; i++) {
	szfr[i] = (double)smnetptr->num_sz_ref[i]/(double)smnetptr->num_ref;
	if (smnet_index == 0)	gszfr[0][i] = szfr[i];
	else gszfr[0][i] += szfr[i];

      }
    
    greqfr [0] += reqfr;
    grepfr [0] += repfr;
    gcohefr [0] += cohefr;
    gcohe_refr [0] += cohe_refr;
    
    fprintf(simout,"              Request          Reply              Coherence        Cohe Reply\n");
    fprintf(simout,"         %10d(%6.4g) %8d(%6.4g) %8d(%6.4g) %8d(%6.4g)\n", 
	    smnetptr->num_req, reqfr, smnetptr->num_rep, repfr, smnetptr->num_cohe, 
	    cohefr, smnetptr->num_cohe_rep, cohe_refr);
    fprintf(simout,"              Word             Double             Line             Other\n");
    fprintf(simout,"         %10d(%6.4g) %8d(%6.4g) %8d(%6.4g) %8d(%6.4g)\n", 
	    smnetptr->num_sz_ref[SZ_WORD], szfr[SZ_WORD], 
	    smnetptr->num_sz_ref[SZ_DBL], szfr[SZ_DBL], 
	    smnetptr->num_sz_ref[SZ_BLK], szfr[SZ_BLK], 
	    smnetptr->num_sz_ref[SZ_OTHER], szfr[SZ_OTHER]);
  }
  
  if (smnetptr->stat_level > 2) {
    StatrecReport(smnetptr->reqQLStat);
    StatrecReport(smnetptr->replyQLStat);
    StatrecReport(smnetptr->reqQTStat);
    StatrecReport(smnetptr->replyQTStat);
  }
}

/*****************************************************************************/
/* SmnetRcvStatReport:                                                       */
/* Reports statistics of a smnet module.                                     */
/*****************************************************************************/

void SmnetRcvStatReport (smnetptr)
SMNET *smnetptr;
{
  int i;
  double reqfr, repfr, cohefr, cohe_refr;
  double szfr[NUM_SIZES];
  double  hitpr, util, lat;

  SMModuleStatReport(smnetptr , &hitpr, &lat, &util);
  gnum_ref [1] += smnetptr->num_ref;
  ghitpr [1] += hitpr; glat[1] +=lat; gutil [1] += util;

  if (smnetptr->stat_level > 1) {
    if (smnetptr->num_ref) {
      reqfr = (double)smnetptr->num_req/(double)smnetptr->num_ref;
      repfr = (double)smnetptr->num_rep/(double)smnetptr->num_ref;
      cohefr = (double)smnetptr->num_cohe/(double)smnetptr->num_ref;
      cohe_refr = (double)smnetptr->num_cohe_rep/(double)smnetptr->num_ref;
    }
    else {reqfr = 0.0; repfr = 0.0; cohefr = 0.0; cohe_refr = 0.0; }

    if (smnetptr->num_ref) 
      for (i=0; i< NUM_SIZES; i++) {
	szfr[i] = (double)smnetptr->num_sz_ref[i]/(double)smnetptr->num_ref;
	if (smnet_index == 0)	gszfr[1][i] = szfr[i];
	else gszfr[1][i] += szfr[i];
      }
    
    greqfr [1] += reqfr;
    grepfr [1] += repfr;
    gcohefr [1] += cohefr;
    gcohe_refr [1] += cohe_refr;
    
    fprintf(simout,"              Request          Reply              Coherence        Cohe Reply\n");
    fprintf(simout,"         %10d(%6.4g) %8d(%6.4g) %8d(%6.4g) %8d(%6.4g)\n", 
	    smnetptr->num_req, reqfr, smnetptr->num_rep, repfr, smnetptr->num_cohe, 
	    cohefr, smnetptr->num_cohe_rep, cohe_refr);
    fprintf(simout,"              Word             Double             Line             Other\n");
    fprintf(simout,"         %10d(%6.4g) %8d(%6.4g) %8d(%6.4g) %8d(%6.4g)\n", 
	    smnetptr->num_sz_ref[SZ_WORD], szfr[SZ_WORD], 
	    smnetptr->num_sz_ref[SZ_DBL], szfr[SZ_DBL], 
	    smnetptr->num_sz_ref[SZ_BLK], szfr[SZ_BLK], 
	    smnetptr->num_sz_ref[SZ_OTHER], szfr[SZ_OTHER]);
  }
    
  if (smnetptr->stat_level > 2) {
    StatrecReport(smnetptr->reqQLStat);
    StatrecReport(smnetptr->replyQLStat);
    StatrecReport(smnetptr->reqQTStat);
    StatrecReport(smnetptr->replyQTStat);
  }
}

/*****************************************************************************/
/* SmnetStatClear :                                                          */
/* Reports statistics of a smnet module.                                     */
/*****************************************************************************/
void SmnetStatClear (smnetptr)
SMNET *smnetptr;
{
  int i;
  SMModuleStatClear(smnetptr);

  if (smnetptr->stat_level > 1) {
    smnetptr->num_req = 0;
    smnetptr->num_rep = 0;
    smnetptr->num_cohe = 0;
    smnetptr->num_cohe_rep = 0;
    for (i=0; i<NUM_SIZES; i++)
      smnetptr->num_sz_ref[i] = 0;
    StatrecReset(smnetptr->reqQLStat);
    StatrecReset(smnetptr->replyQLStat);
    StatrecReset(smnetptr->reqQTStat);
    StatrecReset(smnetptr->replyQTStat);
  }
}

