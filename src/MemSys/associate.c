/*
  associate.c

  Responsible for explicitly assigning regions of the shared address
  space to specific home nodes (or partitioning it out implicitly
  with a first-touch policy). Note that both the explicit association
  and first-touch policy are on a cache-line granularity.
  
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
#include "MemSys/net.h"
#include "MemSys/directory.h"
#include "MemSys/misc.h"
#include "MemSys/associate.h"
#include "Processor/simio.h"
#include <malloc.h>
#include <string.h>

struct AssAddr { /* AssociateAddr structure */
  int start; /* starting line */
  int end;   /* ending line */
  int node;  /* home node */
  int cohe;  /* coherence type */
  char name[32]; /* name of block -- for debugging */
  struct AssAddr *left;  /* left and right children in tree */
  struct AssAddr *right;
};
typedef struct AssAddr LITEM;

#define ASS_NODE 1
#define ASS_COHE 2

LITEM *node_root;
LITEM *cohe_root;
void InsertList(LITEM *,int);
static void PrintListNode(int);
static void PrintListCohe(void);
static void Delete(LITEM *,int,int);

/*****************************************************************************/
/* AssociateAddrNode: Exposed to RSIM user via a system trap. Assigns        */
/* home node for a region of memory                                          */
/*****************************************************************************/
void AssociateAddrNode(start, end, node, name)
unsigned start;
unsigned end;
int  node;
char *name;
{
  LITEM  *lptr;
  end=end-1; /* we don't want end to be inclusive */

  /* Start by calculating cache lines corresponding to these addresses */
  
  start = start >> block_bits;
  end = end >> block_bits;
  if (end < start) {
#ifdef DEBUG_ASSOC
    fprintf(simout,"\n ERROR\n");
    fprintf(simout,"AssociateAddrNode(): Rangename: %s\tRange 0x%x to 0x%x assigned to node: %d\n", 
	   name, start, end, node);
    fprintf(simout,"AssociateAddrNode(): end is less than start  of range\n");
#endif
    exit(-1);
  }

  lptr = (LITEM *)malloc(sizeof(LITEM));
  if (lptr == NULL) {
    fprintf(simerr, "AssociateAddr(): malloc failed \n");
    exit(-1);
  }

  /* Set up all appropriate fields in structure */
  lptr->start = start;
  lptr->end = end;
  lptr->node = node;
  lptr->cohe = -1;
  strncpy(lptr->name,name, 31); 
  lptr->left = NULL;
  lptr->right = NULL;

  InsertList(lptr, ASS_NODE); /* insert it into tree */
}

/* Lookup home node for a given address */
void LookupAddrNode(addr, node)
unsigned addr;
int *node;
{
  LITEM *p;

  addr = addr >> block_bits; /* find the tag for this address */

  p = node_root;
  /* Do an preorder search of the tree looking for the desired range.
     If found, note the node number. If not found, return NLISTED to
     indicate that this node hasn't yet been explicitly associated */
     
  while(p != NULL) {
    if (p->start <= addr && p->end >= addr) {
      *node = p->node;
      break;
    }
    else if (addr < p->start)
      p = p->left;
    else
      p = p->right;
  }
  if (p == NULL) 
    *node = NLISTED;
}
    
/*****************************************************************************/
/* InsertList: maintains the tree of AssociateAddrNode and AssociateAddrCohe */
/*****************************************************************************/

