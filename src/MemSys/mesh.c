/* mesh.c

   This file contains variables and functions relating to the
   2-dimensional mesh interconnection network.

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



#include "MemSys/net.h"
#include "Processor/simio.h"
#include <malloc.h>

/******************************************************************************
  This file provides the user with a n-dimensional bidirectional
  (non-wraparound) mesh. A routing function which uses dimension
  ordered routing is also provided.  The following defines the call:
  
  void	CreateMESH (
  int dim		: The number of dimensions
  int *mesh_size  	: An array which contains the number of nodes in 
  each dimension
  int b_size		: The buffer size in the network.
  int p_size		: The port size. (input as well as output)
  IPORT **iport_ptr	: Pointer to a location of IPORT * which will be
                          filled up by the code. This location must be 
                          (number of nodes) long.
  OPORT **oport_ptr	: Pointer to a location of OPORT * which will be 
  filled up by the code.
  
  int (*router)	: Pointer to the routing function.
  )
  
  The routing function is called MeshRoute() and is defined in this file.
  The only new changes are renaming the function, and the removal of b_thresh.
  
  ****************************************************************************/

#ifndef PARCSIM	
#define PARCSIM
#endif

#include "MemSys/simsys.h"

#define NUM_MODULES 	3	/* DeMux, Mux, Buffer */
#define MODULE_BITS	3	/* 2 for the three modules, 1 for direction */
#define MODULE 		int

void CreateMESH ();
int  MeshRoute ();
void MeshCreate ();
void Create1DMesh ();
void ConnectMeshes ();
void CreateComponent ();
void ConnectComponents ();
void NodeIntraConnect ();
static int MyRoute(int *src, int *dest, int id);
IPORT *cpu_iport[256];
OPORT *cpu_oport[256];

FILE *fp;
int 	end_node, buf_size, port_size, (*router)();

/* These should be static to the file, as the routing function needs them */
static int	num_nodes, incr, *mesh_size, actual_dim, log_dim;

int meshnum;
MODULE	**table;

int mesh_first = 0;

static int MyLog2 (dim)
     int dim; 
{
  
  int     i = 2, j;
  
  for (j = 1; i < dim; i *= 2, j++);
  return (j);
}


/*****************************************************************************/
/* CreateMESH: Mesh initialization function.                                 */
/*****************************************************************************/

