/* cache2.c

   Helper routines for cache.

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
#include "MemSys/cache.h"
#include "MemSys/req.h"
#include "MemSys/directory.h"
#include "MemSys/associate.h"
#include "Processor/memprocess.h"
#include "MemSys/mshr.h"
#include "Processor/capconf.h"
#include "Processor/simio.h"

#include <malloc.h>

extern int captr_block_bits;

/*****************************************************************************/
/* init_cache: This routine initializes the cache data structures.  This     */
/* is called by the NewCache() routine.                                      */
/*****************************************************************************/

void init_cache(CACHE *captr)
{
  int i, j, k, ii;
  int lsb = 0;
  int temp, nof_sets;
  int num_lines;
  int cachesize, blocksize, setsize;
  int set_bits, blockbits;

  cachesize = captr->size;
  blocksize = captr->linesz;
  setsize = captr->setsz;

  if (blocksize < WORDSZ ) {	/* block size should not be less than word size */
    fprintf(simout,"NewCache(): line size: %d bytes in cache \"%s\"  (Word size: %d)\n",blocksize,
           captr->name, WORDSZ);
    YS__errmsg("NewCache(): Smallest line size is a word ");
  }

  blockbits = 0;
  temp = blocksize;
  while (temp && !lsb) {	/* check that block size is a power of two */
    lsb = temp & 1;
    temp = temp >> 1;
    blockbits++;
  }
  blockbits--;
  if (temp) {
    fprintf(simout,"NewCache(): line size: %d bytes in cache \"%s\" \n",blocksize,
	    captr->name);
    YS__errmsg("NewCache(): line size specified in bytes, must be a power of 2");
  }
  
  lsb = 0;
  if (setsize != FULL_ASS) {
    temp = setsize;
    while (temp && !lsb) {	/* check that set size is a power of two */
      lsb = temp & 1;
      temp = temp >> 1;
    }
    if (temp) {
      fprintf(simout,"NewCache(): set size: %d in cache \"%s\" \n",setsize,
             captr->name);
      YS__errmsg("NewCache(): set size must be a power of 2 ");
    }
  }

  if (cachesize == INFINITE) {	/* if cache size is INFINITE set size must be fully associative */
    if (setsize != FULL_ASS)
      YS__warnmsg("NewCache(): cache size infinite; set size made fully associative");
    nof_sets = 1;
    set_bits = 0;
    num_lines  = SUB_SZ;
    setsize = SUB_SZ;
    captr->setsz = SUB_SZ;
  }

  else {                        /* not an infinite cache */
    if ((cachesize*1024)%blocksize) {
      fprintf(simout,"NewCache(): line size: %d\tcache size %d in cache \"%s\" \n",
             blocksize, cachesize,captr->name);
      YS__errmsg("NewCache(): Line size must divide into cache size");
    }

    num_lines = (cachesize*1024)/blocksize; /* get number of lines in the cache */

    if (setsize != FULL_ASS){ /* set associativity */
      if (setsize > num_lines) {
        fprintf(simout,"NewCache(): cache size: %d KB, line size: %d bytes, num_lines: %d, set size: %d\n"
		,cachesize, blocksize, num_lines, setsize);
        fprintf(simout," in cache \"%s\" \n",captr->name);
        YS__errmsg("NewCache(): Degree of set associativity cannot be greater than number of lines in cache");
      }
      nof_sets   = (1024*cachesize)/(blocksize*setsize); /* number of sets */
      set_bits = 0;
      temp = nof_sets;
      while ( temp )		/* get number of bits it takes to express set number */
        {
          temp = temp >> 1;
          set_bits++;
        }
      set_bits--;
    }
    else {			/* set size is fully associative; */
      setsize = num_lines;
      captr->setsz = setsize;
      nof_sets = 1;
      set_bits = 0;
    }
  }
  /* setup cache data structures for manipulating address bits */
  captr->non_tag_bits = blockbits + set_bits; 
  captr_block_bits = captr->block_bits = blockbits;
  captr->set_bits = set_bits;
  captr->num_lines = num_lines;

  /* The cache data structure is divided into a two dimentional
    array. The data structure has one entry for each cache line. The
    array is [100][SUB_SZ] maximum.  This gives a maximum of
    SUB_SZ*100 lines (204800 when this comment was written).  However
    for infinite caches all of this space is not allocated at this
    time.  To start with only space for SUB_SZ lines are
    malloc-ed. Then as the cache has capacity misses more space is
    allocated during simulation. For finite caches all space is
    allocated at this time.
    */

  if (num_lines > (SUB_SZ*100)) {
    fprintf(simout,"NewCache(): cache size: %d KB, line size: %d bytes, num_lines: %d, \n",
	cachesize, blocksize, num_lines);
    fprintf(simout," in cache \"%s\" \n",captr->name);
    YS__errmsg("NewCache(): Simulation exceeds max limit on number of lines in cache");
  }
  /* To increase this limit, change SUB_SZ defined in ../../incl/MemSys/cache.h */
  captr->data = (Cachest **) malloc (sizeof (Cachest *) * 100);
  /* malloc space for 100 pointers (first diemnsion of the cache data structure array */
  if (!captr->data)
    YS__errmsg("NewCache(): malloc failed");

  if (num_lines <= SUB_SZ) {	/* need to malloc only one set of SUB_SZ cache line structures */
    captr->data[0] = (Cachest *) malloc(sizeof (Cachest) * num_lines);
    if (!captr->data[0])
      YS__errmsg("NewCache(): malloc failed");
  }
  else {			/* need to malloc several sets of cache line structures */
    temp = num_lines;
    for (i=0; temp > SUB_SZ; i++) {
      captr->data[i] = (Cachest *) malloc(sizeof (Cachest) * SUB_SZ);
      if (!captr->data[i])
        YS__errmsg("NewCache(): malloc failed");
      temp = temp - SUB_SZ;
    }
    captr->data[i] = (Cachest *) malloc(sizeof (Cachest) * temp);
    if (!captr->data[i])
      YS__errmsg("NewCache(): malloc failed");
  }
  /* initialize the array of cache lines */
  for ( j = 0; j < num_lines; j += setsize)
    for ( k = 0; k < setsize; k++)
      {
	i = (j+k)/SUB_SZ;
       ii = (j+k)%SUB_SZ;
       captr->data[i][ii].tag = -1; 
       captr->data[i][ii].age = k + 1; 
       captr->data[i][ii].pref = 0;
       captr->data[i][ii].pref_tag_repl = -1;
       captr->data[i][ii].state.st = INVALID; /* indicates invalid line */
       captr->data[i][ii].state.cohe_type = 0;
       captr->data[i][ii].state.allo_type = 0;
       captr->data[i][ii].state.mshr_out = 0;
       captr->data[i][ii].state.cohe_pend = 0;
     }

  /* Also start out a new CapConfDetector to determine miss types */
  captr->ccd = NewCapConfDetector(num_lines);

  /* initialize the SmartMSHR list pointers */
  captr->SmartMSHRHead=captr->SmartMSHRTail = NULL;

  /* Clear out and initialize the cache statistics */
  captr->num_ref  = 0;
  captr->num_miss = 0;
  captr->utilization = 0.0;
  memset(&captr->stat,0,sizeof(CacheStatStruct)); /* zero the statistics block out */
  
  for (i=0; i < 3; i++)
    captr->net_demand_miss[i]=NewStatrec("demand",POINT,MEANS,NOHIST,0,0.0,1.0);
  for (i=0; i < 4; i++)
    captr->net_pref_miss[i]=NewStatrec("pref",POINT,MEANS,NOHIST,0,0.0,1.0);
  
  captr->mshr_occ=NewStatrec("Mshr occupancy",INTERVAL,MEANS,HIST,captr->max_mshrs,0.0,(double)captr->max_mshrs);
  
  captr->mshr_req_count=NewStatrec("Mshr req occupancy",INTERVAL,MEANS,HIST,10,0.0,(double)(captr->max_mshrs * MAX_COALS));
  captr->pref_lateness=NewStatrec("Prefetch Lateness",POINT,MEANS,HIST,20,0.0,200.0);
  captr->pref_earlyness=NewStatrec("Prefetch Earlyness",POINT,MEANS,HIST,20,0.0,200.0);
}

