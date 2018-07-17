/*
  net.h

  Header file with information regarding routing, network interface,
  and ports

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


#ifndef _net_h_
#define _net_h_ 1

#include "typedefs.h"
#include "module.h"

/* declarations of routing functions used in RSIM */
int routing1(SMMODULE *,REQ *);      /* used by CPU */
int routing_L1(SMMODULE *,REQ *);    /* used by L1 cache */
int routing_WB(SMMODULE *, REQ *);   /* used by write-buffer */
int routing_L2(SMMODULE *,REQ *);    /* used by L2 cache */
int routing_bus(SMMODULE *, REQ *);  /* used by bus */
int routing_dir(SMMODULE *, REQ *);  /* used by directory */
int routing_SMNET(SMMODULE *, REQ*); /* used by receive network interface */
int routing6(SMMODULE *,REQ *);      /* for send network interface (unused) */

/* declarations of functions used by modules to bring in inputs from ports */
REQ *bus_get_next_req(BUS*, int);
REQ *wb_peek_next_req_RR(SMMODULE *,int);
REQ *commit_req(SMMODULE *,int);
REQ *get_next_req_NOPR(SMNET *,int);

REQ *rmQ(SMPORT *);
REQ *peekQ(SMPORT *);

/* declarations for functions used to put transactions on output ports */
int new_add_req (SMPORT *portq, REQ *req);
int checkQ(SMPORT *portq);
int add_req(SMPORT *portq, REQ *req);
int add_req_head(SMPORT *portq, REQ *req);

#define NO_ADDQ 0
#define ADDQ 1
#define ADDQ_HEAD 2



/* wakeup/handshake functions associated with various modules */
int wakeup();
int wakeup_node_bus(); 
int wakeup_smnetrcv();
void SmnetRcvHandshake();

/* Event functions associated with the network interfaces */
void SmnetSend();
void SmnetRcv();
void ReplySendSemaWait();
void ReqSendSemaWait();
void ReplyRcvSemaWait();
void ReqRcvSemaWait();

/* allocate and initialize a network interface data structure */
SMNET *NewSmnetSend();
SMNET *NewSmnetRcv();

/* statistics for network interaces */
void smnet_stat();
void SmnetStatReportAll();
void SmnetSendStatReport();
void SmnetRcvStatReport();
void SmnetStatClearAll();
void SmnetStatClear();

/* Function to create and initialize the mesh network used in RSIM */
void CreateMESH (int, int *, int, int, IPORT **, OPORT **, int (*)(int *,int *,int), int);



/*****************************************************************************/
/* Port data structure: queues used for connecting between various modules   */
/*****************************************************************************/

struct YS__SMPort {
  char   *pnxt;                /* Next pointer for Pools      */
  char     *pfnxt;             /* free list pointer for pools */
  int      port_num;           /* port number of this port */
  SMMODULE *mptr;              /* module associated with this port */
  int      width;              /* width of the port */
  int      q_sz_tot;           /* maximum size of queue */
  int      q_size;             /* number of elements currently in queue */
  REQ      *head;              /* head pointer of queue list */
  REQ      *tail;              /* tail pointer of queue linked list */
  REQ      *ov_req;            /* 1 overflow entry per queue */
};

/*****************************************************************************/
/*Delays data structure: specifies access and transfer times for each module */
/*****************************************************************************/

struct Delays {
  int      access_time;
  int      init_tfr_time;
  int      flit_tfr_time;
};

/* DirDelays additionally has packet creation time for COHEs sent out */
struct DirDelays {
  int      access_time;
  int      init_tfr_time;
  int      flit_tfr_time;
  int      pkt_create_time;
  int      addtl_pkt_crtime;
};


/*****************************************************************************/
/* Network interface data structure (also called Smnet) declaration          */
/*****************************************************************************/

struct YS__NetInterface { /* used for both send and receive */
  MODULE_FRAMEWORK
  IPORT   *iport_reply;		/* NETSIM IPORT for replies */
  IPORT   *iport_req;		/* NETSIM IPORT for requests */
  OPORT   *oport_reply;		/* NETSIM OPORT for replies */
  OPORT   *oport_req;		/* NETSIM OPORT for requests */
  EVENT   *EvntReply;           /* YACSIM event to handle replies */
  EVENT   *EvntReq;             /* YACSIM event to handle requests */
  int next_port;                /* used for looping through ports */

  /* internal network interface buffers */
  REQ     *reqQHead;    /* singly-linked list pointers */
  REQ     *reqQTail;
  int     reqQsz;       /* number of entries in buffer */
  REQ     *replyQHead;  /* singly-linked list pointers */
  REQ     *replyQTail;
  int     replyQsz;     /* number of entries in buffer */
  int     Q_totsz;      /* maximum sizes of internal buffers */
  double  req_flitsz;   /* flit size for REQUEST network */
  double  reply_flitsz; /* flit size for REPLY network -- should be same */
  int     prev_abv;     /* from where did previous message come */
   int nettype;         /* type of network used */
   int num_hps;         /* number of hops -- for approximate networks */
  func    prnt_rtn;     /* statistics reporting */

