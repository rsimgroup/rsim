/*
  net.c

  Provides functions for moving flits and packets in the network,
  based on whether they have reached a buffer, multiplexor, etc.
  
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
#include "MemSys/tr.net.h"
#include <malloc.h>
#include <math.h>

/*****************************************************************************/
/* NETSIM Operations: This file contains code that implements much of        */
/* the NETSIM extension of YACSIM.  The netsim modules are: Buffers,         */
/* Multiplexers, Demultiplexers, Input Ports, Output Port, and Duplexers.    */
/* The structures for these modules are defined in the file simsys.h.        */
/* Their operations are defined in this file.                                */
/*****************************************************************************/


/* Constants used only within this file: represent the "state machine" for
   progress of a packet */
#define Head_Start              0   /* Head Event State */
#define Next_Module             1
#define Head_Move               2
#define Wait_Mux                3
#define Wakeup_Mux              4
#define Before_Head_Move        5
#define Before_Next_Module      6

#define Tail_Start              0   /* Tail Event States */
#define Tail_Shift              1
#define Tail_Move               2
#define Signal_Port             3
#define Tail_Done               4

int     YS__BufferHeadPut();
void    YS__BufferTailPut();
void    YS__BufferHeadGet();
void    YS__BufferTailGet();
void    YS__BufferNewSpace();
void    YS__HeadEvent();
void    YS__TailEvent();
void    YS__SignalMux();
int     YS__ShiftBubbles();
void    YS__DeliverPacket();
void    YS__PacketStatus();

static double cycletime  = 1.0;  /* All other times a multiple of the cycle time */
static double flitdelay  = 1.0;  /* Time to move a flit from one buffer to another */
static double demuxdelay = 0.0;  /* Routing delay for a demux                */
static double muxdelay   = 0.0;  /* Transfer delay for a mux                 */
static double arbdelay   = 0.0;  /* Arbitration delay for a mux              */

static double pktdelay = 0.0;    /* Time to move a pkt in/out of a port      */
static double delta      = 0.5;  /* Offset time between sample and alter phases*/ 
static int bufthresh     =   1;  /* Buffer threshold -- how many flits
				    of a packet must reach a buffer
				    before it can start being sent out
				    ("1" represents wormhole routing) */
static int waitontail  = NOWAIT; /* WAIT: head waits on tail; NOWAIT: it doesn't wait */ 
static STATREC *nettimestat   = NULL;
static STATREC *blktimestat   = NULL;
static STATREC *oporttimestat = NULL;
static STATREC *movetimestat  = NULL;
static STATREC *lifetimestat  = NULL;

/*****************************************************************************/
/* MUX Operations: Multiplexers control access to a shared data path         */
/* by performing arbitration whenever two or more packets want to use the    */
/* path.  Multiplexers have one output terminal and two or more input        */
/* terminals.  Packets request use of the link connected to the ouput        */
/* terminal through one of the input terminals.  The arbitration is          */
/* implemented by a semaphore which all packets wait on when they want to    */
/* use the path.  Currently the semaphore uses a FIFO queue.                 */
/*****************************************************************************/

MUX *NewMux(id,fanin)         /* Creates & returns a pointer to a new multiplexer     */
int    id;                    /* User assigned integer ID                             */
int    fanin;                 /* Number of input terminals for the mux                */
{
   MUX *mux;
   char namestr[31];

   TRACE_MUX_new;             /* Creating new mux ...                                 */

   if (fanin < 2) YS__errmsg("NewMux called with fanin less than 2");

   mux = (MUX*) malloc(sizeof(MUX));
   if (!mux) YS__errmsg("malloc failed in NewMux");

   mux->id = id;              /* ID used for debugging                                */
   mux->type = MUXTYPE;       /* Identifies module as a multiplexer                   */
   mux->fan_in = fanin;       /* Number of input terminals on the mux                 */
   sprintf(namestr,"MuxSema%d",id);
   mux->arbsema =             /* Semaphore used for arbitration for the output term.  */
      NewSemaphore(namestr,1); 
   mux->nextmodule = NULL;    /* Pointer to the next module connected to this one     */
   mux->index = 0;            /* Identifies input terminal if next module is a mux    */
   mux->muxptr = NULL;        /* Pointer to the preceeding mux, if any                */

   return mux;
}

/*****************************************************************************/
/* DEMUX Operations: Demultiplexers perform routing by moving packets        */
/* from one input terminal to one of several output terminals.  The user     */
/* must provide a pointer to a routing function when a demultiplexer is      */
/* created.  This function must take three arguments and return an           */
/* integer.  When a packet arrives at a demux, this function is called       */
/* with pointers to the IDs of the source processor that sent the packet     */
/* and the destination processor that will receive the packet, and the       */
/* interger ID of the demultiplexer itself.  The return value is the         */
/* index of the demultiplexer's output terminal through which the packet     */
/* leaves the demultiplexer.                                                 */
/*****************************************************************************/

DEMUX *NewDemux(id,fanout,routingfcn)  /* Creates & returns a pointer to a new demux  */
int    id;                             /* User assigned integer id           */
int    fanout;                         /* Number of output terminals on the demux     */
rtfunc2 routingfcn;                     /* Routing function, determines output term.   */
{
   DEMUX *demux;
   int j;

   TRACE_DEMUX_new;                    /* Creating new demux ...             */

   if (fanout < 2) YS__errmsg("NewDemux called with fanout less than 2");

   demux = (DEMUX*)malloc(sizeof(DEMUX));
   if (!demux) YS__errmsg("malloc failed in NewDemux");

   demux->id = id;                   /* ID used for routing and debugging  */
   demux->type = DEMUXTYPE;          /* Identifies module as a demultiplexer */
   demux->fan_out = fanout;          /* Numver of output terminals on the demux */
   demux->router = routingfcn;       /* Called as routingfcn(&src, &dest, current)  */
   demux->nextmodule =               /* Need an array of next module pointer */
      (MODULE**)malloc(fanout*sizeof(MODULE*));
   demux->index =                      /* Need an array of next module
					  pointers       */
      (int*)malloc(fanout*sizeof(int));
   for (j=0;j<fanout;j++) {            /* No outputs connected initially     */
      *((demux->nextmodule)+j) = NULL;
   }

   return demux;
}

/*****************************************************************************/
/* PORT Operations: Packets enter a network through input ports and          */
/* leave through output ports.  Ports are the interface between networks     */
/* and their external environment.  Ports are finite FIFO queues of          */
/* packets (unlike buffers that are queues of flits).  Ports can be used     */
/* by sending and receiving processes to synchron- ize their access to       */
/* the port.  Sending processes can sleep at a full port until space         */
/* becomes available, and receiving processes can sleep at an empty          */
/* output port until a packet arrives.  All flits of a packet must be in     */
/* an output port before the packet can be removed.  Insertion of a          */
/* packet into an input port moves all flits of the packet into the port     */
/* before any of them move into the network.                                 */
/*****************************************************************************/


IPORT *NewIPort(id,sz)              /* Creates and returns a pointer to a new iport   */
int   id;                           /* User assigned integer id for the port */
int   sz;                           /* Maximum # of packets the port can hold*/
{
   IPORT *port;
   char namestr[31];
      
   TRACE_IPORT_new;                 /* Creating new input port ...           */

   if (sz <= 0) YS__errmsg("Input port size must be greater than 0");

   port = (IPORT*)malloc(sizeof(IPORT));
   if (!port) YS__errmsg("malloc failed in NewIPort");

   port->id = id;                   /* User assigned id used for debugging   */
   port->type = IPORTTYPE;          /* Identifies this module as an iport    */
   port->nextmodule = NULL;         /* Iport not connected yet               */
   sprintf(namestr,"PortRdy%d",id);
   port->qfree = sz;                           /* Port empty to start        */
   port->portrdy = NewSemaphore(namestr,sz);   /* Semvalue = # of free pkt positions */
   /* Sending processes can wait in the PortReady semaphore until the
      port has space  */

   sprintf(namestr,"NetRdy%d",id);
   port->netrdy = NewSemaphore(namestr,1);     /* 1 when net ready for pkt from iport */
   /* A packet's head event will wait in NetReady until it can  enter the network     */

   return port;
}

SEMAPHORE *IPortSemaphore(iptr)     /* Returns a pointer to an iport's PortReady sema */
IPORT *iptr;                        /* Pointer to the iport                  */
{
   return iptr->portrdy;
}

int IPortSpace(port)                /* Returns # of free packet spaces in an iport    */
IPORT *port;                        /* Pointer to the iport                  */
{
   return port->qfree;
}