/*****************************************************************************/
/* notpres: determines whether a tag is present in the cache or not.         */
/*                                                                           */
/* Returns: 0 -- hit                                                         */
/*          1 -- present miss (tag in cache, but INVALID due to COHE)        */
/*          2 -- total miss                                                  */
/* 	                                                                     */
/* Additionally, this function sets the tag field to be the tag used         */
/* when accessing this particular set of the cache for this line; the set    */
/* of the cache which holds the line, and the index of the data structure    */
/* which holds the line (if present).                                        */
/*****************************************************************************/

int notpres (long address, long *tag, int *set, int *set_ind, CACHE *captr)

{
 unsigned i, i1, i2;
 unsigned temp;
 
 /* process address to get the tag and set number */
 *tag = (address) >> (captr->non_tag_bits);
 *set = ((address) >> (captr->block_bits)) & ~(~0 << (captr->set_bits));
 *set_ind = *set * captr->setsz;

 if (captr->num_lines <= SUB_SZ) { /* small cache; fits in 1 dimension */
   temp = *set_ind;
   for ( i = 0; i < captr->setsz; i++) /* loop through the set looking for a match */
     {
       if ( captr->data[0][temp].tag == *tag) /* a match! */
         if (captr->data[0][temp].state.st != INVALID)
           {
             *set_ind = temp;
             return(0);		/* found a hit */
           }
         else
           {
             *set_ind = temp;
             return(1);		/* found a present miss; state is INVALID */
           }
       temp++;
     }
   return(2);			/* did not find a tag match; miss */
 }

 else {				/* larger cache */
   temp = *set_ind;
   for ( i = 0; i < captr->setsz; i++) /* loop through set looking for a match */
     {
       i1 = temp /SUB_SZ;
       i2 = temp % SUB_SZ;
       if ( captr->data[i1][i2].tag == *tag) /* a match */
         if (captr->data[i1][i2].state.st != INVALID)
           {
             *set_ind = temp;
             return(0);		/* found a hit */
           }
         else
           {
             *set_ind = temp;
             return(1);		/* found a present miss; state is INVALID */
           }
       temp++;
     }
   return(2);			/* did not find a tag match; miss */
 }

}

