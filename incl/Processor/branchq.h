/******************************************************************************

  branchq.h
  
  Contains definitions for the "branchqelement" and MapTable structures
  and member function definitions for branchq.
  
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



#ifndef _branchq_h_
#define _branchq_h_ 1

/*************************************************************************/
/********************** MapTable class definition ************************/
/*************************************************************************/

/* The implementation of the MapTable closely models the shadow mappers in
   the MIPS R10000 processor implementation. For more details, please refer
   to the MIPS R10000 user manual */

struct MapTable
{
  int *imap;		/* integer register mappings */
  int *fmap;		/* floating point register mappings */
  int inited;		/* are the mappers initialized? */
  MapTable();		/* constructor */
  ~MapTable() {}	/* destructor  */
};

/*************************************************************************/
/******************* BranchQElement class definition *********************/
/*************************************************************************/
struct BranchQElement
{
  int tag;		/* tag of branch/delay slot */
  MapTable *specmap;	/* shadow mappers with branch */
  int done;		/* is branch done? */
  BranchQElement(int tg, MapTable *map);	/* constructor */
  ~BranchQElement() {}				/* destructor  */
};

struct state;
class instance;
class instr;

/*************************************************************************/
/********************* BranchQ function definitions **********************/
/*************************************************************************/

                                       /* The names are self explanatory */

extern int AddBranchQ(int,state *);
extern int RemoveFromBranchQ(int,state *);
extern void BadPrediction(instance *, state *);
extern void GoodPrediction(instance *, state *);
extern void HandleUnPredicted(instance *, state *);
extern int CopyBranchQ(int, state *);
extern void FlushBranchQ(int, state *);
extern int decode_branch_instruction(instr *, instance *, state *);
extern int StartCtlXfer(instance *inst, state *proc); // actually in spec.h

#endif