int IPortGetId(port)                 /* Returns user assigned id of an iport */
IPORT *port;                         /* Pointer to the iport                 */
{
   return port->id;
}

/*****************************************************************************/

OPORT *NewOPort(id,sz)              /* Creates and returns a pointer to a new oport   */
int    id;                          /* User assigned integer id for the port */
int sz;                             /* Maximum # of packets the port can hold*/
{
   OPORT *port;
   char namestr[31];

   TRACE_OPORT_new;                 /* Creating new output port ...          */

   if (sz <= 0) YS__errmsg("Input port size must be greater than 0");

   port = (OPORT*) malloc(sizeof(OPORT));
   if (!port) YS__errmsg("malloc failed in NewOPort");

   port->id = id;                   /* User assigned id for debugging        */
   port->type = OPORTTYPE;          /* Identifies this module as an oport    */
   port->qhead = NULL;              /* Pointers for queue of packets in the port */
   port->qtail = NULL;              /*   waiting to be received by a process */
   port->count = 0;                 /* # of packets in the queue, 0 to start */

   sprintf(namestr,"FreeSpace%d",id);
   port->freespace = NewSemaphore(namestr,sz);

   /* FreeSpace semaphore value = # of packets the port can receive
      from the network. Head events wait in this semaphore's queue
      until port space is avaialable */

   sprintf(namestr,"PacketAvail%d",id);
   port->pktavail = NewSemaphore(namestr,0);

   /* PacketAvail semaphore value = # of packets in the port.  Receiving processes    */
   /* can wait in this semaphore's queue until packets arrive at the port    */
   port->channel_busy = 0.0;	    /* for network utilization stats         */
   port->begin_util = 0.0;	    /* for network utilization stats         */
   port->time_of_last_clear = 0.0;  /* same as above                         */
   return port;
}

SEMAPHORE *OPortSemaphore(optr)      /* Returns a pointer to an oport's semaphore */
OPORT *optr;                         /* Pointer to the oport                 */
{
   return optr->pktavail;
}

int OPortPackets(port)               /* Returns # of packets available in an oport    */
OPORT *port;                         /* Pointer to the oport                 */
{
   int retval;

   retval = port->count;             /* Available packets at port            */
   TRACE_OPORT_available;            /* Checking for packet at port ...      */

   return retval;   
}

int OPortGetId(port)                 /* Returns the user assigned ID of an oport      */
OPORT *port;                         /* Pointer to the oport                 */
{
   return port->id;
}

/*****************************************************************************/
/* BUFFER Operations: Buffers are finite queues that hold flits.  They       */
/* are inplemented as FIFO queues.  NewBuffer() is the only operation on     */
/* buffers available to users.  It creates a new finite buffer of a given    */
/* size.  A threshold for the buffer must also be specified.  It sets the    */
/* number of buffer positions that must be free before a flit can enter      */
/* the buffer.                                                               */
/*                                                                           */
/* The way buffers are implemented is that the only elements that are        */
/* actually linked together in their queues are head and tail flits.  The    */
/* locations of the flits between the head and tail flits is determined      */
/* by several counts in the buffer and packet structures, rather that        */
/* putting an element in the linked list for every flit.                     */
/*****************************************************************************/



BUFFER *NewBuffer(id,sz)       /* Creates & returns a pointer to a new network buffer */
int    id;                     /* User assigned integer ID for the buffer    */
int    sz;                     /* Maximum # of flits the buffer can hold     */
{
   BUFFER *buf;

   TRACE_BUFFER_new;           /* Creating network buffer ...                */

   buf = (BUFFER*) malloc(sizeof(BUFFER));
   if (!buf) YS__errmsg("malloc failed in NewBuffer");

   buf->id = id;               /* ID used for debugging                      */
   buf->type = BUFFERTYPE;     /* Identifies this module as a buffer         */
   buf->size = sz;             /* Maximum number of flits the buffer can hold */
   buf->head = NULL;           /* Pointer to head or tail flit at head of buffer */
   buf->tail = NULL;           /* Pointer to head or tail flit at tail of buffer */
   buf->free = sz;             /* All buffer slots free to start             */
   buf->nextmodule = NULL;     /* Next module connected to buffer output terminal */
   buf->index = 0;             /* Identifies input termial if next module is a MUX */
   buf->destination = NULL;    /* Pointer to the next buffer or oport tail will enter */
   buf->muxptr= NULL;          /* Pointer to the previous MUX head passed thru */
   buf->WaitingHead = NULL;    /* Pointer to the packet waiting to enter the buffer */

   buf->channel_busy = 0.0;    /* for network utilization stats              */
   buf->begin_util = 0.0;      /* for network utilization stats              */
   buf->time_of_last_clear = 0.0;/* for network utilization stats            */
   return buf;
}

/*****************************************************************************/

void YS__BufferHeadGet(buf)              /* Removes a head flit from head of buffer   */
BUFFER *buf;                             /* Pointer to the buffer            */
{          
   PACKET *pkt;

   buf->free++;                           /* Take head out, freeing a flit slot */
   pkt = buf->head;                       /* Get pointer to head's packet    */
   buf->head = pkt->headnext;             /* Readjust the buffer's head pointer */
   pkt->headnext = NULL;                  /* Head flit no longer in a buffer */

   if (buf->head == NULL) {              /* This was the only event in the buffer */
      buf->tail = NULL;                  /* Therefore the packet's tail is in another */
   }
   else {                                /* Packet's tail in same buffer head was in  */
      pkt->tailoffset =                  /* That tail now first element in buffer */
         pkt->data.pktsize - 2;          /*    and is offset from the front  */
   }      

   /* Head event will wake it up head's sleeping tail after returning
      from HeadGet */

   YS__BufferNewSpace(buf);              /* Check for waiting head or tail   */
   return;
}

int YS__BufferHeadPut(buf,pkt) /* Appends head of a packet to a buffer         */
BUFFER *buf;                          /* Pointer to the buffer               */
PACKET *pkt;                          /* Pointer to the packet               */
{
   pkt->headbuf = (MODULE*)buf;   /* Headbuf points to the head's buffer     */
   pkt->headnext = NULL;          /* Head will be at end of buffer, nothing behind it */
   buf->free--;                   /* One fewer free flit slot in that buffer */
   buf->tailtype = HEADTYPE;      /* Last event in buffer is a head event    */

   if (buf->free+1 == buf->size)  /* Count utilization statistics */
     buf->begin_util = YS__Simtime;

   if (buf->head == NULL) {       /* Inserting head flit into an empty buffer*/
      buf->head = pkt;            /* Packet's head will be only element in the buffer */
      buf->tail = pkt;
      TRACE_PACKET_show;          /* Debugging trace                         */
      return 1;                   /* Head flit entered an empty buffer       */
   }                              /* Head is at the front of the buffer      */
   else {                         /* Buffer had at least one tail flit in it */
      buf->tail->tailnext = pkt;  /* Append the head flit to the buffer      */
      buf->tail = pkt;            /* Head is at the back of the buffer       */
      TRACE_PACKET_show;          /* Debugging trace                         */
      return 0;                   /* Head flit entered a nonempty buffer     */
   }                              /* Head flit behind other flits in the buffer */
}

/*****************************************************************************/

void YS__BufferTailGet(buf)       /* Removes a tail flit from head of buffer */
BUFFER *buf;                             /* Pointer to the buffer            */
{
   PACKET *pkt;

   buf->free++;                           /* Departing tail creates a free flit slot  */
   if (buf->free == buf->size)            /* for utilization stats           */
       buf->channel_busy += YS__Simtime - buf->begin_util;
   pkt = buf->head;                       /* Get pointer to tail's packet    */
   buf->head = pkt->tailnext;             /* New head of buffer              */
   pkt->tailnext = NULL;                  /* Tail not in a buffer now        */
   buf->destination = NULL;               /* Dest. no longer needed to route tail */
   if (buf->head == NULL) {               /* This was the only event in the buffer */
      buf->tail = NULL;                   /* Therefore the buffer is now empty */
   }
   else { /* Buffer not empty, blocked head waiting behind departing tail    */
      ActivitySchedTime((ACTIVITY *)buf->head->headev,delta,INDEPENDENT);
   }

   YS__BufferNewSpace(buf);               /* Check for waiting head or tail  */
   return;
}