/*****************************************************************************/
/* hit_update: updates the cache ages in the set to indicate a hit on a      */
/* reference; age is not maintained for an infinite cache because there      */
/* will be no replacement                                                    */
/*****************************************************************************/

void hit_update (unsigned index, CACHE *captr, int set, REQ *req)
{
 unsigned temp, i, tage;
 int i1, i2;

 if (captr->types.replacement == LRU && captr->size != INFINITE)
  {				/* update ages if LRU replacement and not infinite cache */
				/* Infinite caches will not have to worry about replacement */
    if (captr->num_lines <= SUB_SZ) { /* if small cache */
      temp = set *captr->setsz;
      tage = captr->data[0][index].age;	/* age of line hit */
      for ( i = 0; i < captr->setsz; i ++)
        if ( captr->data[0][temp].age < tage) /* ages less than that hit are increased by one  */
          (captr->data[0][temp++].age)++;
        else
          if( temp == index)
	    {
	      if (captr->data[0][temp].pref && !req->s.prefetch)
		{
		  captr->data[0][temp].pref = 0; /* it's now been demand fetched! */
		  captr->stat.pref_useful++;
		  captr->data[0][temp].pref_tag_repl = -1; /* now the blame for replacing a line goes to the demand access, not the prefetch */
		  /* now also account for the earliness factor */
		  StatrecUpdate(captr->pref_earlyness,(double)(YS__Simtime-captr->data[0][temp].pref_time),1.0);
		}
              captr->data[0][temp++].age = 1; /* age of line hit is made 1 */
	    }
          else
            temp++;
    }
    else {			/* if num lines is geater than SUB_SZ */
      temp = set;
      i1 = (index)/SUB_SZ;
      i2 = (index)%SUB_SZ;
      tage = captr->data[i1][i2].age; /* age of line hit */
      for ( i = 0; i < captr->setsz; i ++) {
        i1 = (temp)/SUB_SZ;
        i2 = (temp)%SUB_SZ;
        if ( captr->data[i1][i2].age < tage) {
          (captr->data[i1][i2].age)++; /* ages less than that hit are increased by one  */
          temp ++;
        }
        else
          if( temp == index)
            {
	      if (captr->data[i1][i2].pref && !req->s.prefetch)
		{
		  captr->data[i1][i2].pref = 0; /* it's now been demand fetched! */
		  captr->stat.pref_useful++;
		  captr->data[i1][i2].pref_tag_repl = -1; /* now the blame for replacing a line goes to the demand access, not the prefetch */
		  /* now also account for the earliness factor */
		  StatrecUpdate(captr->pref_earlyness,(double)(YS__Simtime-captr->data[0][temp].pref_time),1.0);
		}
              captr->data[i1][i2].age = 1; /* age of hit line is made 1 */
              temp ++;
            }
          else
            temp++;
      }
    }
  }
}

