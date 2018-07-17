/************************************************************************
 sor.c : Implements a Red-Black SOR using separate red
       : and block matrices to minimize false sharing -- solves a
       : M+2 by 2N+2 array

       SOR - OPTIONS
          -p - Number of processors
	  -m - Number of rows
	  -n - Number of columns by 2
	  -i - Number of iterations
	  -d - Display input and output matrices
	  -h - Help
 ************************************************************************/

#include <rsim_apps.h>
#include <stdio.h>
#include <stdlib.h>


int     NUM_PROCS = 1;      /* number of processors */
int     iterations = 2;    /* number of iterations */
int     M = 100;           /* Dimensions of the array */
int     N = 50;            /* N.B. There are 2N columns. */

/* Shared variables */
float **red_;
float **black_;
TreeBar barrier;                            /* Tree barrier */

/* function definitions */
void sor_odd(int begin, int end, int whoami);
void sor_even(int begin, int end, int whoami);
void root_prog();
void do_sor();
void printall();

/* private variables */
struct arg{
  int start;
  int end;
  int whoami;
};

struct arg **argtable;
int    showmatrix = 0;
int proc_id;
int phase;
extern  char           *optarg;

/*************************************************************************
  main() : handles command line parsing, intialization of shared address
         : space, distribution of shared address space, initialization
	 : of the array and scheduling of the parallel tasks
 *************************************************************************/

main(int argc, char **argv)
{
  int             c, i, j;
  int             begin, end, whoami;

  MEMSYS_OFF;       /* turn off detailed simulation for initialization */
  while ((c = getopt(argc, argv, "i:m:n:p:d")) != -1)
    switch (c) {
    case 'i':
      iterations = atoi(optarg);
      break;
    case 'm':
      M = atoi(optarg);
      break;
    case 'n':
      N = atoi(optarg);
      break;
    case 'p':
      NUM_PROCS = atoi(optarg);
      break;
    case 'd':
      showmatrix = 1;
      break;
    case 'h':
    default:
      fprintf(stderr, "SOR - OPTIONS\n");
      fprintf(stderr, "\tp - Number of processors\n");
      fprintf(stderr, "\tm - Number of rows\n");
      fprintf(stderr, "\tn - Number of columns by 2\n");
      fprintf(stderr, "\ti - Number of iterations\n");
      fprintf(stderr, "\td - Display input and output matrices\n");
      fprintf(stderr, "\th - Help\n");
      return;
    }
  
  red_ = (float **) shmalloc((M + 2)*sizeof(float *));
  black_ = (float **) shmalloc((M + 2)*sizeof(float *));

  if(red_ == NULL || black_ == NULL){
    fprintf(stderr, "Unable to malloc shared region\n");
    exit(-1);
  }
  
  /* Allocate individual rows */
  for (i = 0; i <= M + 1; i++) {
    /* All rows of reds */
    red_[i] = (float *) shmalloc((N + 1)*sizeof(float));
    fprintf(stderr, "red row %2d: %d\n",i,red_[i]);
    if(red_[i] == NULL){
      fprintf(stderr, "out of shared memory\n");
      return ;
    }
  }

  for (i = 0; i <= M + 1; i++) {
    /* All rows of blacks */
    black_[i] = (float *)shmalloc((N + 1)*sizeof(float));
    fprintf(stderr, "black row %2d: %d\n",i,black_[i]);
    if(black_[i]== NULL){
      fprintf(stderr, "out of shared memory\n");
      return ;
    }
  }
  
  for (i = 0; i <= M + 1; i++) {
    /* Initialize the data right now! */
    if ((i == 0) || (i == M + 1))
      for (j = 0; j <= N; j++)
        red_[i][j] = black_[i][j] = 1.0; 
    else if (i & 1) {
      red_[i][0] = 1.0;
      black_[i][N] = 1.0;
    }
    else {
      black_[i][0] = 1.0;
      red_[i][N] = 1.0;
    }
  }
  argtable = (struct arg **)shmalloc(NUM_PROCS * sizeof(struct arg *));
  
  if(argtable==NULL) 
    printf("could not allocate memory for argtable\n");
    
  for(i=0;i<NUM_PROCS;i++) {

    argtable[i] = (struct arg*)shmalloc(sizeof(struct arg));
    if(argtable[i]==NULL)
      printf("could not allocate memory for argtable %d\n",i);
  }


  TreeBarInit(&barrier,NUM_PROCS); /* initialize tree barrier */
  
  root_prog();
  if(showmatrix) printall();
  proc_id = 0;
  MEMSYS_ON;
  for(i=0;i<NUM_PROCS-1;i++){
    if(fork() == 0)     {
      proc_id =  getpid();
      break;
    }
  }
  begin = argtable[proc_id]->start;
  end = argtable[proc_id]->end;
  whoami = argtable[proc_id]->whoami;
  TREEBAR(&barrier, whoami);
  if (proc_id == 0)     {
    StatReportAll();
    StatClearAll();
  }

  /***************************************************************
   *************** Beginning of parallel phase *******************
   ***************************************************************/

  endphase();
  newphase(++phase);
  if (begin & 1)
    sor_odd(begin, end,whoami);
  else
    sor_even(begin, end,whoami);
  
  if (whoami == 0)     {
    StatReportAll();
    StatClearAll();
  }

  /***************************************************************
   ******************* End of parallel phase *********************
   ***************************************************************/
  endphase();
  newphase(++phase);
  if(showmatrix) printall(); 
  
}

