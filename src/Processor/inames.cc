/*
  inames.cc

  Names of the various instructions

  */
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


#include "Processor/instruction.h"

char *inames[numINSTRS]={
"RESERVED","CALL","ILLTRAP","BPcc","Bicc","BPr","SETHI","FBPfcc","FBfcc",
"ADD","AND","OR","XOR","SUB","ANDN","ORN","XNOR","ADDC","MULX","UMUL",
"SMUL","SUBC","UDIVX","UDIV","SDIV","ADDcc","ANDcc","ORcc","XORcc",
"SUBcc","ANDNcc","ORNcc","XNORcc","ADDCcc","UMULcc","SMULcc","SUBCcc",
"UDIVcc","SDIVcc","TADDcc","TSUBcc","TADDccTV","TSUBccTV","MULScc",
"SLL","SRL","SRA","arithS1","RDPR",
"FLUSHW","MOVcc","SDIVX","POPC","MOVR","arithS2",
"SAVRESTD","WRPR","IMPDEP1","IMPDEP2","JMPL",
"RETURN","Tcc","FLUSH","SAVE","RESTORE","DONERETRY","FMOVs","FMOVd",
"FMOVq","FNEGs","FNEGd","FNEGq","FABSs","FABSd","FABSq","FSQRTs",
"FSQRTd","FSQRTq","FADDs","FADDd","FADDq","FSUBs","FSUBd","FSUBq",
"FMULs","FMULd","FMULq","FDIVs","FDIVd","FDIVq","FsMULd","FdMULq",
"FsTOx","FdTOx","FqTOx","FxTOs","FxTOd","FxToq","FiTOs","FdTOs",
"FqTOs","FiTOd","FsTOd","FqTOd","FiTOq","FsTOq","FdTOq","FsTOi",
"FdTOi","FqTOi","FMOVs0","FMOVd0","FMOVq0","FMOVs1","FMOVd1","FMOVq1",
"FMOVs2","FMOVd2","FMOVq2","FMOVs3","FMOVd3","FMOVq3","FMOVsi","FMOVdi",
"FMOVqi","FMOVsx","FMOVdx","FMOVqx","FCMPs","FCMPd","FCMPq",
"FCMPEs","FCMPEd","FCMPEq","FMOVRsZ","FMOVRdZ","FMOVRqZ","FMOVRsLEZ",
"FMOVRdLEZ","FMOVRqLEZ","FMOVRsLZ","FMOVRdLZ","FMOVRqLZ","FMOVRsNZ",
"FMOVRdNZ","FMOVRqNZ","FMOVRsGZ","FMOVRdGZ","FMOVRqGZ","FMOVRsGEZ",
"FMOVRdGEZ","FMOVRqGEZ","LDUW","LDUB","LDUH","LDD","STW","STB","STH",
"STD","LDSW","LDSB","LDSH","LDX","LDSTUB","STX","SWAP","LDUWA","LDUBA",
"LDUHA","LDDA","STWA","STBA","STHA","STDA","LDSWA","LDSBA","LDSHA",
"LDXA","LDSTUBA","STXA","SWAPA","LDF","LDFSR","LDXFSR","LDQF","LDDF","STF",
"STFSR","STXFSR","STQF","STDF","PREFETCH","LDFA","LDQFA","LDDFA","STFA","STQFA",
"STDFA","CASA","PREFETCHA","CASXA",
"RWSD", "RWWT_I", "RWWTI_I", "RWWS_I", "RWWSI_I",
"RWWT_F", "RWWTI_F", "RWWS_F", "RWWSI_F" 
};