/*****************************************************************************/
/* premiss_ageupdate: Updates cache age on a present miss                    */
/*****************************************************************************/

void premiss_ageupdate (CACHE *captr, int set_ind, REQ *req, int nofetchcoal)
{
  int  temp, i;
  int inv_age;
  int i1, i2;

  if (captr->types.replacement == LRU && captr->size != INFINITE)
  {				/* update ages if LRU replacement and not infinite cache */
				/* Infinite caches will not have to worry about replacement */
    if (captr->num_lines <= SUB_SZ) { /* if small cache */
      inv_age = captr->data[0][set_ind].age; /* age of line that had a present miss */

      temp = (int)(set_ind/captr->setsz)*(captr->setsz);
      for ( i = 0; i < captr->setsz; i++)
        if (captr->data[0][temp].age < inv_age)/* ages less than that of pre-miss line are increased by one  */
          (captr->data[0][temp++].age)++;
        else
          if ( temp == set_ind ) /* captr->data[0][temp].age == inv_age) */
            {
	      if (req->s.prefetch && nofetchcoal) /* only prefetches present --
						     no demand fetches yet */
		{		  
		  captr->data[0][temp].pref=1;
		  captr->data[0][temp].pref_time=YS__Simtime; /* used to account for earliness */
		}
	      else
		captr->data[0][temp].pref=0; /* not a "prefetch" line --
						either only demand access or
						a late prefetch */
		
	      captr->data[0][temp].pref_tag_repl = -1; /* this happens regardless, since nothing was replaced */
              captr->data[0][temp++].age = 1; /* age of present miss line is made 1 */
            }
          else
            temp++;
    }

    else {
      i1 = set_ind / SUB_SZ;
      i2 = set_ind % SUB_SZ;
      inv_age = captr->data[i1][i2].age; /* age of line that had a present miss */

      temp = (int)(set_ind/captr->setsz)*(captr->setsz);
      for ( i = 0; i < captr->setsz; i++) {
        i1 = temp / SUB_SZ;
        i2 = temp % SUB_SZ;
        if (captr->data[i1][i2].age < inv_age) {
	  /* ages less than that of pre-miss line are increased by one  */
          (captr->data[i1][i2].age)++;
          temp ++;
        }
        else
          if (temp == set_ind) { 
	      if (req->s.prefetch &&nofetchcoal) /* only a prefetch line */
		{
		  captr->data[i1][i2].pref=1;
		  captr->data[i1][i2].pref_time=YS__Simtime;
		}
	      else
		captr->data[i1][i2].pref=0;
	      captr->data[i1][i2].pref_tag_repl = -1; /* this happens regardless, since nothing was replaced */
              captr->data[i1][i2].age = 1; /* age of present miss line is made 1 */
              temp ++;
            }
          else
            temp++;
      }
    }
  }
}


/*****************************************************************************/
/* miss_ageupdate: This function occurs in the case of a total miss. The     */
/* function must pick a victim and then update the age accordingly. The      */
/* heuristic used here is to pick an INVALID line first. If there are no     */
/* INVALID lines, choose the oldest SH_CL line, then the oldest PR_CL        */
/* line, and finally the oldest PR_DY line. If all lines in the set have     */
/* upgrades outstanding, then this function cannot replace a line and        */
/* thus must return NO_REPLACE.                                              */
/*****************************************************************************/