/*********************************************************************
 root_prog : handles distribution of shared address space among the
           : different processors
 *********************************************************************/
void root_prog(void)
{
  int begin, end, i;
  char name[32];
  
  fprintf(stderr, "\n %d X %d matrix - %d iterations -- %d processors\n\n",
          M, 2*N, iterations,NUM_PROCS);
  for(i=0;i<NUM_PROCS;i++){
    begin = (M*i)/NUM_PROCS + 1;
    end   = (M*(i + 1))/NUM_PROCS;
    fprintf(stderr, "Allocating Jobs %d to %d to processor %d\n",
	    begin, end, i);
    
    argtable[i]->start = begin;
    argtable[i]->end = end;
    argtable[i]->whoami = i;
    
    sprintf(name, "matrixred_%d",i);
    fprintf(stderr,"%15s -allocating %d to %d to processor %d\n",name,
            &(red_[begin][0]), &(red_[end][N]), i);
    AssociateAddrNode(&(red_[begin][0]), &(red_[end][N]), i, name);
    
    sprintf(name, "matrixblack_%d",i);
    fprintf(stderr,"%15s -allocating %d to %d to processor %d\n",name,
            &(black_[begin][0]), &(black_[end][N]), i);
    AssociateAddrNode(&(black_[begin][0]), &(black_[end][N]), i, name);
    
    sprintf(name, "arg_%d",i);
    fprintf(stderr,"%15s -allocating %d to %d to processor %d\n",name,
            argtable[i], argtable[i] + 1, i);
    AssociateAddrNode(argtable[i], argtable[i] + 1, i, name);
  }
  return;
}

/*************************************************************************
  printall() : procedure to print out the matrix properly ordered as
             : reds and blacks
 *************************************************************************/

void printall()
{
  int i,j;
  for(i=0;i<=M+1;i++){
    fprintf(stderr , "row %2d\t",i);
    for(j=0;j<=N;j++){
      if(i&1)
        fprintf(stderr, "%5.2f %5.2f ",red_[i][j],black_[i][j]);
      else
        fprintf(stderr, "%5.2f %5.2f ", black_[i][j],red_[i][j]);
    }
    fprintf(stderr, "\n");
  }
}


/**************************************************************************
  sor_odd :  begin is odd
  *************************************************************************/
void    sor_odd(int begin, int end, int whoami)
{
  int     i, j, k;
  
  for (i = 0; i < iterations; i++) {
    for (j = begin; j <= end; j++) {
      for (k = 0; k < N; k++) {
	black_[j][k] = (red_[j-1][k] + red_[j+1][k]
			+ red_[j][k] + red_[j][k+1])/(float) 4.0;
      }
      if ((j += 1) > end) break;
      for (k = 1; k <= N; k++) {
	black_[j][k] = (red_[j-1][k] + red_[j+1][k]
			+ red_[j][k-1] + red_[j][k])/(float) 4.0;
      }
    }
    TREEBAR(&barrier, whoami);

    for (j = begin; j <= end; j++) {
      for (k = 1; k <= N; k++) {
	red_[j][k] = (black_[j-1][k] + black_[j+1][k]
		      + black_[j][k-1] + black_[j][k])/(float) 4.0;
      }
      if ((j += 1) > end) break;
      for (k = 0; k < N; k++) {
	red_[j][k] = (black_[j-1][k] + black_[j+1][k]
		      + black_[j][k] + black_[j][k+1])/(float) 4.0;
      }
    }
    TREEBAR(&barrier, whoami);
  }
}

/**************************************************************************
  sor_even :  begin is even
  *************************************************************************/
void    sor_even(int begin, int end, int whoami)
{
  int     i, j, k;
  
  for (i = 0; i < iterations; i++) {
    for (j = begin; j <= end; j++) {
      for (k = 1; k <= N; k++) {
	black_[j][k] = (red_[j-1][k] + red_[j+1][k]
			+ red_[j][k-1] + red_[j][k])/(float) 4.0;
      }
      if ((j += 1) > end)  break;
      for (k = 0; k < N; k++) {
	black_[j][k] = (red_[j-1][k] + red_[j+1][k]
			+ red_[j][k] + red_[j][k+1])/(float) 4.0;
      }
    }
    TREEBAR(&barrier, whoami);

    for (j = begin; j <= end; j++) {
      for (k = 0; k < N; k++) {
	red_[j][k] = (black_[j-1][k] + black_[j+1][k]
		      + black_[j][k] + black_[j][k+1])/(float) 4.0;
      }
      if ((j += 1) > end)  break;
      for (k = 1; k <= N; k++) {
	red_[j][k] = (black_[j-1][k] + black_[j+1][k]
		      + black_[j][k-1] + black_[j][k])/(float) 4.0;
      }
    }
    TREEBAR(&barrier, whoami);
  }
}