void InsertList(lnew, type)
LITEM *lnew;
int type;
{
  LITEM *p, *x;
  int left;
  int same;
  LITEM *lsplit;

  x = NULL;
  left = 0;

  if (type == ASS_NODE)
    p = node_root;
  else if(type == ASS_COHE)
    p = cohe_root;
  else 
    YS__errmsg("InsertList(): Unknown type");

  /* Ranges are inserted into the tree according to their addresses. If
     the range being inserted overlaps with another range for the same node,
     the regions can be merged. However, if the range being
     inserted overlaps with a range held at a different node, the regions
     that are already assigned should remain as is, while the new region
     should be split into two portions, each of which is recursively
     inserted with this function. */

  while (p != NULL) {
    if (type == ASS_NODE)
      same = (lnew->node == p->node) ? 1:0;
    else
      same = (lnew->cohe == p->cohe) ? 1:0;
    /* same is set to 1 if both items associated with same node */
    if (lnew->start < p->start) 
      {				/* start < start; insert left in binary tree */
	if (lnew->end > p->start) 
	  if (!same) /* Overlapping ranges associated with different nodes */
	  {
#ifdef DEBUG_ASSOC
	    fprintf(simout,"\n WARNING: WILL FIX THE REGIONS ACCORDINGLY\n");
	    fprintf(simout,"AssocAddr(): Overlapping ranges assigned to different nodes\n");
	    fprintf(simout,"Range name: %s\tRange 0x%x to 0x%x assigned to node: %d\n",
		    p->name, p->start<<block_bits, p->end<<block_bits, p->node);
	    fprintf(simout,"Range name: %s\tRange 0x%x to 0x%x assigned to node: %d\n",
		    lnew->name, lnew->start<<block_bits, lnew->end<<block_bits, lnew->node);
	    fprintf(simout,"\n\n");
#endif
	    if (lnew->end <= p->end)
	      {
		lnew->end = p->start - 1;
		InsertList(lnew, type);
		return;
	      }
	    else
	      { /* we have to divide this into 2 entries */
		lsplit = (LITEM *)malloc(sizeof(LITEM));
		if (lsplit == NULL) {
		  fprintf(simerr, "InsertItem(): malloc failed \n");
		  exit(-1);
		}
		lsplit->start = p->end + 1;
		lsplit->end = lnew->end;
		lsplit->node = lnew->node;
		lsplit->cohe = -1;
		strncpy(lsplit->name,lnew->name, 31); 
		lsplit->left = NULL;
		lsplit->right = NULL;
		InsertList(lsplit, type);

		lnew->end = p->start - 1;
		InsertList(lnew,type);
		return;
	      }
	    
	  }
	  else
	  {			/* Same nodes; adjacent or overlapping ranges; coalesce entries */
	    YS__warnmsg("AssociateAddr: Coalescing overlapping entries");
#ifdef DEBUG_ASSOC
	    fprintf(simout,"Warning Info\tName %s will not appear in list\n", p->name);
	    fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",p->name, p->start<<block_bits, p->end<<block_bits, p->node);
	    fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",lnew->name, lnew->start<<block_bits, lnew->end<<block_bits, lnew->node); 
#endif
	    
	    lnew->end = (lnew->end >= p->end) ? lnew->end: p->end;
				/* lnew->end is set to greater of the two end values */
	    Delete(x,left, type);
	    free(p);
	    InsertList(lnew, type);
	    return;
	  }
	else if ( lnew->end == p->start)
	  {			/* Overlapping ranges; but change limits instead */
	    YS__warnmsg("AssociateAddr: Changing limits of one entry because it overlaps with another");
#ifdef DEBUG_ASSOC
   	    fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",lnew->name, lnew->start<<block_bits, lnew->end<<block_bits, lnew->node);
	    fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n", p->name, p->start<<block_bits, p->end<<block_bits, p->node);
#endif
	    
	    lnew->end --;	/* change limits and insert in list */
	  }      

	if (p->left != NULL) {	/* all other cases are attempted to insert in list */
	  x = p;
	  left = 1;
	  p = p->left;
	}
	else {			/* No more left entries; insert in list */
	  p->left = lnew;
	  return;
	}	    
      }

    else if (lnew->start > p->start) 
      {				/* start > start; insert right in binary tree */
	if ( p->end > lnew->start)
	  if (!same)
	    { /* Overlapping entries associated with different nodes; error */
#ifdef DEBUG_ASSOC	      
	      fprintf(simout,"\n WARNING: WILL FIX THIS ACCORDINGLY\n");
	      fprintf(simout,"AssocAddr(): Overlapping ranges assigned to different nodes\n");
	      fprintf(simout,"Range name: %s\tRange 0x%x to 0x%x assigned to node: %d\n",
		      p->name, p->start<<block_bits, p->end<<block_bits, p->node);
	      fprintf(simout,"Range name: %s\tRange 0x%x to 0x%x assigned to node: %d\n",
		      lnew->name,lnew->start<<block_bits,lnew->end<<block_bits,lnew->node);  
	      fprintf(simout,"\n\n");
#endif
	      if (lnew->end <= p->end) /* no need to insert this */
		{
		  free(lnew);
		  return;
		}
	      else
		{
		  lnew->start = p->end + 1;
		  InsertList(lnew, type);
		  return;
		}
	    }
	  else
	    {			/* Same nodes; adjacent or overlapping ranges; coalesce entries */
	      YS__warnmsg("AssociateAddr: Coalescing adjacent or overlapping entries asscoiated with same node");
#ifdef DEBUG_ASSOC
	      fprintf(simout,"Warning Info\tName %s will not appear in list\n", lnew->name);
	      fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",lnew->name, lnew->start<<block_bits, lnew->end<<block_bits, lnew->node);		   
	      fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",p->name, p->start<<block_bits, p->end<<block_bits, p->node);
#endif	    
	      Delete(x,left, type);
	      p->end = (p->end >= lnew->end)? p->end : lnew->end;
	      free(lnew);
	      p->left = NULL;
	      p->right = NULL;
	      InsertList(p, type);
	      return;
	    }
	
	else if ( p->end == lnew->start)
	  {			/* Overlapping entries; change limits */
	    YS__warnmsg("AssociateAddr: Changing limits of one entry because it overlaps with another"); 
#ifdef DEBUG_ASSOC
	    fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",p->name, p->start<<block_bits, p->end<<block_bits, p->node);		   
	    fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",lnew->name, lnew->start<<block_bits, lnew->end<<block_bits, lnew->node);		   
#endif
	    
	    p->end --;		/* change limits of p; insert lnew in list unchanged */
	  }
	
	if (p->right != NULL){	/* all other cases are attempted to insert in list */
	  x = p;
	  left = 2;		/* right */
	  p = p->right;
	}
	else {			/* No more right entries; insert in list */
	  p->right = lnew;
	  return;
	}
      }

    else if (lnew->start == p->start) 
      {				/* start = start */
	if (lnew->start < lnew->end && p->start < p->end)
	  if (same)
	    {	       	/* overlapping ranges associated with same node; coalesce entries */
	      YS__warnmsg("AssociateAddr: Coalescing 2 overlapping entries asscoiated with same node");
#ifdef DEBUG_ASSOC
	      fprintf(simout,"Warning Info\tName %s will not appear in list\n", lnew->name);
	      fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",lnew->name, lnew->start<<block_bits, lnew->end<<block_bits, lnew->node);		   
	      fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",p->name, p->start<<block_bits, p->end<<block_bits, p->node);
#endif
	      
	      Delete(x,left, type);
	      p->end = (p->end >= lnew->end)? p->end : lnew->end;
	      free(lnew);
	      p->left = NULL;
	      p->right = NULL;
	      InsertList(p, type);
	      return;
	    }		
	  else 
	    {			/* overlapping ranges associated with different nodes */
				/* Both entries not single tag */
#ifdef DEBUG_ASSOC	      
	      fprintf(simout,"\n WARNING: WILL FIX THIS ACCORDINGLY\n");
	      fprintf(simout,"AssocAddr(): Overlapping ranges assigned to different nodes\n");
	      fprintf(simout,"Range name: %s\tRange 0x%x to 0x%x assigned node: %d\n",
		      p->name,p->start<<block_bits, p->end<<block_bits, p->node);
	      fprintf(simout,"Range name: %s\tRange 0x%x to 0x%x assigned node: %d\n",
		      lnew->name,lnew->start<<block_bits,lnew->end<<block_bits,lnew->node);
	      fprintf(simout,"\n\n");
#endif
	      if (p->end > lnew->end)
		{
		  free(lnew);
		  return;
		}
	      else
		{
		  lnew->start = p->end + 1;
		  InsertList(lnew, type);
		  return;
		}
	    }
	else if (lnew->start == lnew->end && p->start == p->end) 
	  {			/* Both single tag entries */
	    YS__warnmsg("AssociateAddr: Two single tag entries; One of them will not appear in list");
#ifdef DEBUG_ASSOC
	    fprintf(simout,"Warning Info\t Name %s will not appear in list\n",lnew->name);
	    fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",lnew->name, lnew->start<<block_bits, lnew->end<<block_bits, lnew->node);
	    fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",p->name, p->start<<block_bits, p->end<<block_bits, p->node);
#endif
	    free (lnew);
	    return;
	  }
	else 
	{			/* Only one of the entries is single */
	    YS__warnmsg("AssociateAddr: Changing limits of one entry because it overlaps with another");
	    
#ifdef DEBUG_ASSOC
	       fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",p->name, p->start<<block_bits, p->end<<block_bits, p->node);
	       fprintf(simout,"Warning Info\tname: %s\t start: 0x%x\t end: 0x%x\t node: %d\n",lnew->name, lnew->start<<block_bits, lnew->end<<block_bits, lnew->node);
#endif
	    
	    if (lnew->start == lnew->end)
	    {
		Delete(x, left, type);
		p->start ++;
		p->left = NULL;
		p->right = NULL;
		InsertList(p, type);
		InsertList(lnew, type);
		return;
	    }
	    else {
		lnew->start ++;	/* Go back to while loop; now start < start */
	    }
	  }
      }
  }
  
  if (type == ASS_NODE) {
    if (node_root == NULL)
      node_root = lnew;
  }
  else if (cohe_root == NULL)
      cohe_root = lnew;

}