void CreateMESH (int dim, int *m_size, int b_size, int p_size, IPORT **iport_ptr, OPORT **oport_ptr, int (*routefunc)(int *,int *,int), int mesh_num)
{
  
  int 	i;
  char name[32];
  
  if (mesh_num < 2) {
    /*Only two cases taken care of --  REQ_NET = 0, REPLY_NET = 1 */
    buf_index[mesh_num] = 0;
    oport_index[mesh_num] = 0;
    meshnum = mesh_num;

    NUM_HOPS = 0;
    for (i=0; i<dim; i++)
      NUM_HOPS += m_size[i]-1;   /* Sum up hops in x direction + hops in y direction */

    /* Produce new statistics for network */
    
    sprintf(name, "PktNumHopsHist_Net%d",mesh_num);
    PktNumHopsHist[mesh_num] = NewStatrec(name, POINT, MEANS, HISTSPECIAL, 
					  NUM_HOPS+1, 0.0, (double)NUM_HOPS+1);  
    sprintf(name, "PktSzHist_Net%d",mesh_num);
    PktSzHist[mesh_num] = NewStatrec(name, POINT, MEANS, HISTSPECIAL, 128, 
				     0.0, 128.0);
    
    PktHpsTimeTotalMean[mesh_num] =  (STATREC **)malloc(sizeof(STATREC *)*(NUM_HOPS+1));
    PktHpsTimeNetMean[mesh_num] =  (STATREC **)malloc(sizeof(STATREC *)*(NUM_HOPS+1));
    PktHpsTimeBlkMean[mesh_num] =  (STATREC **)malloc(sizeof(STATREC *)*(NUM_HOPS+1));
    
    for (i=0; i<NUM_SIZES; i++) {
      sprintf(name, "PktSzTimeTotalMean_Net%d_Sz%d",mesh_num, i);
      PktSzTimeTotalMean[mesh_num][i] = NewStatrec(name, POINT, MEANS,NOHIST,0, 0.0, 0);
      sprintf(name, "PktSzTimeNetMean_Net%d_Sz%d",mesh_num, i);
      PktSzTimeNetMean[mesh_num][i] = NewStatrec(name, POINT, MEANS,NOHIST,0, 0.0, 0);
      sprintf(name, "PktSzTimeBlkMean_Net%d_Sz%d",mesh_num, i);
      PktSzTimeBlkMean[mesh_num][i] = NewStatrec(name, POINT, MEANS,NOHIST,0, 0.0, 0);
    }
    
    for (i=0; i<(NUM_HOPS+1); i++) {
      sprintf(name, "PktHpsTimeTotalMean_Net%d_Hop%d",mesh_num, i);
      PktHpsTimeTotalMean[mesh_num][i] = NewStatrec(name, POINT, MEANS,NOHIST,0, 0.0, 0);
      sprintf(name, "PktHpsTimeNetMean_Net%d_Hop%d",mesh_num, i);
      PktHpsTimeNetMean[mesh_num][i] = NewStatrec(name, POINT, MEANS,NOHIST,0, 0.0, 0);
      sprintf(name, "PktHpsTimeBlkMean_Net%d_Hop%d",mesh_num, i);
      PktHpsTimeBlkMean[mesh_num][i] = NewStatrec(name, POINT, MEANS,NOHIST,0, 0.0, 0);
    }
    
    sprintf(name, "PktTOTimeTotalMean_Net%d",mesh_num);
    PktTOTimeTotalMean[mesh_num] = NewStatrec(name, POINT, MEANS,NOHIST,400, 0.0, 4000.0);
    sprintf(name, "PktTOTimeNetMean_Net%d",mesh_num);
    PktTOTimeNetMean[mesh_num] = NewStatrec(name, POINT, MEANS,NOHIST,400, 0.0, 4000.0);
    sprintf(name, "PktTOTimeBlkMean_Net%d",mesh_num);
    PktTOTimeBlkMean[mesh_num] = NewStatrec(name, POINT, MEANS,NOHIST,400, 0.0, 4000.0);

    for (i=0; i<1024; i++)
      net_index[i] = -1;
    for (i=0; i<NUM_SIZES; i++)
      rev_index[i] = -1;
    cur_index = 0;
  }
  
  num_nodes = 1;		
  
  fp  = fopen ("/dev/null", "w");  /* Turned off this tracing */
  router = routefunc ? routefunc : MeshRoute;
  mesh_size = (int *) malloc (dim * sizeof(int));
  for (i = 0; i < dim; i++) {
    mesh_size[i] = m_size[i];
    num_nodes *= mesh_size[i];
  }
  
  actual_dim = dim;
  buf_size = b_size;
  port_size = p_size;
  log_dim = MyLog2 (dim);
  incr = 0x01 << (MODULE_BITS + log_dim + 1);
  end_node = num_nodes << (MODULE_BITS + log_dim + 1);
  table = (MODULE **) malloc (num_nodes * dim * 6 * sizeof (int));
  MeshCreate (dim, mesh_size, 1);
  NodeIntraConnect (iport_ptr, oport_ptr);	/* Creates the processor port,
    					   	   and interconnects within a 
						   node. */
  free (table);
}


void MeshCreate (dim, mesh_size, num_meshes)
     int dim, *mesh_size, num_meshes; 
{
  
  if (dim == 1)
    Create1DMesh (mesh_size, num_meshes);
  else {
    num_meshes *= mesh_size[dim-1];
    MeshCreate (dim - 1, mesh_size, num_meshes);
    ConnectMeshes (dim, mesh_size, num_meshes);
  }
}


