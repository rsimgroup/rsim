/*
  route.c

  This file includes functions that handle the ports connecting the
  various modules in the memory system simulator.

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
#include "MemSys/arch.h"
#include "MemSys/directory.h"
#include "MemSys/misc.h"
#include "MemSys/cache.h"
#include "MemSys/bus.h"
#include "Processor/simio.h"


/*****************************************************************************/
/*  The main functions here are the routing functions. Each module calls     */
/*  its routing function before sending out a message to another module.     */
/*  (the "send" portion of the network interface does not call a routing     */
/*  function, as it sends packets to the network, not directly to another    */
/*  module).								     */
/*									     */
/*  The routing functions used by the various modules are as follows:	     */
/*									     */
/*  Module                           Function				     */
/*  ------                           --------				     */
/*  CPU                              routing1				     */
/*  L1 cache                         routing_L1				     */
/*  Write buffer                     routing_WB				     */
/*  L2 cache                         routing_L2				     */
/*  Bus                              routing_bus			     */
/*  Directory                        routing_dir			     */
/*  Network interface (receive)      routing_SMNET			     */
/*  Network interface (send)         routing6 (unused)			     */
/*									     */
/*  These functions assume the port connections specified in the RSIM	     */
/*  manual.  Any changes to this port diagram (besides simply using the	     */
/*  "-I" option to increase/decrease the number of memory banks) will	     */
/*  require changes to these routing functions.				     */
/*									     */
/*  This file also provides auxiliary functions related to ports.	     */
/*  "ModuleConnect" is called when setting up the port connections.	     */
/*  "checkQ" determines if a given port is full				     */
/*  "checkQEmp" determines if a given port has any requests		     */
/*  "rmQ" removes the first REQ in a port and returns it.		     */
/*  "peekQ" returns the first REQ in a port without removing it.	     */
/*  "peekQ" is used in conjunction with "commit_req" which goes on to	     */
/*  remove the queue from the specified port.				     */
/*  "addQ" adds a new REQ at the tail of a port, while "addQ_head"	     */
/*  adds a new "REQ" to the tail of a port.				     */
/*									     */
/*  Adding or removing a request from a port may cause a module to be	     */
/*  woken up. When this happens based on an add, a "wakeup" function is	     */
/*  called; when this occurs because of a removal, a "handshake"	     */
/*  function is called. The bus and directory modules, as well as the	     */
/*  send part of the network interface, have wakeup functions.  Only the     */
/*  receive part of the network interface uses a handshake.		     */
/*									     */
/*  "new_add_req", "add_req", and "add_req_head" are wrappers around	     */
/*  "addQ" and "addQ_head" that may additionally call the wakeup	     */
/*  function for the module on the other side of the output port. ("rmQ"     */
/*  itself includes a handshake call when needed).			     */
/*									     */
/*  Finally, this file also provides functions that are used by modules	     */
/*  to bring in messages from their input ports.			     */
/*****************************************************************************/





/****************************************************************************/
/* Routing functions: These functions select the port on which to send an   */
/* outbound message based on the REQ type and possibly the system           */
/* configuration                                                            */
/****************************************************************************/

/* CPU routing function */
int routing1(mptr, req)		/* Route requests from one module to the next */
SMMODULE *mptr;			/* Pointer to module from which request is routed */
REQ *req;			/* Pointer to request being routed */

/* Used by CPU */
{
  return 0;
}

int routing_L1(mptr, req)		/* Route requests from one module to the next */
SMMODULE *mptr;			/* Pointer to module from which request is routed */
REQ *req;			/* Pointer to request being routed */
{
  switch (req->s.type)
    {
    case REQUEST:
      return 1;
    case REPLY:
      return 0;
    case COHE_REPLY:
      return 2;
    default:
      YS__errmsg("Unexpected COHE in L1\n");
      return -1;
    }

}


int routing_WB(mptr, req)		/* Route requests from one module to the next */
SMMODULE *mptr;			/* Pointer to module from which request is routed */
REQ *req;			/* Pointer to request being routed */

/* Used by write buffer */
{
  if (req->s.type == REQUEST)
    return 1;
  else
    return 0;
}
     
int routing_L2(mptr, req)		/* Route requests from one module to the next */
SMMODULE *mptr;			/* Pointer to module from which request is routed */
REQ *req;			/* Pointer to request being routed */
{
  switch (req->s.type)
    {
    case REQUEST:
      return 1;
    case COHE:
      return 3;
    case REPLY:
      if (req->s.reply == REPLY  && (req->forward_to == -1 || req->forward_to == mptr->node_num))
	return 0;
      /* we intentionally don't have a break -- this means that this is
	 a forward to another node or an RAR */
    case COHE_REPLY:
      return 2;
    default:
      /* how can this be? */
      YS__errmsg("default case in routing_L2");
      return -1;
    }
}



/* In order to compute the output port at the bus, the routing function
   for the bus must also account for the degree of memory interleaving
   supported. 
   
   This is supported using the FindLeaf function.  This procedure
   determines the bank of the interleaved memory the address has to
   go to -- returns a number from 0 to INTERLEAVING_FACTOR
   (max_leaf)
   */

static int FindLeaf(int node, REQ *req, int max_leaf)
{
  int rtn = req->tag & (max_leaf -1);
  StatrecUpdate(InterleavingStats[node],(double)rtn,1.0);
  return rtn;
}