/* Deletes items from the binary tree; used for merging */
static void Delete(x, left, type)
LITEM *x;
int left, type;
{
  LITEM *p, *q;


  if (left == 1)
    p = x->left;
  else if (left == 2)
    p = x->right;
  else
    if (type == ASS_NODE)
      p = node_root;
    else if(type == ASS_COHE)
      p = cohe_root;
    else 
      YS__errmsg("Delete(): Unknown type");


  if (p == NULL) {
    fprintf(simerr,"\n ERROR\n");
    fprintf(simerr,"AssociateAddr(): Trying to delete a non-existant entry\n\n\n");
    if (type == ASS_NODE)
      PrintListNode(1);
    else if(type == ASS_COHE)
      PrintListCohe();
    exit(-1);
  }
  
  if (p->right == NULL) {
    if (left == 1)
      x->left = p->left;
    else if (left == 2)
      x->right = p->left;
    else 
      if (type == ASS_NODE)
	node_root = p->left;
      else 
	cohe_root = p->left;
  }
  else if (p->left == NULL) {
    if (left == 1)
      x->left = p->right;
    else if (left == 2)
      x->right = p->right;
    else 
      if (type == ASS_NODE)
	node_root = p->right;
      else 
	cohe_root = p->right;
  }
  else {
    q = p->right;
    while(q->left != NULL) q = q->left;
    q->left = p->left;
    if (left == 1)
      x->left = p->right;
    else if (left == 2)
      x->right = p->right;
    else 
      if (type == ASS_NODE)
	node_root = p->right;
      else 
	cohe_root = p->right;
  }
}
    