void Create1DMesh (mesh_size, num_meshes)
     int *mesh_size, num_meshes; 
{
  
  int 	i, j, row_begin, row_end;
  
  fprintf (fp, "\n\nCreating %d 1-D meshes\nBeginning to create 0th dim comps.\n",
	   num_meshes);
  for (i = 0; i < end_node; i += incr)
    CreateComponent (i, 0);
  
  fprintf (fp, "0th dim components created\n\n");
  
  for (i = 0; i < num_meshes; i++) {
    row_begin = i*mesh_size[0] * incr;
    row_end = (i + 1)*mesh_size[0] * incr;
    
    fprintf (fp, "\nStart mesh %d 0th dim components connection\n", i);
    for (j = row_begin; j < row_end - incr; j += incr)
      ConnectComponents (j, j+incr, 0);
    
    fprintf (fp, "0th dimension connection done\n\n");
  }
}


/* When we enter ConnectMeshes, we have a number of meshes that are connected
   in dimensions 0 through dim-1. We now have to hook them up in dimension dim.
   To do this, components in this dimension have to be created frist, and then
   connected. We will most likely screw up in the component numbering -
   because, unlike as in the 1D case, we cannot just connect adjacent
   components here. */

void ConnectMeshes (dim, mesh_size, num_meshes)
     int dim, *mesh_size, num_meshes; 
{
  
  int 	i, j, k, begin_node;
  int	npm0, npm1, mpm0, mpm1;
  
  
  fprintf (fp, "\n\nConnecting %d    %d-D meshes\n", num_meshes, dim-1); 
  fprintf (fp, "Creating %d-D components\n", dim);
  for (i = 0; i < end_node; i += incr)
    CreateComponent (i, dim-1);
  
  
  /* Variables used below:
     npm0 = number of nodes per mesh (the ones to be connected)
     npm1 = number of nodes in the mesh after connection 
     mpm0 = number of (dim-1) meshes that make up the (dim) mesh.
     mpm1 = number of (dim) meshes that will result.
     */
  
  
  npm0 = 1;
  for (i = 0; i < dim - 1; i++) 
    npm0 *= mesh_size[i];
  
  npm1 = npm0 * mesh_size[dim-1];
  mpm0 = mesh_size[dim-1];
  mpm1 = num_meshes / mpm0;
  
  
  for (i = 0; i < mpm1; i++) {
    begin_node = i * npm1 * incr;
    
    for (j = 0; j < mpm0 - 1; j++) {
      for (k = 0; k < npm0; k++) {
	ConnectComponents (begin_node + k * incr, 
			   begin_node + (k + npm0) * incr, dim-1);
      }
      begin_node += npm0 * incr;
    }
  }
}