  /* Statistics for network interface */
  int     num_req;
  int     num_rep;
  int     num_cohe;
  int     num_cohe_rep;
  int     num_sz_ref[NUM_SIZES]; /* sizes of REQ types */

  /* Queue lengths and occupancy times of each of the internal buffers */
  STATREC *reqQLStat;
  STATREC *replyQLStat;
  STATREC *reqQTStat;
  STATREC *replyQTStat;
};


/*****************************************************************************/
/* Port sizes connecting various modules                                     */
/*****************************************************************************/
#define DEFAULT_Q_SZ 2 /* used for initializing some module ports */

/* Other port size variables are explained in the RSIM manual. */
extern int portszl1wbreq, portszwbl1rep;
extern int portszwbl2req, portszl2wbrep;
extern int portszl1l2req, portszl2l1rep;
extern int portszl2l1cohe, portszl1l2cr;
extern int portszl2busreq, portszbusl2rep;
extern int portszbusl2cohe, portszl2buscr;
extern int portszbusother, portszdir;

extern int INTERCONNECTION_WIDTH_BYTES; /* default width of ports */
extern int INTERCONNECTION_WIDTH_BYTES_DIR; /* default width of directory ports */


/*****************************************************************************/
/* Declarations for interconnection network                                  */
/*****************************************************************************/

#define REQ_NET 0
#define REPLY_NET 1
#define MSG_NET 2

extern int FlitSize;
extern int cur_index;

extern int net_index[1024];
extern int rev_index[NUM_SIZES];


/*****************************************************************************/
/* Defintions related to architectural configurations used                   */
/*****************************************************************************/

#define INTERACTIVE -1
#define INTERACTIVE_NOPRINT -2
#define UNI_ARCH 1
#define BUS_ARCH 2
#define DIR_ARCH 3
#define RC_DIR_ARCH (4)
#define RC_LOCKUP_FREE (5)
#define RC_DIR_ARCH_WB (6)

#define MESH_NET 1
#define HYP_NET  2
#define MIN_NET  3
#define APPR_NET 4
#define XBAR_NET 5


/*****************************************************************************/
/* Miscellaneous structures used in simulator                                */
/*****************************************************************************/

struct YS__Rmq { /* for round-robin scheduling of ports */
  union {
    RMQI     *head;
    int      abv;            
    int      abv_req;
  }u1;
  union {
    RMQI     *tail;
    int      blw;            
    int      blw_req;
    int      abv_rep;
  }u2;

  union {
    int      blw_rep;
  }u3;
};

struct YS__RmqItem { /* used by YS__Rmq above */
  char   *pnxt;                /* Next pointer for Pools                             */
  char     *pfnxt;
  RMQI     *next;               /* Pointer to the element after this one              */
  int      port_num;
  int      priority;
};

/*****************************************************************************/
/* Statistics and declarations used for determining  network utilization     */
/*****************************************************************************/

extern BUFFER *BufTable[3][2048];    /* table of network buffers */
extern char WhichBuf[3][2048][100];
extern int buf_index[3];
extern OPORT *OportTable[3][200];    /* table of output ports */
extern int oport_index[3];

extern double TotBlkTime;
extern int TotNumSamples;
extern STATREC *PktNumHopsHist[3];
extern STATREC *PktSzHist[3];
extern STATREC *CoheNumInvlHist;

extern STATREC *PktSzTimeTotalMean[3][NUM_SIZES];
extern STATREC *PktSzTimeNetMean[3][NUM_SIZES];
extern STATREC *PktSzTimeBlkMean[3][NUM_SIZES];

extern STATREC **PktHpsTimeTotalMean[3];
extern STATREC **PktHpsTimeNetMean[3];
extern STATREC **PktHpsTimeBlkMean[3];
 
extern STATREC *PktTOTimeTotalMean[3];
extern STATREC *PktTOTimeNetMean[3];
extern STATREC *PktTOTimeBlkMean[3];

extern STATREC *Req_stat_total1;
extern STATREC *Req_stat_cache1;
extern STATREC *Req_stat_fsend1;
extern STATREC *Req_stat_fnet1;
extern STATREC *Req_stat_frcv1;
extern STATREC *Req_stat_dir1;
extern STATREC *Req_stat_dir_leave1;
extern STATREC *Req_stat_ssend1;
extern STATREC *Req_stat_snet1;
extern STATREC *Req_stat_srcv1;
extern STATREC *Req_stat_total2;
extern STATREC *Req_stat_cache2;
extern STATREC *Req_stat_fsend2;
extern STATREC *Req_stat_fnet2;
extern STATREC *Req_stat_frcv2;
extern STATREC *Req_stat_dir2;
extern STATREC *Req_stat_w_pend2;
extern STATREC *Req_stat_w_cnt2;
extern STATREC *Req_stat_dir_leave2;
extern STATREC *Req_stat_memory2;
extern STATREC *Req_stat_ssend2;
extern STATREC *Req_stat_snet2;
extern STATREC *Req_stat_srcv2;
extern STATREC *Req_stat_rar2;
extern STATREC *Req_stat_num_rar;
extern STATREC *Req_stat_wrb2;

extern int NUM_HOPS; /* maximum number of hops traveled by packets
			in network */

#endif
