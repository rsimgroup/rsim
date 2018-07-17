/******************************************************************************

  archregnums.h:

  This file contains architecture-specific definitions relating to the
  register file -- currently contains SPARC specific definitions only

  ****************************************************************************/
/*****************************************************************************/
/* This file is part of the RSIM Simulator.                                  */
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


#ifndef _regnums_h_
#define _regnums_h_ 1

/*****************************************************************************/
/******************* SPARC architecture specific *****************************/
/*****************************************************************************/

#define ZEROREG 0                                /* hardwire register 0 to 0 */
#define REG_REGISTERS  0                                  /* starting point */
#define COND_REGISTERS  32

/* COND REGISTERS ARE LAID OUT AS FOLLOWS:
   32 -- fcc0
   33 -- fcc1
   34 -- fcc2
   35 -- fcc3
   36 -- icc
   38 -- xcc                (note: this gets renamed/mapped as the same reg as
                                   the icc, but has a different set of values)
   37,39: reserved */

#define COND_FCC(x) (COND_REGISTERS+(x))
#define COND_ICC (COND_REGISTERS+4)
#define COND_XCC (COND_REGISTERS+6)

#define STATE_REGISTERS  40                      /* starting point is Y reg */
#define STATE_Y (STATE_REGISTERS+0)
#define STATE_CCR (STATE_REGISTERS+2)     /* this can be statically converted
					     to COND_ICC -- no need to do it
					     dynamically */
#define STATE_ASI (STATE_REGISTERS+3)
#define STATE_FPRS (STATE_REGISTERS+6) /* floating point registers state reg */
#define STATE_MEMBAR (STATE_REGISTERS+15)
#define STATE_SIR (STATE_REGISTERS+15) 
#define END_OF_REGISTERS  72           /* we ignore the privileged registers */
#define PRIV_REGISTERS  65536
#define WINSTART (END_OF_REGISTERS-24) /* 48 : the 24 is for i,l,o registers */

#endif