void CreateComponent (i, dim)
     int i, dim; 
{
  
  int mux0, mux1, buf0, buf1, demux0, demux1, node;
  
  node = (i/incr) * actual_dim * 2 * NUM_MODULES + dim * 2 * NUM_MODULES;
  
  mux0   = i | (dim << MODULE_BITS) | 0x0000;
  buf0   = i | (dim << MODULE_BITS) | 0x0001;
  demux0 = i | (dim << MODULE_BITS) | 0x0002;
  
  fprintf (fp, "\nCreating node %d, dim %d, dir0 mux %d\n", 
	   i/incr, dim, mux0);
  table[node] = (MODULE *) NewMux (mux0, actual_dim * 2);
  fprintf (fp, "\n\nCreating node %d, dim %d, dir0 buffer  %d\n", 
	   i/incr, dim, buf0);
  table[node + 1] = (MODULE *) NewBuffer (buf0, buf_size, 1);
  if (buf_index[meshnum] < 2048) {
    sprintf(WhichBuf[meshnum][buf_index[meshnum]], "Buffer of node %d, dim %d, dir0", i/incr, dim);
    BufTable[meshnum][buf_index[meshnum]++] = (BUFFER *)table[node+1];
  }
  fprintf (fp, "\nCreating node %d, dim %d, dir0 demux  %d\n", 
	   i/incr, dim, demux0);
  table[node + 2] = (MODULE *) NewDemux (demux0, actual_dim * 2, router);
  
  mux1   = i | (dim << MODULE_BITS) | 0x0004;
  buf1   = i | (dim << MODULE_BITS) | 0x0005;
  demux1 = i | (dim << MODULE_BITS) | 0x0006;
  
  fprintf (fp, "\nCreating node %d, dim %d, dir1 mux %d\n", 
	   i/incr, dim, mux1);
  table[node + 3] = (MODULE *) NewMux (mux1, actual_dim * 2);
  fprintf (fp, "\n\nCreating node %d, dim %d, dir1 buffer  %d\n", 
	   i/incr, dim, buf1);
  table[node + 4] = (MODULE *) NewBuffer (buf1, buf_size, 1);
  if (buf_index[meshnum] < 2048){
    sprintf(WhichBuf[meshnum][buf_index[meshnum]], "Buffer of node %d, dim %d, dir1", i/incr, dim);
    BufTable[meshnum][buf_index[meshnum]++] = (BUFFER *)table[node+4];
  }
  fprintf (fp, "\nCreating node %d, dim %d, dir1 demux %d\n", 
	   i/incr, dim, demux1);
  table[node + 5] = (MODULE *) NewDemux (demux1, actual_dim * 2 , router);
  
  fprintf (fp, "\n\t--------------------------------------------------------\n\n");
}


void ConnectComponents (node0, node1, dim)
     int node0, node1, dim; 
{
  
  int 	n0, n1;
  MUX	*mux0, *mux1;
  BUFFER	*buf0, *buf1;
  
  n0 = (node0/incr) * actual_dim * 2 * NUM_MODULES + dim * 2 * NUM_MODULES;
  n1 = (node1/incr) * actual_dim * 2 * NUM_MODULES + dim * 2 * NUM_MODULES;
  
  mux0 = (MUX *) table [n0 + 3];	/* Within a node, dim, dir 1 */
  buf0 = (BUFFER *) table [n0 + 4];	/* Within a node, dim, dir 0 */
  
  mux1 = (MUX *) table [n1];       /* Within a node, dim, dir 1 */
  buf1 = (BUFFER *) table [n1 + 1];   /* Within a node, dim, dir 0 */
  
  
  fprintf (fp, "\nConnecting %d --> %d\n", 
	   node0 | (dim << MODULE_BITS) | 0x0004,
	   node1 | (dim << MODULE_BITS) | 0x0001);
  NetworkConnect (mux0, buf1, 0, 0);
  fprintf (fp, "\nConnecting %d --> %d\n", 
	   node1 | (dim << MODULE_BITS) | 0x0000,
	   node0 | (dim << MODULE_BITS) | 0x0005);
  NetworkConnect (mux1, buf0, 0, 0);
  
  fprintf (fp, "\n\t--------------------------------------------------------\n\n");
}


/* Node intra-connection. The following rules are used to determine mux and 
   demux indices. 
   
   1. A normal demux connects to the processor mux(es) through the highest
   index numbers.
   2. A normal demux connects to other normal muxes using indices 0 through the
   number of normal muxes in existence. The index it connects to in the
   destination mux is the source's component number, if the source component
   number is less than the destination's. When the source component number
   is greater than the destination's, it connects to the dest. mux index 
   (source demux component number - 1). 
   3. The processor demux connects to the normal muxes using the component
   numbers as indices on the demux, and the highest index on the destination
   muxes. 				
   
   */