int miss_ageupdate (REQ *req, CACHE *captr, int *set_ind, long tag, int nofetchcoal)
{
  int  temp, i, temp1;
  int i1, i2, repl_age;
  int victim;

  int bestinv=-1, bestclean=-1, bestprivate=-1, bestdirty=-1;
  
  int inv_age = 0;		/* invalid cache line with the largest age */
  int clean_age = 0;		/* valid clean cache line with the largest age */
  int private_age = 0;          /* valid private clean cache line with largest age */
  int dirty_age = 0;		/* dirty cache line with the largest age */

  int reqtag = (req->address >> captr->non_tag_bits);
  
  if (captr->setsz == 1) {	/* Direct mapped caches */
    temp = *set_ind;
    i1 = (temp)/SUB_SZ;
    i2 = (temp)%SUB_SZ;
    if (captr->data[i1][i2].state.mshr_out && captr->cache_level_type != FIRSTLEVEL_WT)
      /* lines with upgrades outstanding should not be replaced -- in a
	 write through cache, though, we sometimes book MSHRs for excl pref,
	 RMWs, etc, but those can be victimzed ... */
      {
	return NO_REPLACE;
      }
    
    /* if the line being brought in is a line that was recently replaced
       by a prefetch to a different line in the same set, that prefetch
       is considered a damaging prefetch */
   if (captr->data[i1][i2].state.st != INVALID && reqtag == captr->data[i1][i2].pref_tag_repl) 
      {
	captr->stat.pref_damaging++;
	captr->data[i1][i2].pref_tag_repl = -1;
      }

   /* If a prefetch is victimized before it can be used, it is counted as useless */
    if (captr->data[i1][i2].pref)
      captr->stat.pref_useless++;

    /* if the incoming line is a prefetch without any demand fetches
       yet, set up prefetch accounting for it */
    if (req->s.prefetch &&nofetchcoal)
      {
	if (captr->data[i1][i2].state.st != INVALID)
	  captr->data[i1][i2].pref_tag_repl = captr->data[i1][i2].tag;
	else
	  captr->data[i1][i2].pref_tag_repl = -1;
	
	captr->data[i1][i2].pref = 1;
	captr->data[i1][i2].pref_time = YS__Simtime;
      }
    else /* otherwise, it's a regular access or a late pf. No PF accounting needed */
      {
	captr->data[i1][i2].pref_tag_repl = -1;
	captr->data[i1][i2].pref = 0;
      }
    return 1;
  }

  if (captr->size != INFINITE) { /* Cache size is not infinite */
    if ( !(captr->types.replacement == LRU || captr->types.replacement == FIFO)) 
      YS__errmsg("miss_ageupdate(): Unknown replacement type");
    
    temp = *set_ind;
    for (i = 0; i < captr->setsz; i++)
      {
	/* In associative caches, this function must find the best
	   choice of lines to replace, according to the heuristic
	   specified above. (prefer INVALID, then SH_CL, then PR_CL,
	   then PR_DY) */
	
	i1 = (temp+i)/SUB_SZ;
	i2 = (temp+i)%SUB_SZ;
	if (!captr->data[i1][i2].state.mshr_out || captr->cache_level_type == FIRSTLEVEL_WT) /* don't look at upgrade lines */
	  {
	    switch (captr->data[i1][i2].state.st)
	      {
	      case INVALID:
		if (inv_age < captr->data[i1][i2].age)
		  {
		    inv_age = captr->data[i1][i2].age;
		    bestinv=i;
		  }
		break;
	      case SH_CL:
		if (clean_age < captr->data[i1][i2].age)
		  {
		    clean_age = captr->data[i1][i2].age;
		    bestclean=i;
		  }
		if (captr->data[i1][i2].pref_tag_repl == reqtag)
		  {
		    captr->data[i1][i2].pref_tag_repl = -1;
		    captr->stat.pref_damaging++;
		  }
		break;
	      case PR_CL: /* this is worse than SH_CL since this might be dirty in upper caches */
		if (private_age < captr->data[i1][i2].age)
		  {
		    private_age = captr->data[i1][i2].age;
		    bestprivate=i;
		  }
		if (captr->data[i1][i2].pref_tag_repl == reqtag)
		  {
		    captr->data[i1][i2].pref_tag_repl = -1;
		    captr->stat.pref_damaging++;
		  }
		break;
	      case PR_DY:
	      case SH_DY:
		if (dirty_age < captr->data[i1][i2].age)
		  {
		    dirty_age = captr->data[i1][i2].age;
		    bestdirty=i;
		  }
		if (captr->data[i1][i2].pref_tag_repl == reqtag)
		  {
		    captr->data[i1][i2].pref_tag_repl = -1;
		    captr->stat.pref_damaging++;
		  }
		break;
	      default:
		YS__errmsg("Unrecognized cacheline state in miss_ageupdate!\n");
		break;
	      }
	  }
      }

    if (bestinv != -1)
      {	
	victim = bestinv;
	repl_age=inv_age;
      }
    else if (bestclean != -1)
      {
	victim = bestclean;
	repl_age=clean_age;
      }
    else if (bestprivate != -1)
      {
	victim = bestprivate;
	repl_age=private_age;
      }
    else if (bestdirty != -1)
      {
	victim = bestdirty;
	repl_age=dirty_age;
      }
    else /* all lines in set have outstanding upgrades! */
      {
	*set_ind = temp;
	return NO_REPLACE;	/* All lines in set are pending */
      }

    *set_ind = temp+victim;
    
    /* having found the victim, update ages */
    for ( i = 0; i < captr->setsz; i++)
      {
	i1 = (temp+i)/SUB_SZ;
	i2 = (temp+i)%SUB_SZ;
	if (captr->data[i1][i2].age < repl_age)
	  {
	    (captr->data[i1][i2].age)++;
	  }
	else if (i == victim)
	  {
	    if (captr->data[i1][i2].pref) /* if the victim was a prefetch, */
	      captr->stat.pref_useless++; /* that prefetch is now useless */
	    if (req->s.prefetch && nofetchcoal) 
	      {
		/* if this access is a prefetch, remember which line (if
		   any) it invalidated so that it can be later determined
		   if this line was a damaging prefetch. */
		if (captr->data[i1][i2].state.st != INVALID)
		  captr->data[i1][i2].pref_tag_repl = captr->data[i1][i2].tag;
		else
		  captr->data[i1][i2].pref_tag_repl = -1;
		
		captr->data[i1][i2].pref = 1;
		captr->data[i1][i2].pref_time = YS__Simtime;
	      }
	    else
	      {
		captr->data[i1][i2].pref_tag_repl = -1;
		captr->data[i1][i2].pref = 0;
	      }

	    captr->data[i1][i2].age = 1;
	  }
      }

    return 1;

  }                             /* end: finite caches */
  else {                        /* Infinite Cache */
    temp = *set_ind;
    temp1 = -1;
    for (i = 0; i < captr->setsz; i++)
      {
        i1 = (temp+i)/SUB_SZ;
        i2 = (temp+i)%SUB_SZ;
        if (captr->data[i1][i2].state.st == INVALID) { 
          temp1 = temp + i;	/* Set to invalid line if any */
          *set_ind = temp1;
          break;
        }
      }
    if (temp1 == -1) {		/* No invalid line in cache */
				/* Increase cache size */
      captr->num_lines += SUB_SZ; 
      if (captr->num_lines > (SUB_SZ*100)) 
	YS__errmsg("miss_ageupdate(): Infinite Cache size exceeded maximum size allocated");

      i1 = (captr->num_lines / SUB_SZ) - 1;
      captr->data[i1] = (Cachest *)malloc(sizeof(Cachest)*SUB_SZ);
      if (!captr->data[i1])
        YS__errmsg("miss_ageupdate(): Malloc failed when extending cache data structure for infinite cache");

      for (i=0; i<SUB_SZ; i++) {
        captr->data[i1][i].tag = -1; /* marked invalid */
        captr->data[i1][i].state.st = INVALID;
      }
      captr->setsz = captr->num_lines;
      *set_ind  = captr->num_lines - SUB_SZ;
    }
    return 1;
  }                             /* end: infinite cache */
}

