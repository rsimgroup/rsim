/*
  pool.c

  Provides essential functions for faster memory allocation of small
  objects used throughout the memory system simulator.
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
#include "MemSys/cpu.h"
#include "MemSys/tr.pool.h"
#include "MemSys/req.h"
#include "MemSys/net.h"
#include "Processor/simio.h"
#include <malloc.h>

#include <string.h>

/*****************************************************************************/
/* POOL Operations: used to manage the allocation of memory used for         */
/* object descriptors.  These reduce the number of calls to malloc by        */
/* maintaining a list of descriptors that can be allocated for new           */
/* objects and then returned to the pool for reuse when the object is        */
/* deleted or the simulation reset.  Pools use malloc to obtain large        */
/* blocks of memory consisting of several objects and then parcels           */
/* them out in response to the "new" object operation.  Pools never          */
/* return memory to the system, so the size of the pool only increases.      */
/* In order to use a pool, the first two elements of each structure must be  */
/* char pointers "pnxt" and "pfnxt", which maintain the pool lists           */
/*****************************************************************************/

/*****************************************************************************/
/* YS__PoolInit: start out a new pool of objects, specifying the number to   */
/* allocate with each increment and the size of each object                  */
/*****************************************************************************/

void YS__PoolInit(pptr,name,objs,objsz)  /* Initialize a pool                */
POOL *pptr;                              /* Pointer to the pool              */
char *name;                              /* User defined name for the pool   */
int  objs;                               /* Number of objects to malloc      */
int  objsz;                              /* Size of each object in bytes     */
{
   pptr->p_head = NULL;                  /* Points to allocated objects      */
   pptr->p_tail = NULL;                  /* Points to the tail of the pool   */
   pptr->pf_head = NULL;                 /* Points to unallocated objects    */
   pptr->pf_tail = NULL;                 /* Points to unallocated objects    */
   pptr->objects = objs;                 /* Number of objects to allocate with
					    each increment                   */
   pptr->objsize = objsz;                /* Size of each object              */
   pptr->newed = pptr->killed = 0;       /* Clear out these allocation stats */
   strncpy(pptr->name,name,31);          /* copy the name in                 */
   pptr->name[31] = '\0';
}

/*****************************************************************************/
/* YS__PoolStats: print out allocation stats, for debugging                  */
/*****************************************************************************/
void YS__PoolStats(POOL *pptr) 
{
#ifdef DEBUG_POOL
  fprintf(simout,"Pool %s stats: newed %d killed %d\n",pptr->name,pptr->newed,pptr->killed);
#endif
}