int routing_bus(mptr, req)	/* Route requests from one module to the next */
SMMODULE *mptr;			/* Pointer to module from which request is routed */
REQ *req;			/* Pointer to request being routed */
{
  int return_int = -1;

  /* Note that COHE_REPLY and REPLY still have the dest node listed as
     the source node (even if they are to remote) since the
     smnet module will flip these as needed */
  switch(req->s.type){
  case REQUEST:
    /* REQUESTs can come from the Cache or from SmnetRcv, and have
       their dest_node set correctly (no flipping involved) */
    if(req->dest_node != mptr->node_num){ /* goes to SmnetSend */
      return_int = 2;
    }
    else /* have reached the dest_node, so go to memory */
      return_int = 6+2*FindLeaf(mptr->node_num,req, INTERLEAVING_FACTOR);
    break;

  case COHE:
    /* COHEs can come from the Directory or from SmnetRcv, and have
       their dest_node set correctly (no flipping involved) */
    if(req->dest_node != mptr->node_num){ /* goes to SmnetSend */
      return_int = 2;
    }
    else /* have reached the dest_node, so go to cache */
      return_int = 0;
    break;

   
  case REPLY:
    /* Replies can come from Directory, Cache, or SmnetRcv. The source
       and destination node are flipped here. */
    
    if(req->dest_node != mptr->node_num)
      YS__errmsg("Things seem to have been flipped before calling smnet!\n");
    if (req->in_port_num >= 6 || req->in_port_num <=1)
      {
	/* This came from the directory or cache, let us see src --
	   implicit flipping */
	if (req->src_node == mptr->node_num) /* in this case, go to cache */
	  return_int = 1;
	else
	  return_int = 3; /* Go to the reply queue of the Smnet */
      }
    else
      return_int = 1; /* replies from smnet can go only to the caches */
    break;
    
  case COHE_REPLY:
    /* COHE_REPLIES can come from the Cache or SmnetRcv. WRB/REPL are also
     sent as COHE_REPLIES, even though they are unsolicited. As a result,
     source and destination are not flipped for those (unlike all others). */
    if(req->req_type != WRB && req->req_type != REPL && req->dest_node != mptr->node_num)
      YS__errmsg("Things seem to have been flipped before calling smnet!\n");
    
    /* first check to see if these came from caches -- if so, they can either
       go to directory or to smnet */
    
    if (req->in_port_num <= 1)
      {
	/* This came from the caches, let us see src -- implicit flipping;
	   unless of course it's a WRB, then use dest node. */
	if ((req->req_type != WRB && req->req_type != REPL && req->src_node == mptr->node_num) ||
	    ((req->req_type == WRB || req->req_type == REPL) && req->dest_node == mptr->node_num))
	  /* in this case, the COHE_REPLY is at its final destination, so
	     go to the directory */
	  return_int = 7+2*FindLeaf(mptr->node_num,req, INTERLEAVING_FACTOR);
	else
	  return_int = 3; /* Go to the reply queue of the Smnet */
      }
      else /* replies from smnet can go only to the directory */
	return_int = 7+2*FindLeaf(mptr->node_num,req, INTERLEAVING_FACTOR);
    break;
    
  default:
    YS__errmsg("Unknown type!\n");
  }
  return return_int;
}

int routing_dir(mptr, req)		/* Route requests from one module to the next */
SMMODULE *mptr;			/* Pointer to module from which request is routed */
REQ *req;			/* Pointer to request being routed */
{
  int return_int;

  return_int = -1;
  if (req->s.type == COHE)
    return 1;
  else /* REPLY or RAR */
    return 0;
  return return_int;
}

/* routing_SMNET:

   Used for routing by the network receive modules (SmnetRcv)

   */

int routing_SMNET(SMMODULE *mptr, REQ *req)
{
  if(req->s.type == REQUEST || req->s.type == COHE)
    return 0; /* 0 is the REQUEST network port */
  else if(req->s.type == COHE_REPLY || req->s.type == REPLY)
    return 1; /* 1 is the reply network port */
  else{
    YS__errmsg("Unknown request type!\n");
    return -1;
  }
  return -1; 
}

/* routing6:

   Used for routing by the network send modules (SmnetSend)

   */

int routing6(mptr, req)
     SMMODULE *mptr;
     REQ *req;
{
  YS__errmsg("routing6 called!!\n");
  return -1;
}




/*****************************************************************************/
/* ModuleConnect: Creates a bidirectional link between two modules at        */
/* the specified ports.  It also sets the width of the data path on the      */
/* connections.  This is typically called from architecture.c when the       */
/* architecture is specified.                                                */
/*****************************************************************************/


void ModuleConnect(one, two, port_one, port_two, width)
				/* Connects two modules together */