/*****************************************************************************/
/* cache_get_cohe_type: returns cohe_type and allo_type for a line           */
/*****************************************************************************/
void cache_get_cohe_type (CACHE *captr, int hittype, int set_ind, long address, int *cohe_type, int *allo_type, int *node)
{
  int x, i1, i2;

  if (captr->types.adap_type == 1 ) {
    if (hittype == 2) {	/* if miss */
       LookupAddrCohe(address, &x);
      if (x != -1) {
	*cohe_type = (x >> cohe_sft) & cohe_mask ;
	*allo_type = (x >> all_sft) & all_mask ;
      }
      else {
	*cohe_type = captr->types.cohe_type;
	*allo_type = captr->types.allo_type;
      }
    }
    else  {			/* if hit */
      i1 = set_ind/SUB_SZ;
      i2 = set_ind % SUB_SZ;
      *cohe_type = captr->data[i1][i2].state.cohe_type;
      *allo_type = captr->data[i1][i2].state.allo_type;
    }	
  }
  else {			/* if not adaptive */
    *cohe_type = captr->types.cohe_type;
    *allo_type = captr->types.allo_type;
  }

  if(*cohe_type != PR_WT && *cohe_type != PR_WB && *cohe_type != WB_NREF && *cohe_type != NC)
    {
      YS__errmsg("Access with unknown coherence type\n");
    }

  if (hittype == 2) {
    extern int YS__NumNodes; /* from mainsim.cc */
    if (YS__NumNodes == 1)
      *node = 0; /* no need to even check, or, for that matter to give any warning if non-assoced */
    else
      {
	LookupAddrNode(address, node); 
	if (*node == NLISTED) {
	  extern int MemWarnings;
	  /* assign an non-allocated addr to the first requestor node */
	  address = (address >> captr->block_bits) << captr->block_bits;
	  AssociateAddrNode((unsigned)address, address + (1<<captr->block_bits) - 1,
			    captr->node_num, "first touch");
	  *node = captr->node_num;
	  if (MemWarnings){
	    fprintf(simerr,"WARNING -- FAILURE TO ASSOCIATE ADDRESS %ld! RSIM address %ld (now %d)\n",
		    address, address+PROC_TO_MEMSYS, *node);
	  }
	  
	}
      }
  }
  else {
    i1 = set_ind/SUB_SZ; i2 = set_ind % SUB_SZ;
    *node = captr->data[i1][i2].dest_node;
  }

}