void YS__BufferTailPut(buf,pkt)  /* Appends tail of a packet to a buffer */
BUFFER *buf;                            /* Pointer to the buffer             */
PACKET *pkt;                            /* Pointer to the event              */
{
   pkt->tailbuf = (MODULE*)buf;     /* Tailbuf points to the tails buffer    */
   pkt->tailnext = NULL;            /* Tail will be at buffer tail, nothing behind it */
   buf->free--;                     /* One fewer free flit slot in that buffer */
   buf->tailtype = TAILTYPE;        /* Last event in buffer is a tail event  */

   if (buf->head == NULL) {         /* Inserting tail flit into an empty queue */
      pkt->tailoffset =             /* There may be interior flits in the buffer */
         buf->size-buf->free-1;     /* Position of tail flit from head, starting at 0 */
      buf->head = pkt;              /* Since the tail flit is only flit in buffer */
      buf->tail = pkt;
   } 
   else {                           /* Packets head must be in this buffer   */
      pkt->tailoffset = -1;         /* -1 means that this tail is not first in buffer */
      pkt->headnext = pkt;          /* Append the tail flit to the buffer    */
      buf->tail = pkt;              /* Tail is now the last event in the buffer */
   }

   if (pkt->waitingfortail) {
      TRACE_TAIL_tailarrives;
      ActivitySchedTime((ACTIVITY *)pkt->headev,delta,INDEPENDENT);
      pkt->waitingfortail = 0;
   }
   TRACE_PACKET_show;               /* Debugging trace                                */
   return;
}

/*****************************************************************************/

 void YS__BufferNewSpace(buf)      /* Activates flits waiting to enter buffer   */
BUFFER *buf;                             /* Pointer to the buffer            */
{
   PACKET *pkt;                          /* Pointer to the event             */

   /* Check for a head event now ready to enter the buffer                   */

   if (buf->WaitingHead &&               /* There is a head waiting to enter */
       buf->free >= bufthresh)           /* & there is space in the buffer   */
   {
      TRACE_HEAD_freetomove;             /* Head of packet free to enter module */
      ActivitySchedTime((ACTIVITY *)buf->WaitingHead,flitdelay,INDEPENDENT);
      buf->WaitingHead = NULL;           /* Just in case it is set           */
   }

   /* Check for a sleeping tail event in a preceeding buffer that
      should wake up */

   if (buf->tail != NULL &&              /* There is a last event in bufffer */
       buf->tailtype == HEADTYPE)        /*    and it is a head              */
   {
      pkt = buf->tail;                   /* Get a pointer to that head's pkt */
      if (pkt->SleepingTail != NULL) {   /* That packts tail is sleeping     */
         TRACE_TAIL_wakes;               /* Tail of packet wakes up          */
         ActivitySchedTime((ACTIVITY *)pkt->SleepingTail, delta, INDEPENDENT);
         pkt->SleepingTail = NULL;       /* Tail not waiting anymore         */
      }
   }
}

/*****************************************************************************/
/* NETWORK Operations: The network operations are used to interconnect       */
/* the network modules to form a network, and to specify the network         */
/* delays and routing mode.  Four routing modes are implemented: Detailed    */
/* wormhole routing, approximate wormhole routing, virtual cut through       */
/* routing and store and forward routing.                                    */
/*****************************************************************************/

void NetworkConnect(source, dest, src_index, dest_index) /* Connects two modules */
MODULE *source;                    /* Pointer to the sending module          */
MODULE *dest;                      /* Pointer to the receiving module        */
int    src_index;                  /* Ouput terminal, only used for demux senders */
int    dest_index;                 /* Input terminal, only used for mux receivers */
{
   int    typ;
   BUFFER *buf;
   MUX    *mux;
   DEMUX  *demux;
   IPORT  *iport;

   typ = source->type;                /* Module type of source                        */
   switch (typ) {                 

      case BUFFERTYPE:
         TRACE_NETWORK_connectbuf;    /* Connecting buffer to ...            */
         buf = (BUFFER*)source;       /* Cast source to type BUFFER          */
         if (dest->type == MUXTYPE)   /* Destination is a MUX                */
            buf->index = dest_index;  /* Buffer must know which input terminal */
         buf->nextmodule = dest;      /* Make the connection                 */
         break;

      case MUXTYPE:
         TRACE_NETWORK_connectmux;    /* Connecting multiplexer to ...       */
         mux = (MUX*)source;          /* Cast source to type MUX             */
         if (dest->type == MUXTYPE)   /* Destination is a MUX                */
            mux->index = dest_index;  /* Mux must know which input terminal to use    */
         mux->nextmodule = dest;      /* Make the connection                 */
         break;

      case DEMUXTYPE:
         TRACE_NETWORK_connectdemux;  /* Connecting demultiplexer to ...     */
         demux = (DEMUX*)source;      /* Cast source to type DEMUX           */
         if (dest->type == MUXTYPE)   /* Destination is a MUX                */
            *((demux->index)+src_index) =          /* Index of input terminal to use  */
               dest_index;                         /*    on the next module  */
         *((demux->nextmodule)+src_index) = dest;  /* Make the connection    */
         break;
 
      case IPORTTYPE:
         TRACE_NETWORK_connectiport;  /* Connecting input port to ...        */
         iport = (IPORT*)source;      /* Cast source to type IPORT           */
         if (dest->type == MUXTYPE)   /* Destination is a MUX                */
            iport->index = dest_index;/* IPort must know which input terminal to use  */
         iport->nextmodule = dest;    /* Make the connection                 */
         break;
 
      default: YS__errmsg("error in specifying network");
   }
}

/*****************************************************************************/void NetworkSetCycleTime(x) /* Sets the cycle time. All other times a multiple of it  */
double x;                   /* Delay value: time to move flit between buffers*/
{
   cycletime = x;
   delta = cycletime/2;
}

void NetworkSetFlitDelay(i)   /* Sets the flit delay                         */
int i;                        /* Delay value: time to move flit between buffers*/
{
   flitdelay = (double)(i*cycletime);        /* Value is a multiple of cycle time  */
}
void NetworkSetMuxDelay(i)    /* Sets the MUX delay                          */
int i;                        /* Delay value: time for head flit to move thru mux */
{                                 
   muxdelay = (double)(i*cycletime);         /* Value is multiple of cycle time */
}

void NetworkSetArbDelay(i)     /* Sets the multiplexer arbitration time      */
int i;                         /* Delay value: Time to perform arbitration   */
{
   arbdelay = (double)(i*cycletime);         /* Value is multiple of cycle time*/
}  

void NetworkSetDemuxDelay(i)   /* Sets the demultplexer routing delay        */
int i;                         /* Delay value: time to perform routing       */
{
   demuxdelay = (double)(i*cycletime);         /* Value is multiple of flit delay */
}

void NetworkSetPktDelay(i)    /* Sets the packet delay                       */
int i;                        /* Delay value: time to move pkt in and out of ports */
{
   pktdelay = (double)(i*cycletime);         /* Value is a multiple of cycle time */
}

void NetworkSetThresh(i)       /* Sets buffer threshold                      */
int i;                         /* New threshold value                        */
{
   bufthresh = i;
}

void NetworkSetWFT(i)
int i;
{
   waitontail = i;
}



/*****************************************************************************/
/* PACKET Operations: Packets are the objects that move through a            */
/* network.  They are implemented as events that are created when a          */
/* packet is created, that work their way through the network, delaying      */
/* to simulate the passage of time, and then going away when the packet      */
/* reaches its destiantion.  Operations are pro- vided for the user to       */
/* create new packets, destroy received packets, send packets into the       */
/* network, receive packets from the network, extract data from a packet,    */
/* and display the status of a packet.  The events that control packet       */
/* movement in a network are where most of the work of NETSIM is done.       */
/* There is one for the head flit of a packet and one for its tail flit.     */
/* Thus only two events are required for each packet no matter how many      */
/* flits are in the packet.                                                  */
/*****************************************************************************/

PACKET *NewPacket(seqno,msgptr,sz)   /* Creates & returns a ptr to a packet  */
int     seqno;                       /* Sequence number of packet within its message  */
MESSAGE *msgptr;                     /* Pointer to the packet's message      */
int     sz;                          /* Number of flits in the packet        */
{
   PACKET *pkt;
   
   pkt = (PACKET*)YS__PoolGetObj(&YS__PktPool);


   /* Initialize user accessible data; other fields set in PacketSend()      */

   pkt->data.seqno = seqno;            /* Used for identifying packets of a message   */
   pkt->data.mesgptr = msgptr;         /* Used only if packet is part of a message    */
   pkt->data.pktsize = sz;             /* Number of flits in the packet      */
   pkt->data.num_hops = 0;	       /* Number of hops traveled */
   pkt->data.createtime = YS__Simtime; /* Time the packet was created        */
   pkt->data.blktime = 0.0;            /* Time spent blocked in the network  */
   pkt->data.oporttime = 0.0;          /* Time spent in an output port       */
   pkt->data.nettime = 0.0;            /* Time spent in the network          */
   
   TRACE_PACKET_new;                   /* Creating new packet ...            */
   return pkt;
}