SMMODULE *one, *two;		/* Pointers to modules connected */
int  port_one, port_two;	/* Port numbers of modules to connect*/
int  width;			/* Width of path in bytes */
{
  if (!one || !two) {
    if (!one && two)
      fprintf(simout,"Trying to connect NULL module with module \"%s\" \n",two->name);
    else if (!two && one)
      fprintf(simout,"Trying to connect NULL module with module \"%s\" \n",one->name);
    
    YS__errmsg("ModuleConnect(): Trying to connect NULL module");
  }

  if (port_one >= one->num_ports ){ /* Specified a greater port number than
				       the module supports */
    fprintf(simout,"Given index, %d, for module \"%s\", is out of range\n",port_one, one->name);
    fprintf(simout,"Number of ports specified for this module is %d \n",one->num_ports);
    YS__errmsg("ModuleConnect(): Index out of range");
  }
  if (port_two >= two->num_ports ){ /* Specified a greater port number than
				       the module supports */
    fprintf(simout,"Given index, %d, for module \"%s\", is out of range\n",port_two, two->name);
    fprintf(simout,"Number of ports specified for this module is %d \n",two->num_ports);
    YS__errmsg("ModuleConnect(): Index out of range");
  }

  if (one->in_port_ptr[port_one] != NULL) {  /* Trying to reconnect a
						connected port */
    fprintf(simout,"Given port, %d, for module \"%s\", is already connected\n",port_one, one->name);
    YS__errmsg("ModuleConnect(): Port already connected");
  }
  if (two->in_port_ptr[port_two] != NULL) {  /* Trying to reconnect a
						connected port */
    fprintf(simout,"Given port, %d, for module \"%s\", is already connected\n",port_two, two->name);
    YS__errmsg("ModuleConnect(): Port already connected");
  }

  /* Now, establish the bidirectional link. This is achieved by making
     the in_port_ptr of one the out_port_ptr of the other, and vice versa. */
  one->in_port_ptr[port_one] = two->out_port_ptr[port_two];
  two->in_port_ptr[port_two] = one->out_port_ptr[port_one];
  two->out_port_ptr[port_two]->width = width;
  one->out_port_ptr[port_one]->width = width;
}

/*****************************************************************************/
/* General queue functions: checkQ, checkQEmp, rmQ, peekQ, addQ, addQ_head   */
/* A queue consists of a fixed number of slots and 1 overflow entry (ov_req) */
/*****************************************************************************/

/*****************************************************************************/
/* checkQ: returns the number of empty slots in the queue and                */
/* if the overflow entry (ov_req) is full, it returns (-1)                   */
/* This is important in capturing contention for resources in the            */
/* network.                                                                  */
/*****************************************************************************/

int checkQ(portq)		/* Checks size of port queue */
SMPORT *portq;			/* Pointer to port */
{
  /* return value is the number of entries that can be added before going
     to the overflow request. Thus, one can still add even if the return
     value is 0; it's only if the return value is -1 that the queue is
     _completely_ full */
  
  int return_int = portq->q_sz_tot - portq->q_size;
  if (return_int == 0 && portq->ov_req)
    return_int = -1;
  return return_int;	       
}

/*****************************************************************************/
/* checkQEmp: returns the number of entries slots in the queue               */
/*****************************************************************************/

int checkQEmp(portq)		/* Checks size of port queue */
SMPORT *portq;			/* Pointer to port */
{
  int return_int = portq->q_size;
  if (return_int == 0 && portq->ov_req) /* this can only occur if */
    return_int = 1;                     /* q_sz_tot==0 */
  return return_int;	       
}

/*****************************************************************************/
/* rmQ: Remove and return first request in queue.  -- this can also push in  */
/* the ov_req (refer to checkQ() above), if there is one, into the           */
/* portq. Modules waiting for this structural hazard to get cleared can be   */
/* rescheduled immediately. This function returns NULL if the queue is empty.*/
/*****************************************************************************/

REQ *rmQ(portq)			/* Return next request from queue */
SMPORT *portq;			/* Pointer to port */
{
  REQ *req;
  ACTIVITY *sim;

  if (portq->q_size > 0) {   /* If queue has requests in it */
    req = portq->head;	     /* Remove head of the queue and update pointers */
    portq->head = req->next;
    if (portq->head == NULL)
      portq->tail = NULL;
    req->next = NULL;
    if (portq->ov_req) {     /* If there is an overflow request move it to
				regular queue  */
      if (portq->tail) {
	portq->tail->next = portq->ov_req;
	portq->tail = portq->ov_req;
	portq->tail->next = NULL;
      }
      else {
	portq->head = portq->ov_req;
	portq->tail = portq->ov_req;
	portq->ov_req->next = NULL; /* might be redundant */
      }
      
      portq->ov_req = NULL;
      sim = portq->mptr->Sim; /* check the module associated with this port */
      if (sim && (portq->mptr->state == WAIT_OUTQ_FULL ||portq->mptr->state == WAIT_QUEUE_EVENT) ) {
	/* Schedule event that was waiting on overfull queue */
	
#ifdef DEBUG_ROUTE
	if (YS__Simtime > DEBUG_TIME)
	  fprintf(simout,"waking up %s now\n", portq->mptr->name);
#endif
	portq->mptr->Sim = NULL;
	ActivitySchedTime(sim, 0.0, INDEPENDENT);
      }
    }
    else		       /* there was no ov_req, so nothing to move in */
      portq->q_size --;
    return req;
  }
  else /* the size of the queue (not counting ov_req) is zero.
	  return NULL if queue is empty , ov_req if the queue has one */
    {
      if (portq->ov_req) {	/* If there is an overflow request
				   return this entry: this also
				   indicates that queue is just a
				   latch and a wire (queue of size 0)*/
	req = portq->ov_req;
	portq->ov_req = NULL;
	req->next = NULL;
	sim = portq->mptr->Sim;
	
	/* This handles case where module using RmQ is connected to
	   amodule that went to sleep on WAIT_OUTQ_FULL. Make sure that
	   this condition is satisfied */
	if (sim && (portq->mptr->state == WAIT_OUTQ_FULL ||portq->mptr->state == WAIT_QUEUE_EVENT)) {
	  /* Schedule event that was waiting on overfull output queue */
	  
#ifdef DEBUG_ROUTE
	  if (YS__Simtime > DEBUG_TIME)
	    fprintf(simout,"waking up %s now\n", portq->mptr->name);
#endif
	  portq->mptr->Sim = NULL;
	  ActivitySchedTime(sim, 0.0, INDEPENDENT);
	}
	return req;
      }
      /* no ov_req, so queue is totally empty. Return NULL. */
      return NULL; 
    }
}

