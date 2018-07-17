/*****************************************************************************/
/*                                                                           */
/* decoding.h :   Function declarations for various operations               */
/*                                                                           */
/*****************************************************************************/
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


#ifndef _decoding_h_
#define _decoding_h_ 1


/* The various functions are defined in funcs.cc. Additional documentation on
   what each function does can be found there */

struct instr;

int call_instr(instr *, unsigned);
int arith_instr(instr *, unsigned);
int arith_res(instr *, unsigned);
int arith_3(instr *, unsigned);
int arith_3y(instr *, unsigned);
int sarith_3(instr *, unsigned);
int rarith_3(instr *, unsigned);
int arith_3cc(instr *, unsigned);
int arith_3sc(instr *, unsigned);
int arith_3sccc(instr *, unsigned);
int jmpl(instr *, unsigned);
int ret(instr *, unsigned);
int flush(instr *, unsigned);
int arith_shift(instr *, unsigned);
int arith_spec1(instr *, unsigned);
int arith_spec2(instr *, unsigned);
int rdpr(instr *, unsigned);
int wrpr(instr *, unsigned);
int flushw(instr *, unsigned);
int fp_op1(instr *, unsigned);
int fp_op2(instr *, unsigned);
int fp_3(instr *, unsigned);
int fp_3s(instr *, unsigned);
int fp_3sd(instr *, unsigned);
int fmovrcc(instr *, unsigned);
int fmovrccs(instr *, unsigned);
int fcmp(instr *, unsigned);
int fcmps(instr *, unsigned);
int fp_2(instr *, unsigned);
int fp_2s(instr *, unsigned);
int fp_2sd(instr *, unsigned);
int fp_2ds(instr *, unsigned);
int fp_2if(instr *, unsigned);
int fp_2fi(instr *, unsigned);
int mem_instr(instr *, unsigned);
int mem_op2(instr *, unsigned);
int amem_op2(instr *, unsigned);
int dmem_op2(instr *, unsigned);
int damem_op2(instr *, unsigned);
int mem_op2f(instr *, unsigned);
int mem_op2fsr(instr *, unsigned);
int mem_op2fs(instr *, unsigned);
int amem_op2f(instr *, unsigned);
int amem_op2fs(instr *, unsigned);
int smem_op2(instr *, unsigned);
int sdmem_op2(instr *, unsigned);
int smem_op2f(instr *, unsigned);
int smem_op2fsr(instr *, unsigned);
int smem_op2fs(instr *, unsigned);
int samem_op2(instr *, unsigned);
int sdamem_op2(instr *, unsigned);
int samem_op2f(instr *, unsigned);
int samem_op2fs(instr *, unsigned);
int pref(instr *, unsigned);
int apref(instr *, unsigned);
int swap(instr *, unsigned);
int aswap(instr *, unsigned);
int cas(instr *, unsigned);
int mem_res(instr *, unsigned);
int movcc(instr *, unsigned);
int fmovcc(instr *, unsigned);
int fmovccs(instr *, unsigned);
int ftrig(instr *, unsigned);
int tcc(instr *, unsigned);
int movr(instr *, unsigned);
int popc(instr *, unsigned);
int savrestd(instr *, unsigned);
int branch_instr(instr *, unsigned);
int illtrap(instr *, unsigned);
int bpcc(instr *, unsigned);
int fbpfcc(instr *, unsigned);
int fbfcc(instr *, unsigned);
int sethi(instr *, unsigned);
int bpr(instr *, unsigned);
int bicc(instr *, unsigned);
int brres(instr *, unsigned);
int impdep1(instr *, unsigned);
int impdep2(instr *, unsigned);

extern IMF start_decode;

#endif