void NodeIntraConnect (iport_ptr, oport_ptr) 
     IPORT **iport_ptr;
     OPORT **oport_ptr;
{
  
  int	node, dim, dir, proc_id, base, dest, mux_index, demux_index,
  num_comps, node_base, end_comp;
  BUFFER 	*buf_ptr, *proc_buf_ptr, *buf2_ptr;
  MUX	*proc_mux_ptr, *mux2_ptr;
  DEMUX	*demux_ptr, *proc_demux_ptr, *demux2_ptr;
  int node2;
  num_comps = 2 * actual_dim;  /* The number of components excluding
				  the processor component. */
  
  
  for (node = 0; node < end_node; node += incr)

  {
      fprintf (fp, "Intra-connecting node %d\n", node);
      proc_id = node | (0x01 << (log_dim + MODULE_BITS));
      proc_mux_ptr = NewMux (proc_id, actual_dim*2 + 1);
      proc_buf_ptr = NewBuffer ((proc_id | 0x01), buf_size, 1);
      if (buf_index[meshnum] < 2048) {
	  sprintf(WhichBuf[meshnum][buf_index[meshnum]], "Buffer of node %d, prcr_buf", node/incr);
	  BufTable[meshnum][buf_index[meshnum]++] = (BUFFER *)proc_buf_ptr;
      }
      proc_demux_ptr = NewDemux ((proc_id | 0x02), actual_dim*2 + 1, router);
      
      /* Create the network input and output ports at this node, and connect
	 them to the processor mux and buffer. 
	 
	 Historically, NETSIM hooks up one index of the processor demux to the 
	 processor mux so that a proc may send itself messages. This
	 connection is unused in RSIM. */
      
      NetworkConnect (proc_demux_ptr, proc_mux_ptr, actual_dim*2,
		      actual_dim*2);
      
      *iport_ptr = NewIPort ((proc_id | 0x03), port_size);
      *oport_ptr = NewOPort ((proc_id | 0x04), port_size);
      if (oport_index[meshnum] < 200)
	  OportTable[meshnum][oport_index[meshnum] ++] = *oport_ptr;
      NetworkConnect (*iport_ptr, proc_buf_ptr, 0, 0);
      if (meshnum == REPLY_NET) {
	  node2 = node >> (MODULE_BITS + log_dim + 1);
	  cpu_iport[node2] = NewIPort(node2, port_size);
	  cpu_oport[node2] = NewOPort(node2, port_size);

	  demux2_ptr = NewDemux(node, 2, MyRoute);
	  buf2_ptr = NewBuffer(node, buf_size, 1);
	  mux2_ptr = NewMux(node,2);

	  NetworkConnect(proc_mux_ptr, demux2_ptr, 0,0);
	 
	  NetworkConnect(demux2_ptr, *oport_ptr, 0,0);
	  NetworkConnect(demux2_ptr, cpu_oport[node2] ,1,0);

	  NetworkConnect(proc_buf_ptr, mux2_ptr,0,0);
	  NetworkConnect(cpu_iport[node2], buf2_ptr,0,0);
	  NetworkConnect(buf2_ptr, mux2_ptr,0,1);
	  NetworkConnect(mux2_ptr, proc_demux_ptr, 0,0);
      }
      else {
	  NetworkConnect (proc_buf_ptr, proc_demux_ptr, 0, 0);
	  NetworkConnect (proc_mux_ptr, *oport_ptr, 0, 0);
      }
      iport_ptr++;
      oport_ptr++;
      
      node_base = (node/incr) * actual_dim * 2 * NUM_MODULES;
      
      for (dim = 0; dim < actual_dim; dim++) {
	  for (dir = 0; dir < 2; dir++) {
	      base = node_base + dim*2*NUM_MODULES + dir*NUM_MODULES;
	      
	      buf_ptr   = (BUFFER *) table [base + 1];
	      demux_ptr = (DEMUX *) table [base + 2];
	      NetworkConnect (buf_ptr, demux_ptr, 0, 0); /* buffer to demux */
	      
	      /* The node, dimension and direction specify a particular
		 component. Now connect this component's demux to all the
		 other component muxes in this node. First connect it to the
		 processor mux. Every demux connects to the processor mux via
		 the highest demux index. It connects to the processor mux 
		 index given by its order number (dim*2 + dir) in the node. */
	      
	      NetworkConnect (demux_ptr, proc_mux_ptr, num_comps - 1, 
			      2 * dim + dir); 
	      
	      end_comp = node_base + num_comps*NUM_MODULES;
	      demux_index = 0;
	      for (dest = node_base; dest < end_comp; dest += NUM_MODULES) {
		  if (base != dest) {
		      mux_index = (base - node_base)/NUM_MODULES;
		      if (base > dest)
			  mux_index--;
		      NetworkConnect (demux_ptr, table[dest], demux_index,
				      mux_index);	
		      demux_index++;
		  }
	      }
	  }
      }
    
      /* Now connect the processor demux to all the component muxes. */
    
      for (dest = node_base; dest < end_comp; dest += NUM_MODULES) {
	  NetworkConnect (proc_demux_ptr, table[dest], 
			  (dest - node_base)/NUM_MODULES, num_comps-1);
      }
      
  }
}