/*****************************************************************************/
/* peekQ: Return first request in queue without removing it.  No handshake   */
/* function is called here, nor any structural hazard resolved, nor any      */
/* ov_req pushed in. This can be used to check that a module can handle the  */
/* request (e.g. has enough buffers, etc.) before removing it from the queue */
/* Should be used in conjunction with commit_req                             */
/*****************************************************************************/

REQ *peekQ(portq)
SMPORT *portq;			/* Pointer to port */
{

  if (portq->q_size > 0)	/* If queue has requests in it */
    return portq->head;
  else
    return portq->ov_req;       /* in case ov_req is full */
}


/*****************************************************************************/
/* commit_req: This function is used to remove a request from an input port. */
/* Since this is called after a call to peekQ(), we are guaranteed that      */
/* there is a request to be removed.  This routine is called when a module   */
/* is confident that it will be able to process this request. This function  */
/* is also more than just a wrapper around rmQ: it also calls the handshake  */
/* function (if any) of the module that sent the REQ                         */
/*****************************************************************************/

REQ *commit_req(mptr, in_port_num)
SMMODULE *mptr;			/* Pointer to module */
int in_port_num;		/* Port number for next request */

{
  int port_num;
  SMPORT *portq;
  REQ *req;

  port_num=in_port_num;
  
  req = rmQ(mptr->in_port_ptr[port_num]);  /* call rmQ in order to get
					      the request */
  if (!req) /* should have already peeked for a request */
    YS__errmsg("commit_req(): This port queue should not be empty");
  
  portq = mptr->in_port_ptr[port_num] ; /* Reverse-resolve this input port to
					   the sender's output port */
  if (portq->mptr->handshake) /* call the handshake function if any. */
    portq->mptr->handshake(portq->mptr, req, portq->port_num); 
  
  req->in_port_num = port_num;

#ifdef DEBUG_ROUTE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(simout,"commit_req():%s; suggested port %d; request %s; from port: %d\n",
	    mptr->name, in_port_num, Req_Type[req->req_type], port_num);
#endif
  mptr->state = PROCESSING;
  
  return req;
}


/*****************************************************************************/
/* addQ: Adds the request to the tail of the queue (portq) or to the         */
/* ov_req if the queue is full.  The insertion also orders requests          */
/* according to their priority in the queue, but RSIM uses the same          */
/* priority for all accesses.                                                */
/* The caller should have already checked for space before calling this      */
/* Returns 1 if the queue has space afterward,                               */
/* and 0 if the queue is full afterward.                                     */
/*****************************************************************************/

int addQ (portq, req)		/* add a request to port  queue */
REQ *req;			/* pointer to request to add */
SMPORT *portq;			/* pointer to port */
{
  REQ *temp, *temp1;
  
  if (portq->q_size < portq->q_sz_tot) /* if queue is not full */
    { 
      req->next = NULL;
      if (portq->head) /* add to a queue that already has entries */
	{
	  if (req->priority <= portq->tail->priority)
	    {
	      /* If we are of the lowest prioirty, put it at the very end! */
	      portq->tail->next = req;
	      req->next = NULL;
	      portq->tail = req;
	    }
	  else /* must check according to priority... */
	    {
	      /* enter request in queue according to priority */
	      temp = portq->head;
	      temp1 = NULL;
	      while(temp)
		{		
		  if (req->priority > temp->priority)
		    {
		      req->next = temp;
		      if (temp1)
			temp1->next = req;
		      else
			portq->head = req;
		      break;
		    }
		  else
		    {
		      temp1 = temp;
		      temp = temp->next;
		    }
		}
	      if (!temp)
		YS__errmsg("addQ(): must have added to queue at this point");
	    }
	}
      else /* the queue is currently empty */
	{
	  portq->head = req;
	  portq->tail = req;
	}
      portq->q_size ++; 
      return 1;
    }
  else	/* if queue is full fill in overflow entry */
    {
      if (portq->ov_req)
	YS__errmsg("addQ(): Queue already overflowed;This request should not have been processed");
      else 
	portq->ov_req = req;
      return 0;
    }
}

/*****************************************************************************/
/* addQ_head: Similar to addQ, but always places this request at the head of */
/*            the queue.                                                     */
/*****************************************************************************/