/*****************************************************************************/

double PacketSend(pkt,port,src,dest)   /* Sends a packet through a network input port */
PACKET *pkt;                           /* Pointer to the packet to send      */
IPORT  *port;                          /* Pointer to the network input port  */
int    src;                            /* ID of source CPU                   */
int    dest;                           /* ID of destination CPU              */
{  
   EVENT *headevent; 
   EVENT *tailevent;
   double x; 
   char namestr[31];

   TRACE_IPORT_send1;                   /* Attempting to send packet ...     */

   if (port->qfree <= 0) {              /* Port full, not ready for a new packet      */
      TRACE_IPORT_send2;                /* - Port full, send fails           */
      return -1.0;
   }

   else {                               /* Port accepts the packet           */

      pkt->data.srccpu = src;           /* Sending cpu or port               */
      pkt->data.destcpu = dest;         /* Receiving cpu or port             */

      pkt->module = (MODULE*)port;      /* Packet in the port                */
      pkt->tailbuf = (MODULE*)port;     /* Tail in the port                  */
      pkt->headbuf = (MODULE*)port;     /* Head in the port                  */
      pkt->tailoffset = -1;             /* -1: tail flit not front element in a buf   */
      pkt->lastiport = port;            /* Remember the entry port           */
      pkt->waitingfortail = 0;
      pkt->muxptr = NULL;               /* Ptr to list of muxes head passed through   */

      port->qfree--;                    /* One fewer port packet slot        */
      if (port->qfree < SemaphoreValue(port->portrdy)) /* Portrdy can not be > qfree  */
         SemaphoreDecr(port->portrdy);
         /* Ready could be < qfree if a process, maybe this one, has
          waited at and decremented semaphore portrdy, prior to the
          activation of this routine */
                                        
      /* Create the head event */

      sprintf(namestr,"Head%d",pkt->data.seqno);
      headevent = NewEvent(namestr,YS__HeadEvent,NODELETE,0);
      ActivitySetArg((ACTIVITY *)headevent,pkt,HEADTYPE);       /* Gives head access to pkt info  */
      pkt->headev = headevent;

      /* Create the tail event */

      sprintf(namestr,"Tail%d",pkt->data.seqno);
      tailevent = NewEvent(namestr,YS__TailEvent,NODELETE,0);
      ActivitySetArg((ACTIVITY *)tailevent,pkt,TAILTYPE);       /* Gives tail access to pkt info  */
      pkt->SleepingTail = tailevent;                /* Tail waiting for head to signal*/

      /* Synchronize the head event with the network cycle time and schedule it */

      if (cycletime > 0.0) {
        x = fmod(YS__Simtime,cycletime);
        if (x != 0.0) x = cycletime - x;
      }
      else x = 0.0;

      pkt->data.nettime =  /* Measures time from head entering to tail leaving net    */
         pkt->data.nettime - (YS__Simtime + x); /* Will add later Simtime to get diff */

      x = x + (pkt->data.pktsize)*pktdelay;       /* Plus time for pkt to enter port  */
      ActivitySchedTime((ACTIVITY *)headevent,x,INDEPENDENT); /* Head wakes up when pkt in port   */
                                                  /* Ports use saf flit control       */
      TRACE_IPORT_send3;                /* - Port ready, packet queued ...   */
      return x;                         /* Returns time to move packet into buffer    */
   }                                    /* Caller can use this time to delay */
}

/*****************************************************************************/

PACKET *PacketReceive(port)    /* Receives a packet from a network output port        */
OPORT  *port;                  /* Pointer to the port                        */
{
   PACKET   *pkt = NULL;
   ACTIVITY *aptr;
   double   x;

   TRACE_OPORT_receive1;                          /* Attempting to receive a packet   */

   if ( port->count > 0) {                        /* A packet is available   */
      pkt = port->qhead;                          /* Take it out of the port's queue  */
      if (pkt == pkt->next) YS__errmsg("Packet points to itself\n");
      port->qhead = pkt->next;                 
      pkt->next = NULL;
      if (port->qhead == NULL) port->qtail = NULL;
      port->count--;                              /* Decrement number of avail. pkts  */
      if (port->count == 0)                       /* for utilization stats   */
	port->channel_busy += YS__Simtime - port->begin_util;
      TRACE_OPORT_receive2;                       /* Received packet ...     */

      if (SemaphoreValue(port->pktavail) > port->count) /* Pktavail must be <= count  */
         SemaphoreDecr(port->pktavail); 
          /* Pktavail could be < count if another process has waited
          at & decremented semaphore pktavail, but has not yet called
          packet receive */

      /* Synchronize packets removal from port  with the network cycle time  */

      if (cycletime > 0.0) {
         x = fmod(YS__Simtime,cycletime);
         if (x != 0.0) x = cycletime - x;
      }
      else x = 0.0;

      /* The following code segement is essentially a Semaphore Signal
       operation with the scheduling of a released activity delayed by
       x to synchronize with head movements and by flitdelay for time
       to move head flit into the oport queue The scheduled head wakes
       up at Head_Move */

      if (port->freespace->size == 0)  {           /* Semaphore queue is empty        */
         port->freespace->val++;                   /* Increment its value    */
      }
      else  {                                      /* Queue not empty        */
         aptr = (ACTIVITY*)YS__QueueGetHead(port->freespace);    /* Get head of queue */
         ActivitySchedTime(aptr, x +flitdelay - delta, INDEPENDENT); /* & schedule it */
      }

  
      pkt->data.oporttime =           /* Update the accumlation of oport waiting time */
         pkt->data.oporttime + YS__Simtime + x;

      x = x + (pkt->data.pktsize)*pktdelay;    /* Plus time for pkt to leave port     */

      pkt->data.recvtime = x;                  /* Time to remove packet from port     */
                                               /* Caller can use this time to delay   */
      pkt->data.nettime =                      /* Update time pkt was in the network  */
         pkt->data.nettime + YS__Simtime + x;  /* Include time to move it out of port */
   } 
   else {
      TRACE_OPORT_receive3;                    /* No packets available       */
   }

   return pkt;                                 /* Returns NULL if no packet available */
}

/*****************************************************************************/

PKTDATA *PacketGetData(pkt)   /* Returns a pointer to a packet's user accessible data */ 
PACKET *pkt;
{
   return &(pkt->data);
}

/*****************************************************************************/

void PacketFree(pkt)          /* Returns a packet to the pool of free packets*/
PACKET *pkt;
{
   /* Update those network statistics records that have been activated */

   if (nettimestat != NULL) 
      StatrecUpdate(nettimestat,pkt->data.nettime,1.0);
   if (blktimestat != NULL)
      StatrecUpdate(blktimestat,pkt->data.blktime,1.0);
   if (oporttimestat != NULL)
      StatrecUpdate(oporttimestat,pkt->data.oporttime,1.0);
   if (movetimestat != NULL)
      StatrecUpdate(movetimestat,
         pkt->data.nettime - pkt->data.blktime - pkt->data.oporttime,1.0);
   if (lifetimestat != NULL)
      StatrecUpdate(lifetimestat,YS__Simtime - pkt->data.createtime,1.0);

   YS__PoolReturnObj(&YS__PktPool, pkt);
}

/*****************************************************************************/

void YS__PacketStatus(pkt)    /* Prints the status of a packet               */
PACKET *pkt;                  /* Pointer to the packet                       */
{
   MODULE *bp;
   int    flits;
   int    i;

   bp = pkt->tailbuf;
   flits = 0;
   TracePrintTag("Packet","Packet %d status:",pkt->data.seqno);
   if (pkt->tailbuf != pkt->headbuf) {
      TracePrintTag("Packet"," %d/%d",bp->id,pkt->tailoffset+1);
      flits = pkt->tailoffset+1;
      if (bp->type == IPORTTYPE) bp = ((IPORT*)bp)->destination;
      else if (bp->type == BUFFERTYPE) bp = ((BUFFER*)bp)->destination;
      while (bp != pkt->headbuf) {
         i = ((BUFFER*)bp)->size - ((BUFFER*)bp)->free;
         TracePrintTag("Packet"," %d/%d",bp->id,i);
         flits = flits  + i;
         bp = ((BUFFER*)bp)->destination;
      }
   }
   TracePrintTag("Packet"," %d/%d\n",bp->id,pkt->data.pktsize-flits);
}