/*****************************************************************************/
/* GetReplReq: provides a replacement request of the specified type (INVL or */
/* WRB, etc). If the direction is above, this is a COHE. Otherwise, it is a  */
/* COHE_REPLY (write-backs are COHE_REPLYs in this system).                  */
/*****************************************************************************/

REQ *GetReplReq(CACHE *captr, REQ *req, int tag, ReqType req_type, 
		int req_sz, int rep_sz, int cohe_type, int allo_type,
		int src_node, int dest_node, int route)
{
  REQ *req_ret;

  /* Allocate a new request */
  req_ret = (REQ *)YS__PoolGetObj(&YS__ReqPool);
  req_ret->id = req->id; /* copy the id */
  req_ret->priority = 0;
  req_ret->in_port_num = req->in_port_num;
  req_ret->address = (tag)<<captr->block_bits; /* make an address for the REQ */
  req_ret->tag = tag; /* set the tag as specified */
  req_ret->req_type = req_type; /* use the specified ReqType */
  req_ret->address_type = DATA; 
  req_ret->dubref = 0;

  /* use the req_sz and rep_sz as specified */
  if (req_sz != NOCHANGE)
    req_ret->size_st = req_sz;  
  if (rep_sz != NOCHANGE)
    req_ret->size_req = rep_sz;
 
  req_ret->s.dir = REQ_FWD;
  if (route == ABV){ /* above replacements are COHEs */
    req_ret->s.ret = RET;
    req_ret->s.type = COHE; 
  }
  else {
    /* below replacements are WRBs/REPLs -- sent as COHE_REPLYs */
    req_ret->s.ret = RET;
    req_ret->s.type = COHE_REPLY;
    req_ret->s.reply = REPLY;
  }

  /* Set route of message */
  req_ret->s.route = route;
  req_ret->src_node = src_node;
  req_ret->dest_node = dest_node;

  /* Set up cohe_type, allo_type, linesz as needed */
  req_ret->cohe_type = cohe_type;
  req_ret->allo_type = allo_type;
  req_ret->linesz = captr->linesz;

  req_ret->s.nack_st = NACK_OK; /* It's ok to NACK this */

  req_ret->s.prefetch=0;
  req_ret->s.prclwrb = 0; /* By default, assume that this is not a WRB
			     sent on a PRCL line at the L2 in the hope
			     that L1 has it dirty; let that be set
			     explicitly in its case */
  return(req_ret);
}