int addQ_head (portq, req)	/* add a request to port  queue */
REQ *req;			/* pointer to request to add */
SMPORT *portq;			/* pointer to port */
{
  REQ *temp, *tail_prev;
  int i;

  if (portq->q_size < portq->q_sz_tot) { /* if queue is not full */
    if (portq->head) { /* queue already has entries; push in new head */
      req->next = portq->head;
      portq->head = req;
    }
    else {
      portq->head = req;
      portq->tail = req;
      req->next = NULL;
    }
    portq->q_size ++;
    return 1;			/* 1 implies queue was not full */
  }
  else {			/* if queue is full */
    if (portq->ov_req)
      {
	YS__errmsg("addQ_head(): Queue already overflowed; This request should not have been processed");
	return 0;
      }
    else {	/* move tail to overflow queue to make space for this request */
      portq->ov_req = portq->tail;
      if (portq->q_size == 1) {
	/* If q_size 1 element, head and tail are always identical */
	portq->head = req;
	portq->tail = req;
	req->next = NULL;
      }
      else if(portq->q_size == 0) {
	/* If q_size is 0 elements, head and tail are always NULL and only
	   ov_req is ever used */
	portq->ov_req = req;
	req->next = NULL;
      }
      else {
	/* Otherwise, head and tail are distinct and ov_req is separate */
	temp = portq->head;
	tail_prev = NULL;
	/* Find the request before the tail -- slow in singly-linked list */
	for (i=0; i< (portq->q_size -1); i++) { 
	  tail_prev= temp;
	  temp = temp->next;
	}
	if (tail_prev) {
	  /* Make the request before the old tail the new tail. */
	  portq->tail = tail_prev;
	  portq->tail->next = NULL;
	}
	else { /* there was no request before the old tail */
	  /* If this is the case, we must have had a q_size of 1 */
	  YS__errmsg("q_size should have been 1!");
	}
	req->next = portq->head; /* mark the old head in the next field */
	portq->head = req;  /* fill in the new head */
      }
      return 0;			/* 0 implies queue was full due to adding this request */
    }
  }
}


/*****************************************************************************/
/* new_add_req: Used by a module to insert a request in the specified        */
/* port.  If the module attached to this port is sleeping, this              */
/* function calls that module's wakeup routine.  Since RSIM uses cycle       */
/* driven simulation, this wakeup call is not necessary for modules in       */
/* the cache hierarchy (caches + write-buffer). As with addQ, the user must  */
/* make sure there is space available before calling. The function returns 0 */
/* if the queue is full afterward.                                           */
/*****************************************************************************/


int new_add_req (SMPORT *portq, REQ *req)
{
  int add;
  SMPORT *in_port;
#ifdef DEBUG_ROUTE
  if (YS__Simtime > DEBUG_TIME)
  fprintf(simout,"add_req():%s; port %d; request %s\n",
	     portq->mptr->name, portq->port_num, Req_Type[req->req_type]);
#endif
  in_port = portq->mptr->in_port_ptr[portq->port_num];
  /* since we use a cycle driven simulation of the caches,
     we need to wake up any kind of module only if it is a non cache
     or a non write-buffer module => it is a smnet module */
  if(in_port->mptr->module_type == CAC_MODULE ||
     in_port->mptr->module_type == WBUF_MODULE)
    {
      /* Dont wake up anyone, we wake up every cycle! */
      add = ADDQ;
    }
  else{
#ifdef DEBUG_ROUTE
    if(in_port->mptr->module_type != BUS_MODULE)
      {
	YS__errmsg("Unknown module type in new_add_req!\n");
      }
#endif
    add = in_port->mptr->wakeup (in_port->mptr, in_port->port_num, req);
    /* call wakeup routine of module attached to this port */
  }
  in_port->mptr->inq_empty = 0; /* The input queue is no longer empty! */
  if (add == ADDQ)
    add = addQ(portq, req);	/* add to queue */
  else
    YS__errmsg("unacceptable add value in new_add_req");

  return add;			/* add now has value returned by addQ: 0 if queue is full */
}

/*****************************************************************************/
/* add_req: Just like new_add_req, but always calls wakeup function          */
/*****************************************************************************/

int add_req (portq, req)	/* add request to the output port */
SMPORT *portq;			/* pointer to port */
REQ *req;			/* pointer to request */
{
  int add;
  SMPORT *in_port;
#ifdef DEBUG_ROUTE
  if (YS__Simtime > DEBUG_TIME)
  fprintf(simout,"add_req():%s; port %d; request %s\n",
	     portq->mptr->name, portq->port_num, Req_Type[req->req_type]);
#endif
  in_port = portq->mptr->in_port_ptr[portq->port_num];
  /* call wakeup routine of module attached to this port */
  
  add = addQ(portq, req);	/* add to queue */
  
  in_port->mptr->inq_empty = 0; /* The input queue is no longer empty! */
  if (in_port->mptr->wakeup)
    in_port->mptr->wakeup (in_port->mptr, in_port->port_num, req); 
  return add;			/* add now has value returned by addQ: 0 if queue is full */
}

/*****************************************************************************/
/* add_req_head: Just like add_req, but adds to head of queue                */
/*****************************************************************************/

