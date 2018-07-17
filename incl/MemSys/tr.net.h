/*
  tr.net.h

  Network activity tracing
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


#ifndef NETTRH
#define NETTRH

#ifdef debug
#include "dbsim.h"
/************************************************************************\
*                              BUFFER tracing statements                               *
\**************************************************************************************/

#define TRACE_BUFFER_new  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Creating network buffer ");  \
      YS__SendPrbPkt(TEXTPROBE,"Buffer",YS__prbpkt); \
      sprintf(YS__prbpkt,"(id = %d, size = %d)\n", id, sz); \
      YS__SendPrbPkt(TEXTPROBE,"Buffer",YS__prbpkt); \
   }

/**************************************************************************************\
*                                MUX tracing statements                                *
\**************************************************************************************/

#define TRACE_MUX_new  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Creating network mux (id = %d, %d inputs)\n",  \
              id, fanin);  \
      YS__SendPrbPkt(TEXTPROBE,"Mux",YS__prbpkt); \
   }

/**************************************************************************************\
*                               DEMUX tracing statements                               *
\**************************************************************************************/

#define TRACE_DEMUX_new  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Creating network demux %d\n", id);  \
      YS__SendPrbPkt(TEXTPROBE,"Demux",YS__prbpkt); \
      }

/**************************************************************************************\
*                                PORT tracing statements                               *
\**************************************************************************************/

#define TRACE_IPORT_new  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Creating input port %d\n", id);  \
      YS__SendPrbPkt(TEXTPROBE,"IPort",YS__prbpkt); \
   }

#define TRACE_IPORT_send1 \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Attempting to send packet %d through network port %d\n", \
              pkt->data.seqno, port->id);  \
      YS__SendPrbPkt(TEXTPROBE,"IPort",YS__prbpkt); \
      sprintf(YS__prbpkt,"    - Source module = %d, Destination module = %d\n", \
              src,dest); \
      YS__SendPrbPkt(TEXTPROBE,"IPort",YS__prbpkt); \
   }

#define TRACE_IPORT_send2 \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Port full, send fails\n"); \
      YS__SendPrbPkt(TEXTPROBE,"IPort",YS__prbpkt); \
   }

#define TRACE_IPORT_send3 \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Port ready, packet queued in port\n"); \
      YS__SendPrbPkt(TEXTPROBE,"IPort",YS__prbpkt); \
   }

#define TRACE_OPORT_new  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Creating output port %d\n", id);  \
      YS__SendPrbPkt(TEXTPROBE,"Oport",YS__prbpkt); \
   }

#define TRACE_OPORT_available  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Checking for packet at port %d\n", \
              port->id);  \
      YS__SendPrbPkt(TEXTPROBE,"Oport",YS__prbpkt); \
      if (retval) \
         sprintf(YS__prbpkt,"    - Packet waiting\n"); \
      else \
         sprintf(YS__prbpkt,"    - No packets waiting\n"); \
      YS__SendPrbPkt(TEXTPROBE,"OPort",YS__prbpkt); \
   }

#define TRACE_OPORT_receive1  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Attempting to receive a packet from port %d\n", \
              port->id);  \
      YS__SendPrbPkt(TEXTPROBE,"OPort",YS__prbpkt); \
   }

#define TRACE_OPORT_receive2  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Packet %d received\n", pkt->data.seqno); \
      YS__SendPrbPkt(TEXTPROBE,"OPort",YS__prbpkt); \
   }

#define TRACE_OPORT_receive3  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - No packets available\n"); \
      YS__SendPrbPkt(TEXTPROBE,"OPort",YS__prbpkt); \
   }

/**************************************************************************************\
*                              NETWORK tracing statements                              *
\**************************************************************************************/