/*****************************************************************************/
/* AssociateAddrCohe: Assigns a coherence type for a region of memory        */
/*****************************************************************************/
void AssociateAddrCohe(start, end, cohe, name)
unsigned start;
unsigned end;
int  cohe;
char *name;
{
  LITEM  *lptr;

  /* start out by finding cache line numbers */
  start = start >> block_bits;
  end = end >> block_bits;
  if (end < start) {
#ifdef DEBUG_ASSOC
    fprintf(simout,"\n ERROR\n");
    fprintf(simout,"AssociateAddrCohe(): Rangename: %s\tRange 0x%x to 0x%x assigned coherence: %d\n", 
	   name, start, end, cohe);
    fprintf(simout,"AssociateAddrCohe(): end is less than start  of range\n");
#endif
    exit(-1);
  }

  lptr = (LITEM *)malloc(sizeof(LITEM));
  if (lptr == NULL) {
    fprintf( simerr, "AssociateAddr(): malloc failed \n");
    exit(-1);
  }

  /* then set up all fields for structure */
  lptr->start = start;
  lptr->end = end;
  lptr->node = -1;
  lptr->cohe = cohe;
  strncpy(lptr->name,name, 31); 
  lptr->left = NULL;
  lptr->right = NULL;

  InsertList(lptr, ASS_COHE); /* insert into tree of associates */
}

/* Lookup the cohe_type of an address */
void LookupAddrCohe(addr, cohe)
unsigned addr;
int *cohe;
{
  LITEM *p;

  /* start by finding tag */
  addr = addr >> block_bits;

  p = cohe_root;
  /* do an in-order walk of tree looking for desired region of memory */
  while(p != NULL) {
    if (p->start <= addr && p->end >= addr) {
      *cohe = p->cohe;
      break;
    }
    else if (addr < p->start)
      p = p->left;
    else
      p = p->right;
  }
  if (p == NULL) 
    *cohe = -1;
}
    