int add_req_head (portq, req)	/* add request to the output port */
SMPORT *portq;			/* pointer to port */
REQ *req;			/* pointer to request */
{
  int add;
  SMPORT *in_port;
#ifdef DEBUG_ROUTE
  if (YS__Simtime > DEBUG_TIME)
  fprintf(simout,"add_req_head():%s; port %d; request %s\n",
	     portq->mptr->name, portq->port_num, Req_Type[req->req_type]);
#endif
  in_port = portq->mptr->in_port_ptr[portq->port_num];
  add = in_port->mptr->wakeup (in_port->mptr, in_port->port_num, req);
				/* call wakeup routine of module attached to this port */
  in_port->mptr->inq_empty = 0; /* The input queue is no longer empty */
  if (add != NO_ADDQ)
    add = addQ_head(portq, req); /* add to head of queue */
  else add = 1;			/* dont add to out queue */
  return add;			/* add now has value returned by addQ: 0 if queue is full */
}


/*****************************************************************************/

/*****************************************************************************/
/* wakeup: This function wakes up the modules it belongs to if the           */
/* latter is suspended waiting on a new input request/message.  This         */
/* function is currently used for SmnetSend and the directory module.        */
/*****************************************************************************/

int wakeup (mptr, port_num, req) /* a wakeup routine */
SMMODULE *mptr;			/* pointer to module called */
int port_num;			/* input port number */
REQ *req;			/* pointer to request */
{
  ACTIVITY *sim;

  if (mptr->Sim) {		/* module event is asleep */
      if (mptr->state == WAIT_INQ_EMPTY || mptr->state==WAIT_QUEUE_EVENT) { /* it waiting for input request */
#ifdef DEBUG_ROUTE
	if (YS__Simtime > DEBUG_TIME)
	fprintf(simout,"wakeup():%s; port %d; request %s from INQ_EMPTY\n",
		 mptr->name, port_num, Req_Type[req->req_type]);
#endif
	  mptr->state = PROCESSING; /* start the new module off */
	  mptr->in_port_num = port_num; /* tell the module which port to look at */
	  sim = mptr->Sim;
	  mptr->Sim = NULL; /* consider the module event active */
	  ActivitySchedTime(sim, 0.0, INDEPENDENT); /* activate the module event */
      }
      else {
	fprintf(simerr, "wakeup():%s; port %d; request %s not busy; but not waking up\n",
		 mptr->name, port_num, Req_Type[req->req_type]);
#ifdef DEBUG_ROUTE
	if (YS__Simtime > DEBUG_TIME)
	fprintf(simout,"wakeup():%s; port %d; request %s not busy; but not waking up\n",
		 mptr->name, port_num, Req_Type[req->req_type]);
#endif
      }
      
  }
  else { /* the module event is active */
#ifdef DEBUG_ROUTE
    if (YS__Simtime > DEBUG_TIME)
    fprintf(simout,"wakeup():%s; port %d; request %s from BUSY\n",
	     mptr->name, port_num, Req_Type[req->req_type]);
#endif

  }
  return ADDQ;			/* add to queue */
}

/*****************************************************************************/
/* wakeup_node_bus: wakeup function for bus                                  */
/*****************************************************************************/

int wakeup_node_bus (mptr, port_num, req)/* bus wakeup routine */
SMMODULE *mptr;			/* pointer to bus module */
int port_num;			/* input port number */
REQ *req;			/* pointer to request */

{
  ACTIVITY *sim;

  if (mptr->Sim) { /* the module event is inactive */
    if (mptr->state == WAIT_INQ_EMPTY) { /* Waiting because in queue is empty */
#ifdef DEBUG_ROUTE
	if (YS__Simtime>DEBUG_TIME)
      fprintf(simout,"wakeup_node_bus():%s; port %d; request %s from INQ_EMPTY\n",
	     mptr->name, port_num, Req_Type[req->req_type]);
#endif
      mptr->state = PROCESSING; /* start the module off */
      sim = mptr->Sim;
      mptr->Sim = NULL; /* consider the module event active */
      ActivitySchedTime(sim, 0.0, INDEPENDENT);	/* Schedule this module's event */
      mptr->in_port_num = port_num; /* tell the bus where to look for the new request */
      return ADDQ;
    }
    
    else /* the module is not waiting for inq */
      return ADDQ;		/* add to queue */
  }
  else { /* the module is active */
#ifdef DEBUG_ROUTE
	if (YS__Simtime>DEBUG_TIME)
	    fprintf(simout,"wakeup_node_bus():%s; port %d; request %s from Busy\n",
	     mptr->name, port_num, Req_Type[req->req_type]);
#endif
    }
  return ADDQ;			/* Module event busy; add to queue */
}



/*****************************************************************************/
/* wakeup_smnetrcv: wakeup function for SmnetRcv module                      */
/*****************************************************************************/

int wakeup_smnetrcv (smnetptr, type, req)
     SMNET *smnetptr;
     int type;
     REQ *req;

{
  YS__errmsg("wakeup for smnetrcv called.\n");
  return -1;
}


/*****************************************************************************/

/*****************************************************************************/
/* SmnetRcvHandshake: Handshake function used by SmnetRcv module (receive    */
/* part of network interface). This function moves the next request of the   */
/* same type to the appropriate output port.                                 */
/*****************************************************************************/

static REQ *SmnetMvreq(SMNET *smnetptr, int type);