#define TRACE_NETWORK_connectbuf  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      if (dest->type == BUFFERTYPE) \
         sprintf(YS__prbpkt,"    Connecting buffer %d to buffer %d\n", \
                 source->id, dest->id);  \
      if (dest->type == MUXTYPE) \
         sprintf(YS__prbpkt,"    Connecting buffer %d to mux %d\n", \
                 source->id, dest->id);  \
      if (dest->type == DEMUXTYPE) \
         sprintf(YS__prbpkt,"    Connecting buffer %d to demux %d\n", \
                 source->id, dest->id);  \
      if (dest->type == DUPLEXMODTYPE) \
         sprintf(YS__prbpkt,"    Connecting buffer %d to duplexmod %d\n", \
                 source->id, dest->id);  \
      if (dest->type == OPORTTYPE) \
         sprintf(YS__prbpkt,"    Connecting buffer %d to output port %d\n", \
                 source->id, dest->id);  \
      YS__SendPrbPkt(TEXTPROBE,"Connection",YS__prbpkt); \
   }

#define TRACE_NETWORK_connectmux  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      if (dest->type == BUFFERTYPE) \
         sprintf(YS__prbpkt,"    Connecting mux %d to buffer %d\n", \
                 source->id, dest->id);  \
      if (dest->type == MUXTYPE) \
         sprintf(YS__prbpkt,"    Connecting mux %d to mux %d\n", \
                 source->id, dest->id);  \
      if (dest->type == DEMUXTYPE) \
         sprintf(YS__prbpkt,"    Connecting mux %d to demux %d\n", \
                 source->id, dest->id);  \
      if (dest->type == DUPLEXMODTYPE) \
         sprintf(YS__prbpkt,"    Connecting mux  %d to duplexmod %d\n", \
                 source->id, dest->id);  \
      if (dest->type == OPORTTYPE) \
         sprintf(YS__prbpkt,"    Connecting mux %d to output port %d\n", \
                 source->id, dest->id);  \
      YS__SendPrbPkt(TEXTPROBE,"Connection",YS__prbpkt); \
   }

#define TRACE_NETWORK_connectiport  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      if (dest->type == BUFFERTYPE) \
         sprintf(YS__prbpkt,"    Connecting input port %d to buffer %d\n", \
                 source->id, dest->id);  \
      if (dest->type == MUXTYPE) \
         sprintf(YS__prbpkt,"    Connecting input port %d to mux %d\n", \
                 source->id, dest->id);  \
      if (dest->type == DEMUXTYPE) \
         sprintf(YS__prbpkt,"    Connecting input port %d to demux %d\n", \
                 source->id, dest->id);  \
      if (dest->type == DUPLEXMODTYPE) \
         sprintf(YS__prbpkt,"    Connecting input port %d to duplexmod %d\n", \
                 source->id, dest->id);  \
      if (dest->type == OPORTTYPE) \
         sprintf(YS__prbpkt,"    Connecting input port %d to output port %d\n", \
                 source->id, dest->id);  \
      YS__SendPrbPkt(TEXTPROBE,"Connection",YS__prbpkt); \
   }

#define TRACE_NETWORK_connectdemux  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      if (dest->type == BUFFERTYPE) \
         sprintf(YS__prbpkt,"    Connecting output %d of demux %d to buffer %d\n", \
                 src_index,source->id, dest->id);  \
      if (dest->type == MUXTYPE) \
         sprintf(YS__prbpkt,"    Connecting output %d of demux %d to input %d of mux %d\n", \
                 src_index, source->id, dest_index, dest->id);  \
      if (dest->type == DEMUXTYPE) \
         sprintf(YS__prbpkt,"    Connecting output %d of demux %d to demux %d\n", \
                 src_index,source->id, dest->id);  \
      if (dest->type == DUPLEXMODTYPE) \
         sprintf(YS__prbpkt,"    Connecting output %d of demux %d to duplexmod %d\n", \
                 src_index,source->id, dest->id);  \
      if (dest->type == OPORTTYPE) \
         sprintf(YS__prbpkt,"    Connecting output %d of demux %d to output port %d\n",\
                 src_index,source->id, dest->id);  \
      YS__SendPrbPkt(TEXTPROBE,"Connection",YS__prbpkt); \
   }