/*****************************************************************************/
/* Printing functions  for debugging                                         */
/*****************************************************************************/

void InorderNodeHex(p)
LITEM *p;

{
#ifdef DEBUG_ASSOC
  if (p) {
    InorderNodeHex(p->left);
    fprintf(simout,"start: 0x%x\t end: 0x%x\tnode: %d\tname: %s\n",
	   p->start<< block_bits, (p->end<<block_bits)+blocksize-4, p->node, p->name);
    InorderNodeHex(p->right);
  }
#endif
}

void InorderNodeDec(p)
LITEM *p;

{
#ifdef DEBUG_ASSOC
  if (p) {
    InorderNodeDec(p->left);
    fprintf(simout,"start: %d\t end: %d\tnode: %d\tname: %s\n",
	   p->start<< block_bits, (p->end<<block_bits)+blocksize-4, p->node, p->name);
    InorderNodeDec(p->right);
  }
#endif
}

void PrintListNode(hex)
     int hex;
{
#ifdef DEBUG_ASSOC
  fprintf(simout,"\n");
  if (hex)
    InorderNodeHex(node_root);
  else 
    InorderNodeDec(node_root);
#endif
}

void InorderNodeTagHex(p)
LITEM *p;

{
#ifdef DEBUG_ASSOC
  if (p) {
    InorderNodeTagHex(p->left);
    fprintf(simout,"start: 0x%x\t end: 0x%x\tnode: %d\tname: %s\n",
	   p->start, p->end, p->node, p->name);
    InorderNodeTagHex(p->right);
  }
#endif
}

void InorderNodeTagDec(p)
LITEM *p;

{
#ifdef DEBUG_ASSOC
  if (p) {
    InorderNodeTagDec(p->left);
    fprintf(simout,"start: %d\t end: %d\tnode: %d\tname: %s\n",
	   p->start, p->end, p->node, p->name);
    InorderNodeTagDec(p->right);
  }
#endif
}

void PrintListNodeTag(hex)
     int hex;
{
#ifdef DEBUG_ASSOC
  fprintf(simout,"\n");
  if (hex)
    InorderNodeTagHex(node_root);
  else 
    InorderNodeTagDec(node_root);
#endif
}

/**************************************************************************************/

void InorderCohe(p)
LITEM *p;

{
} 

void PrintListCohe()
{
#ifdef DEBUG_ASSOC
  fprintf(simout,"\n");
  InorderCohe(cohe_root);
#endif
}

/**************************************************************************************/

void Inorder_node(p, node)
LITEM *p;
int node;
{
#ifdef DEBUG_ASSOC
  if (p) {
    Inorder_node(p->left, node);
#ifdef DEBUG_ASSOC
    if (p->node == node) 
      fprintf(simout,"start: 0x%x\t end: 0x%x\tnode: %d\tname: %s\n",
	     p->start<< block_bits, p->end<<block_bits, p->node, p->name);
#endif
    Inorder_node(p->right, node);
  }
#endif
}

void PrintNodeAddr(node)
int node;
{
#ifdef DEBUG_ASSOC
  fprintf(simout,"\n");
  Inorder_node (node_root, node);
#endif
}

/**************************************************************************************/

LITEM *node_addr[100][100];
int node_index[100];

void Inorder_allnode(p)
LITEM *p;
{
  if (p) {
    Inorder_allnode(p->left);
    if (node_index[p->node] < 100)
      node_addr[p->node][node_index[p->node]++] = p;
    Inorder_allnode(p->right);
  }
}

void PrintAllNodes(num_nodes)
int num_nodes;
{
  int i, j;
  LITEM *p;

  if (num_nodes > 100)
    YS__warnmsg("PrintAllNodes(): Number of nodes over 100; Will print addresses of first 100 nodes");
  for (i=0; i<100; i++) 
    node_index[i] = 0;

  Inorder_allnode (node_root);

  for (i=0; i<num_nodes; i++) {
    fprintf(simout,"\n");
    for (j=0; j<node_index[i]; j++)  {
      p = node_addr[i][j];
      fprintf(simout,"start: 0x%x\t end: 0x%x\tnode: %d\tname: %s\n",
	     p->start<< block_bits, p->end<<block_bits, p->node, p->name);
    }
  }
}

/**************************************************************************************/