void SmnetRcvHandshake(SMNET *smnetptr, REQ *req, int port_num)
{
  REQ *temp;
  int check_type;
  ACTIVITY *sim;
  /* This code moves the next request of the same type to the appropriate
     output port */

  if(MemsimStatOn)
    {
      req->blktime += YS__Simtime;
      TotBlkTime += req->blktime;
      req->blktime = 0.0;
      TotNumSamples ++;
    }
  
  check_type = req->s.type;
  if (check_type == REQUEST || check_type == COHE)
    {
      /* these are types that go on the REQUEST/COHE output port */
      temp = SmnetMvreq(smnetptr, REQUEST); /* move something from the network
					       interface internal buffer to
					       the external port */
      if (temp)
	{
#ifdef DEBUG_SMNET
	  if (YS__Simtime > DEBUG_TIME)
	    {
	      fprintf(simout,"RCV_HS_REQ\t%s\tMoving a request out\n",smnetptr->name);
	      fprintf(simout,"RCV\t\t%s\t%ld\t%ld\t%s\tsrc: %d\t%s\n",smnetptr->name, temp->address,
		      temp->tag, Req_Type[temp->req_type], temp->src_node, Request_st[temp->s.type]);
	    }
#endif
	  if (!add_req(smnetptr->out_port_ptr[port_num], temp)) 
	    YS__errmsg("SmentRcvHandshake(): Moving next req to queue; Queue should not be full");
	  if (smnetptr->EvntReq) /* if Smnet ReqRcvSemaWait went to sleep
				    because reqQsz was full, we need to wake
				    it up so that it can get new packets from
				    the request network */
	    {
#ifdef DEBUG_SMNET
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"RCV_HS_REQ\t%s\tWakeing up event\n",smnetptr->name);
#endif
	      sim = (ACTIVITY *)smnetptr->EvntReq; /* will be ReqRcvSemaWait */
	      smnetptr->EvntReq = NULL;
	      ActivitySchedTime(sim, 0.0, INDEPENDENT); /* reactivate event */
	      return;
	    }
	} 
    }
  else /* a REPLY type */
    {
      temp = SmnetMvreq(smnetptr, REPLY); /* move the next one out of the Smnet
					     internal buffer */
      if (temp)
	{
#ifdef DEBUG_SMNET
	  if (YS__Simtime > DEBUG_TIME)
	    {
	      fprintf(simout,"RCV_HS_REPLY\t%s\tMoving a request out\n",smnetptr->name);
	      fprintf(simout,"RCV\t\t%s\t%ld\t%ld\t%s\tsrc: %d\t%s\n",smnetptr->name, temp->address,
		     temp->tag, Req_Type[temp->req_type], temp->src_node, Request_st[temp->s.type]);
	    }
#endif
	  if (!add_req_head(smnetptr->out_port_ptr[port_num], temp)) 
	    YS__errmsg("SmentRcvHandshake(): Moving next req to queue; Queue should not be full");
	  if (smnetptr->EvntReply) /* if Smnet ReplyRcvSemaWait went to sleep
				    because reqQsz was full, we need to wake
				    it up so that it can get new packets from
				    the reply network */
	    {
#ifdef DEBUG_SMNET
	      if (YS__Simtime > DEBUG_TIME)
		fprintf(simout,"RCV_HS_REPLY\t%s\tWakeing up event\n",smnetptr->name);
#endif
	      sim = (ACTIVITY *)smnetptr->EvntReply; /* will be ReplyRcvSemaWait */
	      smnetptr->EvntReply = NULL;
	      ActivitySchedTime(sim, 0.0, INDEPENDENT); /* reactivate event */
	      return;
	    }
	}
    }
}

/*****************************************************************************/
/* SmnetMvreq: a helper function of SmnetRcvHandshake that actually brings   */
/* forth the head element of the appropriate internal buffer                 */
/*****************************************************************************/

static REQ *SmnetMvreq(SMNET *smnetptr, int type)
{
  REQ *temp;

  if (type == REQUEST)
    {
      if (smnetptr->reqQHead == NULL)
	return NULL;
      
      temp = smnetptr->reqQHead;
      smnetptr->reqQHead = smnetptr->reqQHead->next;
      smnetptr->reqQsz --;
      if (temp->next == NULL)
	smnetptr->reqQTail = NULL;
      return temp;
    }
  else /* type is REPLY, so do the same stuff on the REPLY/COHE-REPLY queue */
    {
      if (smnetptr->replyQHead == NULL)
	return NULL;
      
      temp = smnetptr->replyQHead;
      smnetptr->replyQHead = smnetptr->replyQHead->next;
      smnetptr->replyQsz --;
      if (temp->next == NULL)
	smnetptr->replyQTail = NULL;
      return temp;
    }
}

/****************************************************************************/
/* The remainder are functions to bring in a new REQ for processing         */
/****************************************************************************/


/*****************************************************************************/
/* check_all_blw: returns the index of the first non-empty port from below   */
/* the module. If none of the queues have entries, it returns (-1).          */
/*****************************************************************************/

static int check_all_blw(mptr)
SMMODULE *mptr;
{
  int i, num_blw, num_abv, blw_index;

  num_blw = mptr->num_ports - mptr->num_ports_abv;
  num_abv = mptr->num_ports_abv;

  if (num_blw > 1)
    blw_index = ((RMQ *)(mptr->rm_q))->u2.blw;
  else 
    blw_index = num_abv;	
				
  for (i=0; i< num_blw; i++) 
    {
      if( mptr->in_port_ptr[blw_index]->q_size) {
	((RMQ *)(mptr->rm_q))->u2.blw = ((blw_index +1) == mptr->num_ports ? 
					 num_abv : blw_index+1) ;
	return blw_index;
      }
      blw_index = ((blw_index +1) == mptr->num_ports ? num_abv : blw_index+1) ;
    }
  return -1;
}