/*****************************************************************************/
/* YS__PoolGetObj: Return a pointer to an object from the pool. If there are */
/* no free objects at the time, allocate a new chunk of objects and set      */
/* the pool fields for them. Zero them out also. Initialize some fields for  */
/* REQ data structures additionally.                                         */
/*****************************************************************************/
char *YS__PoolGetObj(pptr)       /* Get an object from the pool              */
POOL *pptr;                      /* Pointer to the pool                      */
{
   char *ptr;

   pptr->newed++;
#ifdef POOL_AS_MALLOC
   ptr = (char *)malloc(pptr->objsize);
   if (ptr == NULL) 
     YS__errmsg("Malloc fails in PoolGetObj");
   memset(ptr,0,pptr->objsize);
   /* no need to assign pnext and pfnext */
#else /* Regular pool operation */
   
   TRACE_POOL_getobj1;           /* Getting object from pool                 */
   if (pptr->pf_head == NULL) {  /* No unallocated objects in the pool       */
     int i;
     TRACE_POOL_getobj2;        /* Pool gets new block from system          */
     ptr = (char*)malloc((pptr->objects)*(pptr->objsize)); /* Get a block of objects */
     
     if (ptr == NULL) 
       YS__errmsg("Malloc fails in PoolGetObj");
     memset(ptr,0,(pptr->objects)*(pptr->objsize));
     
     for(i = 0; i<(pptr->objects)-1; i++) { /* Link together the new objects*/
       *((char**)(ptr+i*(pptr->objsize))) = ptr+(i+1)*(pptr->objsize); /* Setting up pfnxt */
       *((char**)(ptr+i*(pptr->objsize) + sizeof(char *))) = ptr+(i+1)*(pptr->objsize); /* Setting up pnxt */
     }
     
     if (pptr->p_tail == NULL) { /* The pool is empty, this is first call to GetObj  */
       pptr->p_head = ptr;
     }      
     else {                 /* Add the new objects at the tail of the pool */
       *((char**)(pptr->p_tail)) = ptr;
     } 
     pptr->p_tail = ptr + (pptr->objects - 1)*(pptr->objsize); /* Adjust tail pointer  */
     *((char**)(pptr->p_tail)) = NULL;         /* Last object has no next object  */
     
     pptr->pf_head = ptr;
     pptr->pf_tail = ptr + (pptr->objects - 1)*(pptr->objsize); /* Adjust tail pointer  */
     *((char**)(pptr->pf_tail+sizeof(char *))) = NULL;         /* Last object has no next object */
   }
   ptr = pptr->pf_head;                        /* Get the next free object             */
   pptr->pf_head = *((char**)(pptr->pf_head + sizeof(char *)));  /* Shift the middle to the right */
#endif
   if (pptr == &YS__ReqPool)
     {
       REQ *rptr=(REQ *)ptr;
       if (rptr->inuse)
	 {
	   YS__errmsg("Getting an already gotten request from pool!\n");
	 }
       rptr->next = NULL; /* Clear out this field */
       rptr->inuse=1; /* This request is now in use */
       rptr->forward_to = -1; /* Don't consider this access a forward
				 unless it is explicitly made into one */
     }
   return ptr;   
}

/*****************************************************************************/
/* YS__PoolReturnObj: Put an object back into its pool                       */
/*****************************************************************************/

void YS__PoolReturnObj(POOL *pptr,void *optr)       /* Return an object to
						       its pool             */
{
   pptr->killed++;
   if (pptr == &YS__ReqPool)
     {
       REQ *rptr = (REQ *)optr;
       
       if (rptr->inuse == 0)
	 {
	   YS__errmsg("Repooling a previously pooled request!\n");
	 }

       memset((char *)optr + sizeof(optr)*2, '\0', pptr->objsize - sizeof(optr)*2); 
       rptr->inuse=0;
       rptr->forward_to = -1;       
     }
   else
     {
       memset((char *)optr + sizeof(optr)*2, '\0', pptr->objsize - sizeof(optr)*2);
     }

#ifdef POOL_AS_MALLOC
   free(optr);
#else
   TRACE_POOL_retobj;                    /* Returning object to pool                  */
   
   if (pptr->pf_tail) {
     *((char **) (pptr->pf_tail+sizeof(char *))) = optr;
     *((char **) ((char *)optr+sizeof(char *))) = NULL;
     pptr->pf_tail = optr;
   }
   else {
     pptr->pf_head = optr;
     pptr->pf_tail = optr;
     *((char **) ((char *)optr+sizeof(char *))) = NULL;
   }
#endif

}

/*****************************************************************************/
/* YS__PoolReset: Deallocate and clear all objects in the pool.              */
/*****************************************************************************/

void YS__PoolReset(pptr)     /* Deallocates & clears all objects in the pool */
POOL *pptr;                  /* Pointer to the pool                          */
{
   char *ptr;
   int i;

#ifdef DEBUG_POOL
   fprintf(simout,"Pool %s at reset: newed %d killed %d\n",pptr->name,pptr->newed,pptr->killed);
#endif
   pptr->newed=pptr->killed=0;

   for (ptr = pptr->p_head; ptr != NULL; ptr = *((char **)(ptr + sizeof(char *)))) {
     if (*((char **)ptr) == NULL) /* Not part of free pool items queue */
       {
	 for (i= sizeof(char *)*2; i< pptr->objsize; i++) *(ptr+i) = '\0';
       }
     *((char **)(ptr+sizeof(char *))) = ptr + pptr->objsize; /* Putting all objects in one free pool */
   }
   pptr->pf_head = pptr->p_head;
   pptr->pf_tail = pptr->p_tail;
 }