/**************************************************************************************\
*                              PACKET tracing statements                               *
\**************************************************************************************/

#define TRACE_PACKET_new  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Creating packet %d of size %d\n", seqno, sz); \
      YS__SendPrbPkt(TEXTPROBE,"Packet",YS__prbpkt); \
   }

#define TRACE_PACKET_show  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - "); \
      YS__SendPrbPkt(TEXTPROBE,"Packet",YS__prbpkt); \
      YS__PacketStatus(pkt); \
   }

/**************************************************************************************\
*                          HEAD EVENT tracing statements                               *
\**************************************************************************************/

#define TRACE_HEAD_chkwft  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Head of packet %d waits in module %d for its tail\n", \
              pkt->data.seqno, pkt->module->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_nextmod  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Head of packet %d determining its next module\n", \
              pkt->data.seqno);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_nextbuf  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Head's next module is buffer %d\n",buf->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_spaceavail  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - That buffer has space, head free to move\n"); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_buffull  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - That buffer full head sleeps\n"); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_nextmux  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Head's next module is mux %d\n",mux->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
      sprintf(YS__prbpkt,"    - Head arbitrates at mux for time %g\n",arbdelay); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_muxfree  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt, \
              "    Mux %d free, head of packet %d delays mux transfer time %g\n",  \
              mux->id, pkt->data.seqno, muxdelay); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_muxblocked \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Mux %d in use, head of packet %d sleeps\n",  \
              mux->id, pkt->data.seqno); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_muxwakeup  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Head of packet %d wakes up from mux %d\n",  \
              pkt->data.seqno,mux->id); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_nextoport  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Head's next module is output port %d\n",oport->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_oportrdy  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Port queue has space, head delays for move time\n"); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_oportwait  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Output port queue full, head sleeps\n");  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_iportrdy  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Head of packet %d at front of input port queue %d\n", \
              pkt->data.seqno, iport->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_nextdemux1  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Head's next module is demux %d ",demux->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_nextdemux2  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"routing requires time %g\n", demuxdelay); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_demuxport  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt, \
              "    Head of packet %d will leave thru terminal %d of demux %d\n",\
              pkt->data.seqno,pkt->index,curmod->id); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_fromiport  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Head of packet %d moves from input port %d\n", \
              pkt->data.seqno, buf->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_frombuf  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Head of packet %d moves from buffer %d \n", \
              pkt->data.seqno, buf->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_tobuf  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Head of packet %d moves to buffer %d \n", \
              pkt->data.seqno,pkt->module->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_tooport  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Head of packet %d moves to output port %d \n", \
              pkt->data.seqno, pkt->module->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_athead  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Head at the front of the buffer\n");  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_notathead  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    - Head behind other packets in the buffer\n");  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_HEAD_freetomove  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Flit %s free to enter module %d\n", \
         buf->WaitingHead->name, buf->id);  \
      YS__SendPrbPkt(TEXTPROBE,buf->WaitingHead->name,YS__prbpkt); \
   }

/**************************************************************************************\
*                          TAIL EVENT tracing statements                               *
\**************************************************************************************/

#define TRACE_TAIL_nobubbles  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Tail of packet %d has caught up with its head\n", \
              pkt->data.seqno);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_TAIL_buftobuf  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt, \
              "    Tail of packet %d moves from buffer %d to buffer %d\n", \
              pkt->data.seqno, curmod->id, nxtmod->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_TAIL_buftoport  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt, \
              "    Tail of packet %d moves from buffer %d to output port %d\n", \
              pkt->data.seqno, curmod->id, nxtmod->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_TAIL_porttobuf  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt, \
              "    Tail of packet %d moves from input port %d to buffer %d\n", \
              pkt->data.seqno, curmod->id, nxtmod->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_TAIL_porttoport  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt, \
              "    Tail of packet %d moves from input port %d to output port %d\n", \
              pkt->data.seqno, curmod->id, nxtmod->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_TAIL_shift  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
         sprintf(YS__prbpkt,"    Tail of acket %d free to shift in module %d\n", \
                 pkt->data.seqno,pkt->tailbuf->id); \
         YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
      } 