/*****************************************************************************/
/* YS__HeadEvent: Event to control the movement of head flits.  Each         */
/* time a packet is sent into a network, with PacketSend(), an event with    */
/* this body is created and scheduled.  It is responsible for routing the    */
/* packet through the network.  A pointer to the flit's packet structure     */
/* is passed to the event as its argument.  The event's argsize is set to    */
/* HEADTYPE to indicate its type                                             */
/*****************************************************************************/

 void YS__HeadEvent()       /* This event controls the movement of head flits   */
{
   BUFFER    *buf;                /* Temporary pointers for indicated module types    */
   MUX       *mux;
   DEMUX     *demux;
   PACKET    *pkt;
   IPORT     *iport;
   OPORT     *oport;

   MODULE    *curmod;             /* Points to the module the head is currently in    */
   int       curtype;             /* The type (e.g. buffer, mux, etc.) the head is in */
   int       athead = 0;          /* 1 if the head flit is at the head of the buffer  */
  
   pkt = (PACKET*)ActivityGetArg(ME);        /* Get a pointer to head's packet        */
 
   switch (EventGetState()) {                /* Resume at last point of suspension    */

      case Head_Start: {    /* This startup code is only executed once       */

         iport = (IPORT*)pkt->module;              /* Cast module to iport type       */
         if (SemaphoreValue(iport->netrdy) <= 0) { /* Network not ready to take a pkt */
            pkt->data.blktime =                    /* Update time pkt blocked in net  */
               pkt->data.blktime - YS__Simtime;    /* Later simtime added at wakeup   */
            EventReschedSema(iport->netrdy,Before_Next_Module); /* Put pkt in port    */
            return;                                             /* queue, then sleep  */
         }
         else {                                 /* The network is ready to take a pkt */
            TRACE_HEAD_iportrdy                 /* Head at front of iport queue ...   */
            SemaphoreDecr(iport->netrdy);       /* Blocking following packets*/
            goto label_Next_Module;             /* Skip the next case        */
         }
      }

      case Before_Next_Module: {  /* Arrive here if packet was blocked in the net */
         /* Event will go directly to case Next_Module if packet was
            not blocked */

         pkt->data.blktime =      /* Packet no longer blocked, so update time blocked */
            pkt->data.blktime + YS__Simtime;
      }                           /* Fall through to case Next_Module        */

      case Next_Module: label_Next_Module: { /* Finds and identifies the next module  */

      /* The event returns here whenever it is at one module and needs
      a pointer to the next module connected to this one's output
      terminal.  First check to see if WFT is on and tail and head are
      in the same buffer */

         if (waitontail == WAIT &&            /* WFT mode is active          */
             pkt->headbuf != pkt->tailbuf)    /* & head's tail has not arrived yet    */
         {
            TRACE_HEAD_chkwft;                /* Head of packet waits for its tail    */
            EventSetState(ME, Next_Module);   /* Sleep until it does         */
            pkt->waitingfortail = 1;          /* Used in BufferTailPut() to wake head */
            return;
         }

      /* The head event first determines its next module and sets
         pkt->module to it */

         TRACE_HEAD_nextmod;            /* Head of packet deteriming its next module  */

         curmod = pkt->module;               /* The module the head is currently in   */
         curtype = curmod->type;             /* The type of that module      */

         if (curtype == BUFFERTYPE)          /* Current module is a BUFFER   */
            pkt->module = ((BUFFER*)curmod)->nextmodule;         /* Cast to BUFFER    */

         else if (curtype == MUXTYPE)        /* Current module is a MUX      */
            pkt->module = ((MUX*)curmod)->nextmodule;            /* Cast to MUX       */

         else if (curtype == DEMUXTYPE) {    /* Current module is a DEMUX    */
            pkt->module =                    /* Demuxes have multiple outputs*/
               *((((DEMUX*)(curmod))->nextmodule)+(pkt->index)); /* Cast to DEMUX     */
            TRACE_HEAD_demuxport;            /* Head of packet will leave thru term i */
         }

         else if (curtype == IPORTTYPE) {       /* Current module is an IPORT*/
               pkt->module = ((IPORT*)curmod)->nextmodule;       /* Cast to IPORT     */
         }

         else YS__errmsg("Invalid current module type"); /* This should never happen  */

      /* Pkt->module now points to the next module the head will encounter   */

         if (pkt->module->type == DEMUXTYPE) {       /* Next Module is a DEMUX        */

            demux = (DEMUX*)pkt->module;             /* Cast module to type DEMUX     */
            TRACE_HEAD_nextdemux1;                   /* Head's next module is a demux */
            pkt->index =                             /* Calling routing function      */
               (demux->router)(&(pkt->data.srccpu),&(pkt->data.destcpu),demux->id);
	    pkt->data.num_hops ++; /* Increment number of hops */
            /* The source and destination are passed as pointers so
             that the user can write routing functions that modify
             these values as the packet moves through the network.  */

            TRACE_HEAD_nextdemux2;                     /* Routing requires time ...   */
            EventReschedTime(demuxdelay, Next_Module); /* Delay routing time */
            return;
         }

         else if (pkt->module->type == MUXTYPE) {     /* Next Module is a MUX*/

            mux = (MUX*)(pkt->module);                /* Cast module to type MUX  */
            TRACE_HEAD_nextmux;                       /* Head's next module is a mux  */
            EventReschedTime(arbdelay, Wait_Mux);     /* Delay for arbitration time   */
            return;
         }
	
         else if (pkt->module->type == BUFFERTYPE) { /* Next module is a BUFFER */

            buf = (BUFFER*)(pkt->module);            /* Cast module to buffer type    */
            TRACE_HEAD_nextbuf;                      /* Head's next mod is a buffer   */

            /* Set up pointer to next buffer that tail will use when
               it comes thru */

            if (pkt->headbuf->type == IPORTTYPE)       /* Currently in an iport       */
               ((IPORT*)pkt->headbuf)->destination = pkt->module;   /* Cast IPORT     */
            else if (pkt->headbuf->type == BUFFERTYPE) /* Currently in a buffer       */
               ((BUFFER*)pkt->headbuf)->destination = pkt->module;  /* Cast BUFFER    */

            /* Set up pointer to list of muxes the head passed thru to
            get from its previous buffer to this next buffer.  The
            tail can then signal their semaphores when it gets here.  */

            buf->muxptr = pkt->muxptr;             /* The packt brought the pointer   */
            pkt->muxptr = NULL;                    /*   to the last sema passed thru  */

            /* Check to see if head can move into the next buffer */

            if (buf->free >= bufthresh) {          /* Space avail in next buffer      */
               TRACE_HEAD_spaceavail;              /* That buffer has space ...       */
               EventReschedTime(flitdelay-delta,Head_Move);
               buf->WaitingHead = NULL;            /* Just in case it is set */
            }
            else {
               TRACE_HEAD_buffull;             /* That buffer full, head sleeps       */
               buf->WaitingHead = pkt->headev; /* Used to wake head when space avail  */
               pkt->data.blktime =             /* Update time pkt blocked in the net  */
                  pkt->data.blktime - YS__Simtime;    /* Later simtime added for diff */
               EventSetState(NULL, Before_Head_Move); /* Wake up in state Head_Move   */
            }
            return;
         }

         else if (pkt->module->type == OPORTTYPE) { /* Next Module is an OPORT        */

            oport = (OPORT*)(pkt->module);          /* Cast module to type OPORT      */
            TRACE_HEAD_nextoport;                   /* Head's next module is an oport */

            /* Set up pointer to next buffer that tail will use when
	       it comes thru */

            if (pkt->headbuf->type == IPORTTYPE)              /* Head is in an iport  */
               ((IPORT*)pkt->headbuf)->destination = pkt->module;
            else if (pkt->headbuf->type == BUFFERTYPE)        /* Head is in a buffer  */
              ((BUFFER*)pkt->headbuf)->destination = pkt->module;

            /* Set up pointer to list of muxes the head passed thru to
             get from its previous buffer to this next buffer.  The
             tail can then signal their semaphores when it gets here.  */

            oport->muxptr = pkt->muxptr;            /* The packt brought the prointer */
            pkt->muxptr = NULL;                     /*   to the last sema passed thru */
 
            /* Check to see if head can move into the oport */

            if (SemaphoreValue(oport->freespace) > 0) { /* Port space available       */
               TRACE_HEAD_oportrdy;                     /* Port queue has space ...   */
               SemaphoreDecr(oport->freespace);         /* 1 less space in port queue */
               EventReschedTime(flitdelay-delta, Head_Move); /* Port delay time       */
            }
            else {
               TRACE_HEAD_oportwait;                /* Output port full, head sleeps  */
               pkt->data.blktime =                  /* Update time pkt blocked in net */
                  pkt->data.blktime - YS__Simtime;  /* Add later simtime to get diff  */
               EventReschedSema(oport->freespace,Before_Head_Move); 
            }                                       /* Wait in port semaphore*/

            return;
         }

         else YS__errmsg("Invalid network module type"); /* This should never happen  */
      }

      case Wait_Mux: { /* The HeadEvent wakes up here after a mux arbitration delay   */

         mux = (MUX*)pkt->module;                /* Cast module to MUX type  */

         if (SemaphoreValue(mux->arbsema) > 0) { /* Head wins mux arbitration*/
            SemaphoreDecr(mux->arbsema);         /*    Decrement the semaphore        */

            /* Set up pointer to list of muxes the head passed thru to
             get from its previous buffer to this next buffer.  The
             tail can then signal their semaphores when it passes
             through and gets to the next buffer.  */

            mux->muxptr = pkt->muxptr;           /* Enter it in a list of semas       */
            pkt->muxptr = mux;                   /*   the tail will signal   */

            TRACE_HEAD_muxfree;                      /* Mux is free, head delays ...  */
            EventReschedTime(muxdelay, Next_Module); /* MUX trasfer delay    */
         }  /* Return to look for the next module that follows this one      */

         else {                                  /* Semaphore in use, head loses arb  */
            TRACE_HEAD_muxblocked;               /* Mux in use, head of packet sleeps */
            EventReschedSema(mux->arbsema, Wakeup_Mux); /* Wait for it to open        */
            pkt->data.blktime =                  /* Update time pkt blocked in net    */
               pkt->data.blktime - YS__Simtime;  /* Add later simtime to get diff     */
         }

         return;
      }

      case Wakeup_Mux: {   /* Head wakes up from waiting at a MUX semaphore  */
         /* Arrive here when a tail signals a mux arbitration
         semaphore in which this head event was waiting.  The
         semaphore is signalled by tail at t+delta; so the head must
         be further delayed by 1-delta to get in sync with heads */

         mux = (MUX*)pkt->module;                  /* Cast module to MUX type*/
         TRACE_HEAD_muxwakeup;                     /* Head wakes up from mux */

         /* Set up pointer to list of muxes the head passed thru to
         get from its previous buffer to this next buffer.  The tail
         can then signal their semaphores when it passes through and
         gets to the next buffer.  */

         mux->muxptr = pkt->muxptr;                /* Packet carries pointer to the   */
         pkt->muxptr = mux;                        /*   last mux it passed   */

         /* Delay for MUX transfer time + time to synchronize with heads     */
         EventReschedTime(delta+muxdelay,Next_Module);
         pkt->data.blktime =                       /* Update time pkt blocked in net  */
            pkt->data.blktime + YS__Simtime + delta; /* Move complete after delta     */
         return;
      }
      
      case Before_Head_Move: { /* Arrive here if packet was blocked in the network    */
         /* Event will go directly to case Head_Move if packet was not
            blocked */

         pkt->data.blktime =   /* Packet no longer blocked, so update time blocked    */
            pkt->data.blktime + YS__Simtime + delta - flitdelay;
            /* Reach here when move needs only delta to complete,
            hence subtract flitdelay since packet not blocked when
            flits are moving.  */
	 
      }     /* Fall through to case Head_Move                                */

      case Head_Move: {

         buf = (BUFFER*)pkt->headbuf;        /* Cast module to BUFFER type   */

         if (buf->type == IPORTTYPE) {       /* Head moving from an iport    */
            TRACE_HEAD_fromiport;            /* Head of packet moves from iport       */
            pkt->tailoffset =                /* Tail offset from iport head  */
               pkt->data.pktsize - 2;        /* -1 for head & -1 for starting at 0    */
         }
         else {                              /* Head moving from a buffer    */
            TRACE_HEAD_frombuf;              /* Head of packet moves from a buffer    */
            YS__BufferHeadGet(buf);          /* Take head out of its current buffer   */
         }

         /* Wake up sleeping tail here instead of HeadGet since it may be in an iport */

         if (pkt->SleepingTail != NULL) {    /* This heads tail is sleeping; sched it */
            TRACE_TAIL_wakes;                /* Tail of packet wakes up      */
            ActivitySchedTime((ACTIVITY *)pkt->SleepingTail, delta, INDEPENDENT);
            pkt->SleepingTail = NULL;        /* Tail not waiting anymore     */
         }

         if (pkt->module->type == OPORTTYPE) { /* This head leaves the network        */
            TRACE_HEAD_tooport;                /* Head moves to oport        */
	    if (((OPORT *)pkt->module)->count == 0)
	      ((OPORT *)pkt->module)->begin_util = YS__Simtime; /* for util stats     */
            pkt->headev = NULL;                /* Head event will be deleted */
            pkt->headbuf = pkt->module;        /* But head flit is in the oport       */
            EventSetDelFlag();                 /* Delete the head event      */
            return;
         }

         TRACE_HEAD_tobuf;                   /* Head moves to buffer ...     */
         athead =                            /* Put head in the next buffer  */
            YS__BufferHeadPut(pkt->module,pkt);
         if (athead) {                       /* Head at the front of the next buffer  */
            TRACE_HEAD_athead;               /* Head at front of buffer ...  */
            EventReschedTime(delta,Next_Module);
            return;
         }
         else {                              /* Head flit not at front of new buffer  */ 
            TRACE_HEAD_notathead;            /* Head behind other packet ... */
            pkt->data.blktime =              /* Update time pkt blocked in the net    */
               pkt->data.blktime - (YS__Simtime + delta); /* Delta completes the move */
            EventSetState(ME, Before_Next_Module);  /* Head blocked and goes to sleep */
            return;
         }
      }
   }
}

