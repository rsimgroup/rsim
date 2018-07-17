/*
  names.c

  This file has the names used in the various trace and statistics messages
  All arrays correspond to enum types or #defines from the various header
  files
  
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




#include "MemSys/req.h"
#include "MemSys/stats.h"
#include "MemSys/cache.h"
#include "MemSys/directory.h"

/* Req_Type: corresponds to enum ReqType -- req_type field of REQ */
char *Req_Type[Req_type_max] = {"","READ", "WRITE", "RMW",
				"L1WRITE_PREFETCH","L1READ_PREFETCH",
				"L2WRITE_PREFETCH","L2READ_PREFETCH",
				"TTS", "SPIN", "WRITEFLAG",
				
				"READ_SH","READ_OWN","UPGRADE",
				"READ_DISC","WRITE_INV",
				"RWRITE",
				
				"REPLY_SH","REPLY_EXCL","REPLY_UPGRADE",
				"REPLY_EXCLDY","REPLY_SHDY",
				"REPLY_DISC","REPLY_WRITEINV",
				
				"COPYBACK","COPYBACK_INVL","INVL",
				"COPYBACK_SHDY","COPYBACK_DISC","PUSH",
				
				"WRB","REPL"
				};


/* Reply_st: corresponds to #defines in req.h  -- s.reply field of REQ */
char *Reply_st[Reply_type_max] = {"", "REPLY", "", "RAR", ""};
/* Request_st: corresponds to #defines in req.h  -- s.type field of REQ  */
char *Request_st[Req_st_max] = {"", "REPLY", "","","","REQUEST", "COHE","COHE_REPLY"};

/* Reply: corresponds to #defines in req.h -- s.reply field of REQ */
char *Reply[Reply_type_max] = {"","REPLY","","RAR","", "NACK","NACK_PEND"};
/* ReqDir: corresponds to #defines in req.h -- s.dir field of REQ */
char *ReqDir[Dir_type_max] = {"","REQ_FWD", "REQ_BWD", ""};

/* CacheMissTypes: corresponds to enum CacheMissType in stats.h --
   used in stat.demand_miss and stat.pref_miss fields of CACHE */
char *CacheMissTypes[NUM_CACHE_MISS_TYPES] = { "CACHE_HIT",
					       "CACHE_MISS_COLD",
					       "CACHE_MISS_CONF",
					       "CACHE_MISS_CAP",
					       "CACHE_MISS_COHE",
					       "CACHE_MISS_UPGR",
					       "CACHE_MISS_WT",
					       "CACHE_MISS_COAL",
};

/* State: corresponds to enum CacheLineState in cache.h -- used in
   state.st field of cache_data_struct (held for each line) */
char *State[NUM_CACHE_LINE_STATES] = {"","INVALID","PR_CL","SH_CL","PR_DY","SH_DY"};
/* Cohe: corresponds to #defines in cohe_types.h -- represent cohe types
   of lines in caches (not all supported in RSIM) */
char *Cohe[Cohe_max] = {"","PR_WT","PR_WB","WT_INV","WT_UPD","WB_REF","WB_NREF",
			       "WTWSH_REF","WTWSH_NREF","NC"};

/* DirRtnStatus: corresponds to #defines in directory.h -- response types from
   Dir_Cohe */
char *DirRtnStatus[Directory_rtn_max] = {"", "WAIT_CNT", "WAIT_PEND", "VISIT_MEM", "DIR_REPLY", 
                                "", "", "SPECIAL_REPLY",
			        "FORWARD_REQUEST", "ACK_REPLY", "WAIT_FOR_WRB"};

/* MSHRret -- corresponds to enum MSHR_Response in mshr.h -- responses
              from notpres_mshr  */
char *MSHRret[] = {"MSHR_COAL","MSHR_NEW","MSHR_FWD",
		   "MSHR_STALL_WAR","MSHR_STALL_COHE",
		   "MSHR_STALL_COAL","MSHR_STALL_WRB",
		   "MSHR_USELESS_FETCH_IN_PROGRESS",
		   "NOMSHR_STALL",
		   "NOMSHR_STALL_CONF","NOMSHR_STALL_COHE",
		   "NOMSHR_STALL_WRBBUF_FULL",
		   "NOMSHR_FWD","NOMSHR_PFBUF","NOMSHR"};

/* Req_stat_type: corresponds to enum ReqStatType in req.h -- specifies
   how access was resolved */
char *Req_stat_type[] = {"UNKNOWN", "L1HIT", "L1COAL", "WBCOAL",
			 "L2HIT", "L2COAL",
			 "DIR_LH_NOCOHE", "DIR_LH_RCOHE",
			 "DIR_RH_NOCOHE", "DIR_RH_LCOHE", "DIR_RH_RCOHE", "CACHE_TO_CACHE"
};
