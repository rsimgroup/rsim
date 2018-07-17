/*
  module.h

  All the basic units of the cache hierarchy and directory structure,
  as well as some of the interconnection system, are implemented as
  Memsys (RPPT) modules. These modules include a basic framework
  common to all the units, defined in this file. 
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


#ifndef _module_h_
#define _module_h_ 1

#include "typedefs.h"

/* Maximum number of coalescings allowed per MSHR or write buffer entry */
#define MAX_MAX_COALS 64
extern int MAX_COALS;

struct link_para {
  int linktype;
  double av_pkt_size;
  double flitsz;
  int flitd;
  int num_hops;
  int xbar_req;			
  int xbar_rep;
  cond hps_rtn;
};


struct Delays;

/* MODULE_FRAMEWORK is an item that should be the first item in every
   "derived" class (such as caches, directories, network interfaces,
   bus, etc.). */

#define MODULE_FRAMEWORK   char   *pnxt; /* Next pointer for Pools */\
  char    *pfnxt;	        /* free list pointer for pools */\
  QE      *next;		/* Pointer to the next element */\
  int     type;		        /* type field */\
  int     id;			/* System defined unique ID */\
  char    name[32];		/* User defined name */\
  int     node_num;		/* Node number where module is located */\
  int     module_type;          /* module type */\
  int     state;                /* current state of module */\
  rtfunc  routing;              /* routing function used to send out REQs */\
  void  *Delays;		/* User-defined delays structure */\
  int     num_ports;		/* Total count of ports */\
  int     num_ports_abv;	/* Number of ports above module */\
  SMPORT  **in_port_ptr;	/* input ports of module -- modules are connected by ports */\
  SMPORT  **out_port_ptr;	/* output ports of module */\
  ACTIVITY *Sim;		/* event to be activated for module */\
  char    *rm_q;		/* rm_q structure used for round-robin scheduling */\
  REQ     *req;		        /* REQ in course of being processed */\
  int     in_port_num;		/* input port from which current REQ came */\
  int netsend_port;		/* Output port to network, if any */\
  int netrcv_port;		/* Input port from network, if any */\
  cond    wakeup;		/* function to call on wakeup */\
  func    handshake;		/* handshake function -- called when REQ sent is picked up */\
  LINKPARA link_para;		/* link parameters -- unused */\
				/* STATS */\
  int     num_ref;							\
  int     num_miss;							\
  double  latency;							\
  double  start_time;							\
  int     num_lat;							\
  double  utilization;							\
  int     stat_level;                                                   \
				/* Cycle-by cycle simulator variables */\
  int     inq_empty;            /* any REQs on input ports? */\
  int     pipe_empty;           /* any REQs currently being processed by unit */




/*::::::::::::::::::::::::: Generic Module Structure  :::::::::::::::::::::::*/

struct YS__Module
{
  MODULE_FRAMEWORK
};


/*:::::::::::::::::::::: Argument passed to events of modules :::::::::::::::*/

struct YS__arg {		
  SMMODULE *mptr;
  int index;
};


/* Module types */
#define CAC_MODULE 1
#define PROC_MODULE 2
#define WBUF_MODULE 3
#define BUS_MODULE 4
#define MEM_MODULE 5
#define DIR_MODULE 6
#define LINK_MODULE 7
#define SMNET_SEND_MODULE 8
#define SMNET_RCV_MODULE 9
#define TEST_MODULE 10

/* Module states */
#define PROCESSING 0
#define WAIT_INQ_EMPTY 1
#define WAIT_OUTQ_FULL 2
#define WAIT_QUEUE_EVENT 3 /* wake up on either adding to or subtracting from queue */

#define ON 1
#define OFF 0

void ModuleConnect(); /* create a bidirectional port connection between
			 two modules */

/* statistics functions */
void StatReportAll();
void StatClearAll();
void SMModuleStatReport ();
void SMModuleStatClear ();

/* initialize the fields of the module framework */
void SMModuleInit();
void ModuleInit (SMMODULE *, int, int, rtfunc, void *, int, int);

/* possible REQ sizes */
#define SZ_WORD 0
#define SZ_DBL 1
#define SZ_BLK 2
#define SZ_OTHER 3

#define NUM_SIZES 10

#endif