/*****************************************************************************/
/* YS__TailEvent: event to control movement of tail flits.                   */
/* Each time a packet is sent into a network with PacketSend(), an           */
/* event with this body is created and scheduled.  It is responsible         */
/* for moving the tail flit through the network following the path           */
/* established by the head flit.  It also manages the the distribution       */
/* of the flits between the head and tail flits.  A pointer to the           */
/* flit's packet structure is passed to the event as a its argument.         */
/* The event's argsize is set to TAILTYPE to indicate its type               */
/*****************************************************************************/


 void YS__TailEvent()     /* This event controls the movement of tail flits     */
{
   PACKET *pkt;
   MODULE *curmod;
   MODULE *nxtmod;
   BUFFER *buf;
   OPORT  *oport;
   int    moveflag;

   pkt = (PACKET*)ActivityGetArg(ME);      /* Get a pointer to the tail's packet      */
   
   switch(EventGetState()) {               /* Resume at last point of suspension      */
     
      case Tail_Start: label_Tail_Start:

         moveflag = YS__ShiftBubbles(pkt);    /* Adjust bubbles in packet    */

         if (moveflag == 2 &&                 /* No bubbles between tail & head flits */
             pkt->headev != NULL)             /* Head flit still in a buffer */
         {
            TRACE_TAIL_nobubbles;             /* Tail of pkt has caught up with head  */
             EventSetState(NULL, Tail_Start); /* Tail sleeps until head moves*/
             pkt->SleepingTail =              /* Pointer used by head to wake up tail */
                (EVENT*)(ActivityGetMyPtr()); /*    when it moves            */
             return;
         }

         if (moveflag == 0) {                   /* Tail doesn't move         */
            TRACE_TAIL_nomove;
            EventReschedTime(flitdelay,Tail_Start);
         }
         else if (moveflag == -1) {             /* Tail moves within its buffer       */
            TRACE_TAIL_shift;
            EventReschedTime(flitdelay-delta,Tail_Shift);
         }
         else if (moveflag == 1) {              /* Tail moves between buffers*/
            TRACE_TAIL_move;
            EventReschedTime(flitdelay-delta,Tail_Move);
         }
         else YS__errmsg("Incorrect return value from ShiftBubbles");

         return;

      case Tail_Shift:  

         curmod = pkt->tailbuf;                /* Module that tail is in     */
         if (curmod->type == BUFFERTYPE) {     /* Tail in a buffer           */
            buf = (BUFFER*)curmod;             /* Cast to type BUFFER        */
            buf->free++;                       /* One more free flit slot in buf      */
            YS__BufferNewSpace(buf);           /* Chk for waiting head or tail        */
         }
         EventReschedTime(delta,Tail_Start);
         return;
         
      case Tail_Move:

         curmod = pkt->tailbuf;                   /* Module that tail is in  */
         if (curmod->type == IPORTTYPE)           /* Tail in an input port   */
            nxtmod = ((IPORT*)pkt->tailbuf)->destination;  /* Cast module to IPORT    */
         else                                     /* Tail in a buffer        */
            nxtmod = ((BUFFER*)pkt->tailbuf)->destination; /* Cast module to BUF      */

         /* Nxtmod now points to the next buffer or oport the tail
            will encounter */
         if (curmod->type == BUFFERTYPE && nxtmod->type == BUFFERTYPE) {
            TRACE_TAIL_buftobuf;             /* Tail moves from buf to buf   */
            YS__BufferTailGet(curmod);       /* Take tail out of its buffer  */
            YS__BufferTailPut(nxtmod,pkt);   /* & put it in the next buffer  */
            YS__SignalMux(nxtmod);           /* Tail passed, free muxes      */
            EventReschedTime(delta,Tail_Start);
            return;                          /* Delay for flit delay         */
         }

         if (curmod->type == BUFFERTYPE && nxtmod->type == OPORTTYPE) {
            TRACE_TAIL_buftoport;            /* Tail moves from buf to oport */
            YS__BufferTailGet(curmod);       /* Take tail out of its buffer  */
            YS__SignalMux(nxtmod);           /* Tail passed, free muxes      */
            pkt->tailbuf = nxtmod;           /* Tail in an oport now         */
            EventReschedTime(delta,Tail_Done);
            return;                          /* Delay for flit delay         */
         }

         if (curmod->type == IPORTTYPE && nxtmod->type == BUFFERTYPE) {

            if (pkt->data.mesgptr) {            /* This packet associated with a mesg */
               pkt->data.mesgptr->pktosend--;   /* One more packet of message sent    */
               if ( pkt->data.mesgptr->pktosend == 0 &&  /* This was the last packet  */
                  pkt->data.mesgptr->blockflag == BLOCK_UNTIL_SENT) /* Sender blocked */
                     ActivitySchedTime(pkt->data.mesgptr->source,   /* Wake up sender */
                                       delta,INDEPENDENT);  
            }

            TRACE_TAIL_porttobuf;            /* Tail moves from iport to buf */
            YS__BufferTailPut(nxtmod,pkt);   /* Put tail in next buffer      */
            YS__SignalMux(nxtmod);           /* Tail passed, free muxes      */
            EventReschedTime(delta, Signal_Port);
            pkt->lastiport->qfree++;         /* One fewer packet in the port */
            return;
         }

         if (curmod->type == IPORTTYPE && nxtmod->type == OPORTTYPE) {

            if (pkt->data.mesgptr) {            /* This packet associated with a mesg */
               pkt->data.mesgptr->pktosend--;   /* One more packet of message sent    */
               if ( pkt->data.mesgptr->pktosend == 0 &&  /* This was the last packet  */
                  pkt->data.mesgptr->blockflag == BLOCK_UNTIL_SENT) /* Sender blocked */
                     ActivitySchedTime(pkt->data.mesgptr->source,   /* Wake up sender */
                                       delta,INDEPENDENT);  
            }

            TRACE_TAIL_porttoport;           /* Tail moves from iport to oport        */
            YS__SignalMux(nxtmod);           /* Tail passed, free muxes      */
            pkt->tailbuf = nxtmod;           /* Tail in an oport now         */
            EventReschedTime(delta,Tail_Done);
            return;
         }

      case Signal_Port: /* Tail leaves an iport & enters a buffer            */

         SemaphoreSignal(pkt->lastiport->netrdy);   /* Net ready for more pkts        */
         SemaphoreSignal(pkt->lastiport->portrdy);  /* One more free packet position  */
         pkt->lastiport = NULL;                     /* Tail no longer in an iport     */

         goto label_Tail_Start;
     
      case Tail_Done: /* Tail enters oport and leaves the network            */

         if (pkt->lastiport != NULL) {    /* Tail in an iport                */
            pkt->lastiport->qfree++;      /* One fewer packet in the port    */
            SemaphoreSignal(pkt->lastiport->netrdy);  /* Net ready for more packets   */
            SemaphoreSignal(pkt->lastiport->portrdy); /* One more free pkt position   */
         }         
 
         oport = (OPORT*)(pkt->tailbuf);   /* Get a pointer to the oport     */
	 
         if (oport->qhead == NULL) {       /* Insert packet input an empty port queue */
            oport->qhead = pkt;
            oport->qtail = pkt;
	    pkt->next = NULL;              /* was missing in original netsim release  */
         }
         else {                            /* Append to tail of a nonempty port queue */
            oport->qtail->next = pkt;
            oport->qtail = pkt;
            pkt->next = NULL;
         }
         TRACE_TAIL_done;                  /* Packet queued at output port   */
         oport->count++;                   /* count = # of packets available in port  */

         pkt->data.oporttime =             /* Packet enters the output port, so start */
            pkt->data.oporttime - YS__Simtime; /* counting time in oport     */

         SemaphoreSignal(oport->pktavail); /* Releases  activities waiting on packets */

         EventSetDelFlag();                /* Tail event dies                */
         return;
   }
}