#define TRACE_TAIL_move  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Tail of packet %d free to move out of module %d\n", \
              pkt->data.seqno,pkt->tailbuf->id); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_TAIL_nomove  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Tail of packet %d not free to move\n",pkt->data.seqno); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_TAIL_done  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Packet %d queued at ouput port %d\n", \
              pkt->data.seqno, oport->id); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_TAIL_wakes  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Tail of packet %d wakes up\n", \
         pkt->data.seqno); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_TAIL_tailarrives  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Tail of packet %d arrives in buffer %d ", \
         pkt->data.seqno, buf->id); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
      sprintf(YS__prbpkt,"and releases packet's head\n"); \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_TAIL_signalmux  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Packet %d signals MUX semaphore %d\n", \
              ((PACKET*)(ActivityGetArg(ME)))->data.seqno,mxptr->id);  \
      YS__SendPrbPkt(TEXTPROBE,YS__ActEvnt->name,YS__prbpkt); \
   }

#else  /*******************************************************************************/

#define TRACE_BUFFER_new  
#define TRACE_MUX_new  
#define TRACE_DEMUX_new  
#define TRACE_IPORT_new  
#define TRACE_IPORT_send1 
#define TRACE_IPORT_send2 
#define TRACE_IPORT_send3 
#define TRACE_OPORT_new  
#define TRACE_OPORT_available  
#define TRACE_OPORT_receive1  
#define TRACE_OPORT_receive2  
#define TRACE_OPORT_receive3  
#define TRACE_NETWORK_connectbuf  
#define TRACE_NETWORK_connectmux  
#define TRACE_NETWORK_connectiport  
#define TRACE_NETWORK_connectdemux 
#define TRACE_PACKET_new  
#define TRACE_PACKET_show  
#define TRACE_HEAD_chkwft  
#define TRACE_HEAD_nextmod  
#define TRACE_HEAD_nextbuf  
#define TRACE_HEAD_spaceavail  
#define TRACE_HEAD_buffull  
#define TRACE_HEAD_nextmux  
#define TRACE_HEAD_muxfree  
#define TRACE_HEAD_muxblocked 
#define TRACE_HEAD_muxwakeup  
#define TRACE_HEAD_nextoport  
#define TRACE_HEAD_oportrdy  
#define TRACE_HEAD_oportwait  
#define TRACE_HEAD_iportrdy  
#define TRACE_HEAD_nextdemux1  
#define TRACE_HEAD_nextdemux2 
#define TRACE_HEAD_demuxport  
#define TRACE_HEAD_fromiport  
#define TRACE_HEAD_frombuf  
#define TRACE_HEAD_tobuf  
#define TRACE_HEAD_tooport  
#define TRACE_HEAD_athead  
#define TRACE_HEAD_notathead  
#define TRACE_HEAD_freetomove 
#define TRACE_TAIL_nobubbles  
#define TRACE_TAIL_buftobuf  
#define TRACE_TAIL_buftoport  
#define TRACE_TAIL_porttobuf  
#define TRACE_TAIL_porttoport  
#define TRACE_TAIL_shift  
#define TRACE_TAIL_move  
#define TRACE_TAIL_nomove  
#define TRACE_TAIL_done  
#define TRACE_TAIL_wakes  
#define TRACE_TAIL_tailarrives  
#define TRACE_TAIL_signalmux  

#endif  /******************************************************************************/


#endif

