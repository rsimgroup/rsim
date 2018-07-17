/*

  Processor/units.cc

  This file contains function which determines which functional units are 
  in charge of processing the different kinds of instructions.

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
#include "Processor/instance.h"
#include "Processor/state.h"
#include "Processor/memory.h"

extern "C"
{
#include "MemSys/req.h"
}


UTYPE unit[numINSTRS];
LATTYPE lattype[numINSTRS];

#define WD 4     /* Word */
#define HW 2     /* Half-word */
#define DW 8     /* Double word */
#define QW 16    /* Quad word */
#define BT 1     /* Byte */

void UnitArraySetup()
{
  unit[iRESERVED]=uALU;
  unit[iCALL]=uALU;

  unit[iILLTRAP]=uALU; 

  unit[iBPcc]=uALU;
  unit[iBicc]=uALU;
  unit[iBPr]=uALU;
  unit[iFBPfcc]=uALU;
  unit[iFBfcc]=uALU;
  
  unit[iSETHI]=uALU;
  unit[iADD]=uALU;
  unit[iAND]=uALU;
  unit[iOR]=uALU;
  unit[iXOR]=uALU;
  unit[iSUB]=uALU;
  unit[iANDN]=uALU;
  unit[iORN]=uALU;
  unit[iXNOR]=uALU;
  unit[iADDC]=uALU;
  
  unit[iMULX]=uALU; // uMUL;
  unit[iUMUL]=uALU; // uMUL;
  unit[iSMUL]=uALU; // uMUL;
  unit[iUDIVX]=uALU; // uMUL;
  unit[iUDIV]=uALU; // uMUL;
  unit[iSDIV]=uALU; // uMUL;
  
  unit[iSUBC]=uALU;
  unit[iADDcc]=uALU;
  unit[iANDcc]=uALU;
  unit[iORcc]=uALU;
  unit[iXORcc]=uALU;
  
  unit[iSUBcc]=uALU;
  unit[iANDNcc]=uALU;
  unit[iORNcc]=uALU;
  unit[iXNORcc]=uALU;
  unit[iADDCcc]=uALU;
  
  unit[iUMULcc]=uALU; // uMUL;
  unit[iSMULcc]=uALU; // uMUL;
  unit[iUDIVcc]=uALU; // uMUL;
  unit[iSDIVcc]=uALU; // uMUL;
  
  unit[iSUBCcc]=uALU;
  unit[iTADDcc]=uALU;
  unit[iTSUBcc]=uALU;
  unit[iTADDccTV]=uALU;
  unit[iTSUBccTV]=uALU;
  unit[iMULScc]=uALU;
  
  unit[iSLL]=uALU; // uSHIFT;
  unit[iSRL]=uALU; // uSHIFT;
  unit[iSRA]=uALU; // uSHIFT;
  
  unit[iarithSPECIAL1]=uALU; /* arithSPECIAL1 includes MEMBAR */
  
  unit[iRDPR]=uALU;
  unit[iFLUSHW]=uMEM;
  unit[iMOVcc]=uALU;
  unit[iSDIVX]=uALU;
  unit[iPOPC]=uALU;
  unit[iMOVR]=uALU;
  unit[iarithSPECIAL2]=uALU;
  
  unit[iSAVRESTD]=uALU;
  unit[iWRPR]=uALU;
  unit[iIMPDEP1]=uALU;
  unit[iIMPDEP2]=uALU;
  unit[iJMPL]=uALU;
  
  unit[iRETURN]=uALU;
  unit[iTcc]=uALU;
  unit[iFLUSH]=uMEM;
  unit[iSAVE]=uALU;
  unit[iRESTORE]=uALU;
  unit[iDONERETRY]=uALU;
  
  unit[iFMOVs]=uFP;
  unit[iFMOVd]=uFP;
  unit[iFMOVq]=uFP; // since these are such simple ops
  unit[iFNEGs]=uFP; // let them run at high speed
  unit[iFNEGd]=uFP;
  unit[iFNEGq]=uFP;
  unit[iFABSs]=uFP;
  unit[iFABSd]=uFP;
  unit[iFABSq]=uFP;
  
  unit[iFSQRTs]=uFP;  
  unit[iFSQRTd]=uFP;
  unit[iFSQRTq]=uFP;
  unit[iFADDs]=uFP;
  unit[iFADDd]=uFP;
  unit[iFADDq]=uFP;
  unit[iFSUBs]=uFP;
  unit[iFSUBd]=uFP;
  unit[iFSUBq]=uFP;
  
  unit[iFMULs]=uFP;
  unit[iFMULd]=uFP;
  unit[iFMULq]=uFP;
  unit[iFDIVs]=uFP;
  unit[iFDIVd]=uFP;
  unit[iFDIVq]=uFP;
  unit[iFsMULd]=uFP;
  unit[iFdMULq]=uFP;
  
  unit[iFsTOx]=uFP;
  unit[iFdTOx]=uFP;
  unit[iFqTOx]=uFP;
  unit[iFxTOs]=uFP;
  unit[iFxTOd]=uFP;
  unit[iFxToq]=uFP;
  unit[iFiTOs]=uFP;
  unit[iFdTOs]=uFP;
  
  unit[iFqTOs]=uFP;
  unit[iFiTOd]=uFP;
  unit[iFsTOd]=uFP;
  unit[iFqTOd]=uFP;
  unit[iFiTOq]=uFP;
  unit[iFsTOq]=uFP;
  unit[iFdTOq]=uFP;
  unit[iFsTOi]=uFP;
  
  unit[iFdTOi]=uFP;
  unit[iFqTOi]=uFP;
  unit[iFMOVs0]=uFP;
  unit[iFMOVd0]=uFP;
  unit[iFMOVq0]=uFP;
  unit[iFMOVs1]=uFP;
  unit[iFMOVd1]=uFP;
  unit[iFMOVq1]=uFP;
  
  unit[iFMOVs2]=uFP;
  unit[iFMOVd2]=uFP;
  unit[iFMOVq2]=uFP;
  unit[iFMOVs3]=uFP;
  unit[iFMOVd3]=uFP;
  unit[iFMOVq3]=uFP;
  unit[iFMOVsi]=uFP;
  
  unit[iFMOVdi]=uFP;
  unit[iFMOVqi]=uFP;
  unit[iFMOVsx]=uFP;
  unit[iFMOVdx]=uFP;
  unit[iFMOVqx]=uFP;
  unit[iFCMPs]=uFP;
  unit[iFCMPd]=uFP;
  unit[iFCMPq]=uFP;
  
  unit[iFCMPEs]=uFP;
  unit[iFCMPEd]=uFP;
  unit[iFCMPEq]=uFP;
  unit[iFMOVRsZ]=uFP;
  unit[iFMOVRdZ]=uFP;
  unit[iFMOVRqZ]=uFP;
  unit[iFMOVRsLEZ]=uFP;
  
  unit[iFMOVRdLEZ]=uFP;
  unit[iFMOVRqLEZ]=uFP;
  unit[iFMOVRsLZ]=uFP;
  unit[iFMOVRdLZ]=uFP;
  unit[iFMOVRqLZ]=uFP;
  unit[iFMOVRsNZ]=uFP;
  
  unit[iFMOVRdNZ]=uFP;
  unit[iFMOVRqNZ]=uFP;
  unit[iFMOVRsGZ]=uFP;
  unit[iFMOVRdGZ]=uFP;
  unit[iFMOVRqGZ]=uFP;
  unit[iFMOVRsGEZ]=uFP;
  
  unit[iFMOVRdGEZ]=uFP;
  unit[iFMOVRqGEZ]=uFP;

  unit[iLDUW]=uMEM;
  unit[iLDUB]=uMEM;
  unit[iLDUH]=uMEM;
  unit[iLDD]=uMEM;
  unit[iSTW]=uMEM;
  unit[iSTB]=uMEM;
  unit[iSTH]=uMEM;
  
  unit[iSTD]=uMEM;
  unit[iLDSW]=uMEM;
  unit[iLDSB]=uMEM;
  unit[iLDSH]=uMEM;
  unit[iLDX]=uMEM;
  unit[iLDSTUB]=uMEM;
  unit[iSTX]=uMEM;
  unit[iSWAP]=uMEM;
  unit[iLDUWA]=uMEM;
  unit[iLDUBA]=uMEM;
  
  unit[iLDUHA]=uMEM;
  unit[iLDDA]=uMEM;
  unit[iSTWA]=uMEM;
  unit[iSTBA]=uMEM;
  unit[iSTHA]=uMEM;
  unit[iSTDA]=uMEM;
  unit[iLDSWA]=uMEM;
  unit[iLDSBA]=uMEM;
  unit[iLDSHA]=uMEM;
  
  unit[iLDXA]=uMEM;
  unit[iLDSTUBA]=uMEM;
  unit[iSTXA]=uMEM;
  unit[iSWAPA]=uMEM;
  unit[iLDF]=uMEM;
  unit[iLDFSR]=uMEM;
  unit[iLDXFSR]=uMEM;
  unit[iLDQF]=uMEM;
  unit[iLDDF]=uMEM;
  unit[iSTF]=uMEM;
  
  unit[iSTFSR]=uMEM;
  unit[iSTXFSR]=uMEM;
  unit[iSTQF]=uMEM;
  unit[iSTDF]=uMEM;
  unit[iPREFETCH]=uMEM;
  unit[iLDFA]=uMEM;
  unit[iLDQFA]=uMEM;
  unit[iLDDFA]=uMEM;
  unit[iSTFA]=uMEM;
  unit[iSTQFA]=uMEM;

  /* remote write related instructions */	
  unit[iRWSD]   = uMEM;
  unit[iRWWT_I] = uMEM;
  unit[iRWWT_F] = uMEM;
  unit[iRWWTI_I] = uMEM;
  unit[iRWWTI_F] = uMEM;
  unit[iRWWS_I]  = uMEM;
  unit[iRWWS_F]  = uMEM;
  unit[iRWWSI_I]  = uMEM;
  unit[iRWWSI_F] = uMEM;
  /* end of remote write instructions */

  unit[iSTDFA]=uMEM;
  unit[iCASA]=uMEM;
  unit[iPREFETCHA]=uMEM;
  unit[iCASXA]=uMEM;

  /* remote write related types */
  mem_acctype[iRWWT_I] = RWRITE;
  mem_length[iRWWT_I] = WD;
  mem_align[iRWWT_I] = WD;
  mem_acctype[iRWWTI_I] = RWRITE;
  mem_length[iRWWTI_I] = WD;
  mem_align[iRWWTI_I] = WD;
  mem_acctype[iRWWS_I] = RWRITE;
  mem_length[iRWWS_I] = WD;
  mem_align[iRWWS_I] = WD;
  mem_acctype[iRWWSI_I] = RWRITE;
  mem_length[iRWWSI_I] = WD;
  mem_align[iRWWSI_I] = WD;

  mem_acctype[iRWWT_F] = RWRITE;
  mem_length[iRWWT_F] = DW;
  mem_align[iRWWT_F] = DW;
  mem_acctype[iRWWTI_F] = RWRITE;
  mem_length[iRWWTI_F] = DW;
  mem_align[iRWWTI_F] = DW;
  mem_acctype[iRWWS_F] = RWRITE;
  mem_length[iRWWS_F] = DW;
  mem_align[iRWWS_F] = DW;
  mem_acctype[iRWWSI_F] = RWRITE;
  mem_length[iRWWSI_F] = DW;
  mem_align[iRWWSI_F] = DW;
  /* end of remote write types */

  mem_acctype[iLDUW]=READ;
  mem_length[iLDUW]=WD;
  mem_align[iLDUW]=WD;

  mem_acctype[iLDUB]=READ;
  mem_length[iLDUB]=BT;
  mem_align[iLDUB]=BT;

  mem_acctype[iLDUH]=READ;
  mem_length[iLDUH]=HW;
  mem_align[iLDUH]=HW;

  mem_acctype[iLDD]=READ;
  mem_length[iLDD]=DW;
  mem_align[iLDD]=DW;

  mem_acctype[iSTW]=WRITE;
  mem_length[iSTW]=WD;
  mem_align[iSTW]=WD;

  mem_acctype[iSTB]=WRITE;
  mem_length[iSTB]=BT;
  mem_align[iSTB]=BT;

  mem_acctype[iSTH]=WRITE;
  mem_length[iSTH]=HW;
  mem_align[iSTH]=HW;

  mem_acctype[iSTD]=WRITE;
  mem_length[iSTD]=DW;
  mem_align[iSTD]=DW;
  // mem_length[iSTD]=WD;
  // mem_align[iSTD]=WD;

  mem_acctype[iLDSW]=READ;
  mem_length[iLDSW]=WD;
  mem_align[iLDSW]=WD;

  mem_acctype[iLDSB]=READ;
  mem_length[iLDSB]=BT;
  mem_align[iLDSB]=BT;

  mem_acctype[iLDSH]=READ;
  mem_length[iLDSH]=HW;
  mem_align[iLDSH]=HW;

  mem_acctype[iLDX]=READ;
  mem_length[iLDX]=DW;
  mem_align[iLDX]=DW;

  mem_acctype[iLDSTUB]=RMW; // *** maybe this should be LOCK_REQ ?
  mem_length[iLDSTUB]=BT;
  mem_align[iLDSTUB]=BT;

  mem_acctype[iSTX]=WRITE;
  mem_length[iSTX]=DW;
  mem_align[iSTX]=DW;

  mem_acctype[iSWAP]=RMW; // *** maybe this should be something else, based on MCS? maybe use 
  mem_length[iSWAP]=WD;
  mem_align[iSWAP]=WD;

  mem_acctype[iLDUWA]=READ;
  mem_length[iLDUWA]=WD;
  mem_align[iLDUWA]=WD;

  mem_acctype[iLDUBA]=READ;
  mem_length[iLDUBA]=BT;
  mem_align[iLDUBA]=BT;

  mem_acctype[iLDUHA]=READ;
  mem_length[iLDUHA]=HW;
  mem_align[iLDUHA]=HW;

  mem_acctype[iLDDA]=READ;
  mem_length[iLDDA]=DW;
  mem_align[iLDDA]=DW;

  mem_acctype[iSTWA]=WRITE;
  mem_length[iSTWA]=WD;
  mem_align[iSTWA]=WD;

  mem_acctype[iSTBA]=WRITE;
  mem_length[iSTBA]=BT;
  mem_align[iSTBA]=BT;

  mem_acctype[iSTHA]=WRITE;
  mem_length[iSTHA]=HW;
  mem_align[iSTHA]=HW;

  mem_acctype[iSTDA]=WRITE;
  mem_length[iSTDA]=DW;
  mem_align[iSTDA]=DW;
  // mem_length[iSTDA]=WD;
  // mem_align[iSTDA]=WD;

  mem_acctype[iLDSWA]=READ;
  mem_length[iLDSWA]=WD;
  mem_align[iLDSWA]=WD;

  mem_acctype[iLDSBA]=READ;
  mem_length[iLDSBA]=BT;
  mem_align[iLDSBA]=BT;

  mem_acctype[iLDSHA]=READ;
  mem_length[iLDSHA]=HW;
  mem_align[iLDSHA]=HW;

  mem_acctype[iLDXA]=READ;
  mem_length[iLDXA]=DW;
  mem_align[iLDXA]=DW;

  mem_acctype[iLDSTUBA]=RMW;
  // I had made the "ASI" version of this a LOCK; the ordinary can be RMW
  mem_length[iLDSTUBA]=BT;
  mem_align[iLDSTUBA]=BT;

  mem_acctype[iSTXA]=WRITE;
  mem_length[iSTXA]=DW;
  mem_align[iSTXA]=DW;

  mem_acctype[iSWAPA]=RMW;
  mem_length[iSWAPA]=WD;
  mem_align[iSWAPA]=WD;

  mem_acctype[iLDF]=READ;
  mem_length[iLDF]=WD;
  mem_align[iLDF]=WD;

  mem_acctype[iLDFSR]=READ;
  mem_length[iLDFSR]=WD;
  mem_align[iLDFSR]=WD;

  mem_acctype[iLDXFSR]=READ;
  mem_length[iLDXFSR]=DW;
  mem_align[iLDXFSR]=DW;

  mem_acctype[iLDQF]=READ;
  mem_length[iLDQF]=QW;
  mem_align[iLDQF]=WD;
  /* note: even though this is QW length, it only needs to be WD aligned */

  mem_acctype[iLDDF]=READ;
  mem_length[iLDDF]=DW;
  mem_align[iLDDF]=WD;
  /* note: even though this is DW length, it only needs to be WD aligned */

  mem_acctype[iSTF]=WRITE;
  mem_length[iSTF]=WD;
  mem_align[iSTF]=WD;

  mem_acctype[iSTFSR]=WRITE;
  mem_length[iSTFSR]=WD;
  mem_align[iSTFSR]=WD;

  mem_acctype[iSTXFSR]=WRITE;
  mem_length[iSTXFSR]=DW;
  mem_align[iSTXFSR]=DW;

  mem_acctype[iSTQF]=WRITE;
  mem_length[iSTQF]=QW;
  mem_align[iSTQF]=WD;
  /* note: even though this is QW length, it only needs to be WD aligned */

  mem_acctype[iSTDF]=WRITE;
  mem_length[iSTDF]=DW;
  mem_align[iSTDF]=WD;
  /* note: even though this is DW length, it only needs to be WD aligned */

  mem_acctype[iPREFETCH]=READ;
  mem_length[iPREFETCH]=BT;
  mem_align[iPREFETCH]=BT; /* prefetches are never misaligned... */

  mem_acctype[iLDFA]=READ;
  mem_length[iLDFA]=WD;
  mem_align[iLDFA]=WD;

  mem_acctype[iLDQFA]=READ;
  mem_length[iLDQFA]=QW;
  mem_align[iLDQFA]=WD;
  /* note: even though this is QW length, it only needs to be WD aligned */

  mem_acctype[iLDDFA]=READ;
  mem_length[iLDDFA]=DW;
  mem_align[iLDDFA]=WD;
  /* note: even though this is DW length, it only needs to be WD aligned */

  mem_acctype[iSTFA]=WRITE;
  mem_length[iSTFA]=WD;
  mem_align[iSTFA]=WD;

  mem_acctype[iSTQFA]=WRITE;
  mem_length[iSTQFA]=QW;
  mem_align[iSTQFA]=WD;
  /* note: even though this is QW length, it only needs to be WD aligned */

  mem_acctype[iSTDFA]=WRITE;
  mem_length[iSTDFA]=DW;
  mem_align[iSTDFA]=WD;
  /* note: even though this is DW length, it only needs to be WD aligned */

  mem_acctype[iCASA]=RMW; 
  mem_length[iCASA]=WD;
  mem_align[iCASA]=WD;

  mem_acctype[iPREFETCHA]=READ;
  mem_length[iPREFETCHA]=WD;
  mem_align[iPREFETCHA]=WD;

  mem_acctype[iCASXA]=RMW;
  mem_length[iCASXA]=DW;
  mem_align[iCASXA]=DW;


  lattype[iRESERVED]=lEXCEPT;
  lattype[iCALL]=lBRANCH;
  lattype[iILLTRAP]=lEXCEPT; 
  lattype[iSETHI]=lALU;

  lattype[iBPcc]=lBRANCH;
  lattype[iBicc]=lBRANCH;
  lattype[iBPr]=lBRANCH;
  lattype[iFBPfcc]=lBRANCH;
  lattype[iFBfcc]=lBRANCH;
  
  lattype[iADD]=lALU;
  lattype[iAND]=lALU;
  lattype[iOR]=lALU;
  lattype[iXOR]=lALU;
  lattype[iSUB]=lALU;
  lattype[iANDN]=lALU;
  lattype[iORN]=lALU;
  lattype[iXNOR]=lALU;
  lattype[iADDC]=lALU;
  lattype[iMULX]=lALU;
  lattype[iUMUL]=lALU;
  lattype[iSMUL]=lALU;
  lattype[iUDIVX]=lALU;
  lattype[iUDIV]=lALU;
  lattype[iSDIV]=lALU;
  lattype[iSUBC]=lALU;
  lattype[iADDcc]=lALU;
  lattype[iANDcc]=lALU;
  lattype[iORcc]=lALU;
  lattype[iXORcc]=lALU;
  lattype[iSUBcc]=lALU;
  lattype[iANDNcc]=lALU;
  lattype[iORNcc]=lALU;
  lattype[iXNORcc]=lALU;
  lattype[iADDCcc]=lALU;
  lattype[iUMULcc]=lALU;
  lattype[iSMULcc]=lALU;
  lattype[iUDIVcc]=lALU;
  lattype[iSDIVcc]=lALU;
  lattype[iSUBCcc]=lALU;
  lattype[iTADDcc]=lALU;
  lattype[iTSUBcc]=lALU;
  lattype[iTADDccTV]=lALU;
  lattype[iTSUBccTV]=lALU;
  lattype[iMULScc]=lALU;
  lattype[iSLL]=lALU;
  lattype[iSRL]=lALU;
  lattype[iSRA]=lALU;
  lattype[iarithSPECIAL1]=lMEMBAR;
  lattype[iRDPR]=lALU;
  lattype[iFLUSHW]=lEXCEPT;
  lattype[iMOVcc]=lALU;
  lattype[iSDIVX]=lALU;
  lattype[iPOPC]=lALU;
  lattype[iMOVR]=lALU;
  lattype[iarithSPECIAL2]=lALU;
  lattype[iSAVRESTD]=lALU;
  lattype[iWRPR]=lALU;
  lattype[iIMPDEP1]=lALU;
  lattype[iIMPDEP2]=lALU;
  lattype[iRETURN]=lALU;
  lattype[iTcc]=lEXCEPT;
  lattype[iFLUSH]=lEXCEPT;
  lattype[iSAVE]=lALU;
  lattype[iRESTORE]=lALU;
  lattype[iDONERETRY]=lALU;
  lattype[iFMOVs]=lFPU;
  lattype[iFMOVd]=lFPU;
  lattype[iFMOVq]=lFPU; // since these are such simple ops
  lattype[iFNEGs]=lFPU; // let them run at high speed
  lattype[iFNEGd]=lFPU;
  lattype[iFNEGq]=lFPU;
  lattype[iFABSs]=lFPU;
  lattype[iFABSd]=lFPU;
  lattype[iFABSq]=lFPU;
  lattype[iFSQRTs]=lFPU;  
  lattype[iFSQRTd]=lFPU;
  lattype[iFSQRTq]=lFPU;
  lattype[iFADDs]=lFPU;
  lattype[iFADDd]=lFPU;
  lattype[iFADDq]=lFPU;
  lattype[iFSUBs]=lFPU;
  lattype[iFSUBd]=lFPU;
  lattype[iFSUBq]=lFPU;
  lattype[iFMULs]=lFPU;
  lattype[iFMULd]=lFPU;
  lattype[iFMULq]=lFPU;
  lattype[iFDIVs]=lFPU;
  lattype[iFDIVd]=lFPU;
  lattype[iFDIVq]=lFPU;
  lattype[iFsMULd]=lFPU;
  lattype[iFdMULq]=lFPU;
  lattype[iFsTOx]=lFPU;
  lattype[iFdTOx]=lFPU;
  lattype[iFqTOx]=lFPU;
  lattype[iFxTOs]=lFPU;
  lattype[iFxTOd]=lFPU;
  lattype[iFxToq]=lFPU;
  lattype[iFiTOs]=lFPU;
  lattype[iFdTOs]=lFPU;
  lattype[iFqTOs]=lFPU;
  lattype[iFiTOd]=lFPU;
  lattype[iFsTOd]=lFPU;
  lattype[iFqTOd]=lFPU;
  lattype[iFiTOq]=lFPU;
  lattype[iFsTOq]=lFPU;
  lattype[iFdTOq]=lFPU;
  lattype[iFsTOi]=lFPU;
  lattype[iFdTOi]=lFPU;
  lattype[iFqTOi]=lFPU;
  lattype[iFMOVs0]=lFPU;
  lattype[iFMOVd0]=lFPU;
  lattype[iFMOVq0]=lFPU;
  lattype[iFMOVs1]=lFPU;
  lattype[iFMOVd1]=lFPU;
  lattype[iFMOVq1]=lFPU;
  lattype[iFMOVs2]=lFPU;
  lattype[iFMOVd2]=lFPU;
  lattype[iFMOVq2]=lFPU;
  lattype[iFMOVs3]=lFPU;
  lattype[iFMOVd3]=lFPU;
  lattype[iFMOVq3]=lFPU;
  lattype[iFMOVsi]=lFPU;
  lattype[iFMOVdi]=lFPU;
  lattype[iFMOVqi]=lFPU;
  lattype[iFMOVsx]=lFPU;
  lattype[iFMOVdx]=lFPU;
  lattype[iFMOVqx]=lFPU;
  lattype[iFCMPs]=lFPU;
  lattype[iFCMPd]=lFPU;
  lattype[iFCMPq]=lFPU;
  lattype[iFCMPEs]=lFPU;
  lattype[iFCMPEd]=lFPU;
  lattype[iFCMPEq]=lFPU;
  lattype[iFMOVRsZ]=lFPU;
  lattype[iFMOVRdZ]=lFPU;
  lattype[iFMOVRqZ]=lFPU;
  lattype[iFMOVRsLEZ]=lFPU;
  lattype[iFMOVRdLEZ]=lFPU;
  lattype[iFMOVRqLEZ]=lFPU;
  lattype[iFMOVRsLZ]=lFPU;
  lattype[iFMOVRdLZ]=lFPU;
  lattype[iFMOVRqLZ]=lFPU;
  lattype[iFMOVRsNZ]=lFPU;
  lattype[iFMOVRdNZ]=lFPU;
  lattype[iFMOVRqNZ]=lFPU;
  lattype[iFMOVRsGZ]=lFPU;
  lattype[iFMOVRdGZ]=lFPU;
  lattype[iFMOVRqGZ]=lFPU;
  lattype[iFMOVRsGEZ]=lFPU;
  lattype[iFMOVRdGEZ]=lFPU;
  lattype[iFMOVRqGEZ]=lFPU;
  lattype[iLDUW]=lRD;
  lattype[iLDUB]=lRD;
  lattype[iLDUH]=lRD;
  lattype[iLDD]=lRD;
  lattype[iSTW]=lWT;
  lattype[iSTB]=lWT;
  lattype[iSTH]=lWT;
  lattype[iSTD]=lWT;
  lattype[iLDSW]=lRD;
  lattype[iLDSB]=lRD;
  lattype[iLDSH]=lRD;
  lattype[iLDX]=lRD;
  lattype[iLDSTUB]=lRMW;
  lattype[iSTX]=lWT;
  lattype[iSWAP]=lRMW;
  lattype[iLDUWA]=lRD;
  lattype[iLDUBA]=lRD;
  lattype[iLDUHA]=lRD;
  lattype[iLDDA]=lRD;
  lattype[iSTWA]=lWT;
  lattype[iSTBA]=lWT;
  lattype[iSTHA]=lWT;
  lattype[iSTDA]=lWT;
  lattype[iLDSWA]=lRD;
  lattype[iLDSBA]=lRD;
  lattype[iLDSHA]=lRD;
  lattype[iLDXA]=lRD;
  lattype[iLDSTUBA]=lRMW;
  lattype[iSTXA]=lWT;
  lattype[iSWAPA]=lRMW;
  lattype[iLDF]=lRD;
  lattype[iLDFSR]=lRD;
  lattype[iLDXFSR]=lRD;
  lattype[iLDQF]=lRD;
  lattype[iLDDF]=lRD;
  lattype[iSTF]=lWT;
  lattype[iSTFSR]=lWT;
  lattype[iSTXFSR]=lWT;
  lattype[iSTQF]=lWT;
  lattype[iSTDF]=lWT;
  lattype[iPREFETCH]=lRD;
  lattype[iLDFA]=lRD;
  lattype[iLDQFA]=lRD;
  lattype[iLDDFA]=lRD;
  lattype[iSTFA]=lWT;
  lattype[iSTQFA]=lWT;
  lattype[iSTDFA]=lWT;
  lattype[iCASA]=lRMW;
  lattype[iPREFETCHA]=lRD;
  lattype[iCASXA]=lRMW;
  lattype[iRWSD]=lWT;
  lattype[iRWWT_I]=lWT;
  lattype[iRWWTI_I]=lWT;
  lattype[iRWWS_I]=lWT;
  lattype[iRWWSI_I]=lWT;
  lattype[iRWWT_F]=lWT;
  lattype[iRWWTI_F]=lWT;
  lattype[iRWWS_F]=lWT;
  lattype[iRWWSI_F]=lWT;

}