/****************************************************************************/

 void YS__SignalMux(modptr)          /* Signals muxes that tail passes through  */
MODULE *modptr;                            /* Ptr to buffer or oport containing head  */
{
   MUX *mxptr;
   MUX *mxtemp;

   if (modptr->type == BUFFERTYPE) {       /* Module is a buffer             */
      mxptr = ((BUFFER*)(modptr))->muxptr; /* Cast module pointer and get pointer to  */
      ((BUFFER*)(modptr))->muxptr = NULL;  /*    preceeding mux, if any      */
   }

   else  {                                 /* Module is an oport             */
      mxptr = ((OPORT*)(modptr))->muxptr;  /* Cast module pointer and get pointer to  */
      ((OPORT*)(modptr))->muxptr = NULL;   /*    preceeding mux, if any      */
   }   
   while (mxptr != NULL) {                 /* Follow list of preceeding muxes*/
      TRACE_TAIL_signalmux;                /* Packet signals mux semaphore   */
      SemaphoreSignal(mxptr->arbsema);     /* Signal this mux                */
      mxtemp = mxptr;                      /* Remember this mux              */
      mxptr = mxptr->muxptr;               /* Get the next mux in the list   */
      mxtemp->muxptr = NULL;               /* Clear the previous mux pointer */
   }
}

/*****************************************************************************/

 int YS__ShiftBubbles(pkt)  /* Manages flits between a packet's the head & tail */