/*****************************************************************************/
/* MeshRoute: Mesh routing function. Routing has three phases.               */
/*   Entry through a processor DeMux,                                        */
/*   Routing through node DeMuxes,                                           */
/*   Departure through a network Mux                                         */
/*                                                                           */
/*   The routing function here routes in the highest dimension before        */
/*   going to the next one. This is deadlock free.                           */
/*****************************************************************************/


int MeshRoute (int *src, int *dest, int id) 
{
  
  int	radix, src_index, dest_index, curr_node, dest_node, dim, dir,
  src_demux_id, exit_demux_id, exit_index, temp;
  
  
  curr_node = id / (0x01 << (MODULE_BITS + log_dim + 1));
  /* The seemingly wierd expression is used instead of a direct
     right shift: id >> (MODULE_BITS + log_dim + 1) to prevent
     any accidents when id becomes huge enough for some machines to
     treat it as a signed quantity and carry out arithmetic right
     shifts. */
  
  
  temp = curr_node << (MODULE_BITS + log_dim + 1);

  dest_node = (*dest & 0x0fff); /* check for shared-memory or message-passing */
  /* fprintf (fp, "\n\nMESH ROUTE: %d --> %d, now at node %d module %d.",
   *src, *dest, curr_node, id); */

  if (*src == dest_node) { /* should never enter this case. */
    fprintf (simerr, "WARNING: Processor %d sending to itself %d %d @%1.0f\n",
	     *src, dest_node, *dest, YS__Simtime); 
    return (2*actual_dim);
  }
  
  if (curr_node == dest_node) {
    return (2*actual_dim - 1);
  }
  
  
  radix = num_nodes;
  
  for (dim = actual_dim - 1; dim >= 0; dim--) {
    radix /= mesh_size[dim];
    src_index = curr_node/radix;
    dest_index = dest_node/radix;
    if (src_index != dest_index)
      break;
    curr_node %= radix;
    dest_node %= radix;
  }
  
  /* We have to route along dim now. */
  
  dir = (src_index < dest_index);
  
  /* Now we can find out the component through which the packet must leave
     this node. The index through which the demux must be left can be found
     easily by comparing the source demux id with the id of the demux in the
     destination component. This is what we do next. See indexing rules in
     NodeIntraConnect for more details.  src_demux_id != id. */
  
  src_demux_id = id & (~(~0x00 << (1 + log_dim + MODULE_BITS)));
  
  exit_demux_id = (dim << MODULE_BITS) | (dir << (MODULE_BITS - 1)) | 0x02;
  
  exit_index = 2*dim + dir;
  
  if (exit_demux_id > src_demux_id)
    exit_index--;
  
  /*fprintf (fp, " Exit module %d\n", (exit_demux_id & ~0x02) + temp); */
  return exit_index;
  
}

/**************************************************************************/
/* MyRoute: called by the demux that selects between the CPU (for         */
/* message-passing) and shared-memory. If "0x1000" is in the              */
/* destination, then the packet must go to the CPU. Otherwise, it is a    */
/* shared memory reference. Only shared-memory supported in RSIM          */
/**************************************************************************/

static int MyRoute(src, dest, id)
int *src, *dest, id;
{
    if (*dest & 0x1000)
	return(1);
    return(0);
}