/*****************************************************************************/
/* check_all_abv: returns the index of the first non-empty port from above   */
/* the module. If none of the queues have entries, it returns (-1).          */
/*****************************************************************************/

static int check_all_abv(mptr)
SMMODULE *mptr;
{
  int i, num_abv, abv_index;

  num_abv = mptr->num_ports_abv;

  if (num_abv > 1)
    abv_index = ((RMQ *)(mptr->rm_q))->u1.abv;
  else 
    abv_index = 0;	
				
  for (i=0; i< num_abv; i++) 
    {
      if( mptr->in_port_ptr[abv_index]->q_size) {
	((RMQ *)(mptr->rm_q))->u1.abv = ((abv_index +1) == num_abv ? 0 : abv_index+1) ;
	return abv_index;
      }
      abv_index = ((abv_index +1) == num_abv ? 0 : abv_index+1) ;
    }
  return -1;
}

/*****************************************************************************/
/* bus_get_next_req: Fetches the next request from the specified port.       */
/* If the port is empty it returns NULL.  If a request is removed, the       */
/* handshake routine for that specific port is called.                       */
/*****************************************************************************/

REQ *bus_get_next_req(mptr,portid)
BUS *mptr;			/* Pointer to module */
int portid;                     /* Port id hint */
{
  int port_num;
  SMPORT *portq;
  REQ *req;
  
  port_num = portid;
  req = rmQ(mptr->in_port_ptr[port_num]); /* Remove the head element from the
					     specified port_num */
  if (!req) { /* no such request available? */
      return NULL;
  }

  req->in_port_num = portid;
  
  portq = mptr->in_port_ptr[port_num] ; 
  if (portq->mptr->handshake) /* call handshake if available */
    portq->mptr->handshake(portq->mptr, req, portq->port_num); 
  req->in_port_num = port_num; 
  return req; 
}


/*****************************************************************************/
/* get_next_req_NOPR: Used by SmnetSend to get the next request to be        */
/* Prefers specified in_port_num queue, but has no other preferences         */
/*****************************************************************************/

REQ *get_next_req_NOPR(smnetptr, in_port_num)
SMNET *smnetptr;			/* Pointer to module */
int in_port_num;		/* Port number for next request */
{
  SMPORT *portq;
  REQ *req;
  int next_port = -1;
  
  if (in_port_num >= 0 && in_port_num < smnetptr->num_ports) { /* valid in_port_num? */
    if (checkQEmp(smnetptr->in_port_ptr[in_port_num])) { /* are there any entries there? */
      next_port = in_port_num; 
      if (next_port < smnetptr->num_ports_abv) 
	((RMQ *)(smnetptr->rm_q))->u1.abv = ((next_port +1) == smnetptr->num_ports_abv ? 0 : next_port+1) ;      
      else 
	((RMQ *)(smnetptr->rm_q))->u2.blw = ((next_port +1) == smnetptr->num_ports ? 0 : next_port+1) ;      
    }
    else {
      fprintf(simout,"WARNING: get_next_req_NOPR(): This port queue should not be empty: %s: port %d\n",
	     smnetptr->name, in_port_num);
      YS__warnmsg("get_next_req_NOPR(): This port queue should not be empty");
    }
  }
  if (next_port < 0) {
    if (smnetptr->prev_abv) { /* if got last message from above */
      next_port = check_all_blw((SMMODULE *)smnetptr); /* this time check
							  below first */
      if (next_port < 0)		/* if no requests below check above */
	next_port = check_all_abv((SMMODULE *)smnetptr); 
    }
    else {  /* last time got a message from below */
      next_port = check_all_abv((SMMODULE *)smnetptr);  /* this time check
							   above first */
      if (next_port < 0)		/* if no requests below check below */
	next_port = check_all_blw((SMMODULE *)smnetptr);
    }
    if (next_port < 0) { /* still no request out there */
      smnetptr->state = WAIT_INQ_EMPTY;
      return NULL;
    }
  }
  
  if (next_port < smnetptr->num_ports_abv) /* remember which side message
					      came from */
    smnetptr->prev_abv = 1;
  else
    smnetptr->prev_abv = 0;
  
  req = rmQ(smnetptr->in_port_ptr[next_port]); /* get the entry and remove
						  it from its queue */
  if (!req)
    YS__errmsg("get_next_req_NOPR(): This port queue should not be empty");
  
  portq = smnetptr->in_port_ptr[next_port] ;
  if (portq->mptr->handshake) /* call handshake function if any */
    portq->mptr->handshake(portq->mptr, req, portq->port_num); 
  
  req->in_port_num = next_port;

#ifdef DEBUG_ROUTE
  if (YS__Simtime > DEBUG_TIME)
  fprintf(simout,"get_next_req_NOPR():%s; suggested port %d; request %s; from port: %d\n",
	 smnetptr->name, in_port_num, Req_Type[req->req_type], next_port);
#endif
  smnetptr->state = PROCESSING;
  return req;
}