PACKET *pkt;                      /* Pointer to the packet                   */
{
   MODULE *curmod;                /* Pointer to the mocule the current       */
   MODULE *nxtmod;                /* Pointer to the module that follows it   */
   BUFFER *buf;                   /* Curmod cast to type buffer              */
   int moveflit;                  /* Flag to indicate a flit is to switch buffers     */
   int nextfree;                  /* # of free slots in the next buffer      */
   int retval;                    /* -1: tail shifts, 1: tail moves, 0: tail ~ move   */
   int zbub = 1;                  /* Bubble count was zero when routine started       */

   if (pkt->headbuf == pkt->tailbuf)  /* Head and tail in same buffer,       */
      return 2;                       /* no bubbles between them to shift    */

   /* Since head and tail are in different buffers, there can be no
   other head or flits ahead of the tail in its buffer.  There may be
   internal flits ahead of it */

   curmod = pkt->tailbuf;             /* Get a pointer to the tail flits buffer       */
   buf = (BUFFER*)curmod;             /* Cast it to type BUFFER              */

   if (pkt->tailbuf->type == IPORTTYPE)              /* Tail still in an iport        */
      nxtmod = ((IPORT*)pkt->tailbuf)->destination;  /* Get ptr to the next module    */
   else                                              /* Tail is in a buffer  */
      nxtmod = ((BUFFER*)pkt->tailbuf)->destination; /* Get ptr to the next module    */

   if (nxtmod->type == BUFFERTYPE)                   /* Next module is a buffer       */
      nextfree = ((BUFFER*)nxtmod)->free;            /* # free slots in next buffer   */
   else                                              /* Next module is an oport       */
      nextfree = -1;                                 /* Oports can hold a whole pkt   */
   
   if (nextfree != 0) {          /* There is space for in the next buffer    */
      if (pkt->tailoffset > 0) { /* There is an internal flit ahead of tail  */
         retval = -1;            /* Tail shifts within its buffer            */
         zbub = 0;               /* Bubble count was not zero at start       */
         moveflit = 1;           /* Tells the next buffer that a flit has moved in    */
         pkt->tailoffset--;      /* All flits shift, moving tail closer to front      */
      }
      else {                     /* Tail at front of its buffer, no flits ahead of it */
         retval = 1;             /* Tail moves between buffers               */
         zbub = 0;               /* Bubble count was not zero at start       */
         moveflit = 0;           /* Notify the next buffer not to expect a flit       */
      }
   }                     
   else {                        /* No flits can move out of this buffer     */
      retval = 0;                /* Tail doesn't move                        */
      moveflit = 0;              /* Notify the next buffer not to expect a flit       */
   }                             /*   because flit will be moved later by TailPut     */

   curmod = nxtmod;            /* Look at the next buffer between head and tail       */
   buf = (BUFFER*)curmod;      /* Cast it to type BUFFER                     */

   while (curmod != pkt->headbuf) {  /* Follow list of buffers between head & tail    */

      nxtmod = ((BUFFER*)curmod)->destination; /* Get a ptr to the next module        */
      if (nxtmod->type == BUFFERTYPE)          /* Next module is a buffer    */
         nextfree = ((BUFFER*)nxtmod)->free;   /* # free slots in next buffer*/
      else                                     /* Nest module is an oport    */
         nextfree = -1;                        /* Oports can hold a whole pkt*/
      buf = (BUFFER*)curmod;                   /* Cast module to type BUFFER */

      if (buf->free < buf->size &&      /* There is a flit in this buffer    */
          nextfree != 0)                /*    & space for it in the next buffer       */
      {
         zbub = 0;                      /* Bubble count was not zero at start*/
         if (!moveflit) {               /* No flit moved in from previous buffer      */
            buf->free++;                /* Therefore one free slot created   */
         }                              
         /* Else a flit moves in and one moves out leaving free and bubbles unchanged */
         moveflit = 1;                  /* Tells next buffer that it got a flit       */
      }  
      else {                            /* No flits can move out of this buffer       */
         if (moveflit) {                /* But one moved in                  */
            zbub = 0;                   /* Bubble count was not zero at start*/
            buf->free--;                /* Therefore, one fewer flit slot    */
         }
         moveflit = 0;                  /* Notify next buffer not to expect a flit    */
      }
      curmod = nxtmod;                  /* Get a pointer to the next module & repeat  */
      buf = (BUFFER*)curmod;            /* Cast it to type BUFFER            */
   }

   /* We have reached the head flit's buffer */

   if (curmod->type != OPORTTYPE) {     /* Head's buffer is really a buffer  */
      if (moveflit) {                   /*   and a flit moved into it        */
         buf->free--;                   /* Therefore, one fewer free flit slot        */
      }
   }
   if (zbub) return 2;                  /* Tail has caught up with its head  */
   else return retval;          
}

/*****************************************************************************/
/* Statistics Operations: The following statistics can be                    */
/* automatically collected on the states of the packets as they move         */
/* through a network:                                                        */
/*                                                                           */
/*      - Total time spent in the network (packet latency)                   */
/*      - Time blocked by multiplexers or full buffers                       */
/*      - Time in an output port waiting to be received                      */
/*      - Total time that at least one flit was moving in the network        */
/*                                                                           */
/* All of theses statistics are collected in YACSIM statistics records,      */
/* and therefore, all statrec statistics such as means and histograms are    */
/* avaiable.                                                                 */
/*****************************************************************************/

void NetworkCollectStats(type,histflg,nbin,low,high)
                /* Activates automatic statistics collection for the network */
int type;       /* NETTIME, BLKTIME, OPORTTIME, MOVETIME                     */
int histflg;    /* HIST or NOHIST                                            */
int nbin;       /* Number of histogram bins                                  */
double low;     /* Max value of the low bin                                  */
double high;    /* Min value of the high bin                                 */
{
   if (type == NETTIME) {           /* Collect total time in net stats       */
      if (nettimestat == NULL) {    /* Stat collection not yet inititated    */
         nettimestat =              /* Get a new statrec                     */
            NewStatrec("NetTimeStats",POINT,MEANS,histflg,nbin,low,high);
      }
      else YS__warnmsg("Time in net statistics collection already set");
   }
   else if (type == BLKTIME) {      /* Collect time packet blocked stats     */
      if (blktimestat == NULL) {    /* Stat collection not yet inititated    */
         blktimestat =              /* Get a new statrec                     */
            NewStatrec("BlkTimeStats",POINT,MEANS,histflg,nbin,low,high);
      }
      else YS__warnmsg("Time blocked statistics collection already set");
   }
   else if (type == OPORTTIME)   {  /* Collect time waiting in oport stats   */
      if (oporttimestat == NULL) {  /* Stat collection not yet inititated    */
         oporttimestat =            /* Get a new statrec                     */
            NewStatrec("OPortTimeStats",POINT,MEANS,histflg,nbin,low,high);
      }
      else YS__warnmsg("Time in net statistics collection already set");
   }
   else if (type == MOVETIME) {     /* Collect time packet in motion stats   */
      if (movetimestat == NULL) {   /* Stat collection not yet inititated    */
         movetimestat =             /* Get a new statrec                     */
            NewStatrec("MoveTimeStats",POINT,MEANS,histflg,nbin,low,high);
      }
      else YS__warnmsg("Time packet moving statistics collection already set");
   }
   else if (type == LIFETIME) {     /* Collect packet lifetime stats         */
      if (lifetimestat == NULL) {   /* Stat collection not yet inititated    */
         lifetimestat =             /* Get a new statrec                     */
            NewStatrec("MoveTimeStats",POINT,MEANS,histflg,nbin,low,high);
      }
      else YS__warnmsg("Time packet moving statistics collection already set");
   }
   else YS__errmsg("Invalid statistic type for networks, can't collect");
}

/*****************************************************************************/

void NetworkResetStats()        /* Resets all network statistics records     */
{
   if (nettimestat != NULL)     /* Collecting time in net statistics         */
      StatrecReset(nettimestat);
   if (blktimestat != NULL)     /* Collecting time blocked statistics        */
      StatrecReset(blktimestat);
   if (oporttimestat != NULL)   /* Collecting waiting in port statistics     */
      StatrecReset(oporttimestat);
   if (movetimestat != NULL)     /* Collecting time moving statistics        */
      StatrecReset(movetimestat);
   if (lifetimestat != NULL)     /* Collecting packet lifetime statistics    */
      StatrecReset(lifetimestat);
}

/*****************************************************************************/

STATREC *NetworkStatPtr(type)    /* Returns a pointer to a network statistics record  */
int type;                        /* NETTIME, BLKTIME, OPORTTIME, MOVETIME    */
{
   if (type == NETTIME) return nettimestat;
   else if (type == BLKTIME) return blktimestat;
   else if (type == OPORTTIME) return oporttimestat;
   else if (type == MOVETIME) return movetimestat;
   else if (type == LIFETIME) return lifetimestat;
   else YS__errmsg("Invalid statistic type for networks");
   return NULL;
}

/*****************************************************************************/

void NetworkStatRept()        /* Prints a report of network statistics       */
{
   if (nettimestat != NULL) {
      TracePrintTag("statrpt",
         "    Network throughput = %g\n",StatrecRate(nettimestat));
      TracePrintTag("statrpt",
         "    Average packet latency = %g\n",StatrecMean(nettimestat));
   }
   if (movetimestat != NULL) {
      TracePrintTag("statrpt","    Ideal latencey (no conflicts) = %g\n",
             StatrecMean(movetimestat));
   }
   if (movetimestat != NULL && nettimestat != NULL)
      TracePrintTag("statrpt","    Normalized latency = %g\n",
             StatrecMean(movetimestat)/StatrecMean(nettimestat));
   if (blktimestat != NULL) {
      TracePrintTag("statrpt","    Average time packets blocked in the network = %g\n",
             StatrecMean(blktimestat));
   }
   if (oporttimestat != NULL) {
      TracePrintTag("statrpt","    Average time packets wait to be received = %g\n",
             StatrecMean(oporttimestat));
   }
   if (lifetimestat != NULL) {
      TracePrintTag("statrpt","    Average packet lifetime = %g\n",
             StatrecMean(lifetimestat));
   }
   TracePrintTag("statrpt","\n");
}

