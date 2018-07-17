/*****************************************************************************
  quicksort.c

  Code based on the quick sort code distributed as part of Treadmarks.
  Modified for hardware DSMs.
  
  QS - OPTIONS
      -p - Number of processors
      -s - Size of array
      -b - Bubble threshold
      -B - Bubble
      -d - Display output
      -v - Verify results
      -H - Help
      
  ***************************************************************************/

/* Include files */
#include "quicksort.h"
#include <stdio.h>
#include <stdlib.h>
#include <locks.h>
#include <treebar.h>
#include <rsim_apps.h>

GlobalMemory *gMem;                       /* global memory */
int NUM_PROCS = 1;                        /* number of processors */
int size = DEFAULT_SIZE;                  /* default sort size */
int BubbleThresh = DEFAULT_BUBBLE;        /* threshold to switch bubblesort */
int verify = 1;                           /* verify correctness : default on */
int show_array = 0;                       /* print sorted array : def: off */
int bubble = 1;                           /* bubble sort on flag */
int collect_info;                         /* collect stats flag */

static int chunk;                         /* private variables */
extern int private_accesses[128];
int whoami;                               /* processor number */
int phase = 0;                            /* execution phase */
TreeBar tree;                             /* Tree barrier */
int PopWork(TaskElement **);

/*************************************************************************
  main(): Parse command line, and call the main worker routine
  ************************************************************************/

main(int argc, char **argv)
{
  int i,j;
  int c;
  extern char *optarg;
  int ctrproc;
  int start;
  int proc_size;
  char name[32];

  /*********************************************************************
    Parse the command line
    ********************************************************************/
  collect_info = 1;
  MEMSYS_OFF;                             /* in the initialization phase */
  while ((c = getopt(argc, argv, "p:s:b:BdvH")) != -1) {
    switch(c) {
    case 'p': NUM_PROCS = atoi(optarg);       
      if (NUM_PROCS < 1) {
        printf("P must be >= 1\n");
        exit(-1);
      }
      break;
      
    case 's':
      size = atoi(optarg);
      break;
      
    case 'b':
      BubbleThresh = atoi(optarg);
      break;
      
    case 'B': bubble = 1;   break;
    case 'd': show_array = 1; break;
    case 'v': verify = 1; break;
    default:  printf("Bad option : %s \n",optarg); 
    case 'h':
      printf( "\t\t\tQS - OPTIONS\n");
      printf( "\tp - Number of processors\n");
      printf( "\ts - Size of array\n");
      printf( "\tb - Bubble threshold\n");
      printf( "\tB - Bubble\n");
      printf( "\td - Display output \n");
      printf( "\tv - Verify results \n");
      printf( "\tH - Help\n");
      exit(0);
    }
  }
  
  StatClearAll();         /* clear the stats */
  /**********************************************************************/
  /* Use shmalloc to allocate memory from the shared address space, also
     use AssociateAddrNode to determine the distribution of the shared
     memory among the various processors */
  /**********************************************************************/
  gMem = (GlobalMemory *) shmalloc(sizeof(GlobalMemory));
  ctrproc = NUM_PROCS/2  + (int)(sqrt((double)NUM_PROCS)/2);
  /* choose a "middle-point" in the mesh network */

  /* associate the task stack variables to a processor easily accessible */
#if !defined(SPATIAL)
  AssociateAddrNode((void *)&(gMem->TaskStackLock),
                    (void *)(&(gMem->NumWaiting)+1),
                    ctrproc>=NUM_PROCS-1? NUM_PROCS-1 :  ctrproc+1,"lock");
  AssociateAddrNode((void *)&(gMem->TaskStackTop),
                    (void *)(&(gMem->TaskStackTop)+1),
                    ctrproc>=NUM_PROCS-1 ? NUM_PROCS-1 : ctrproc,"top");
#endif

  /************** associate the task queue among all processors *********/
  chunk = MAX_TASK_QUEUE / NUM_PROCS;
  for (i=0; i< NUM_PROCS; i++)   {
#if !defined(SPATIAL)
    AssociateAddrNode(&gMem->tasks[i*chunk],
		      &gMem->tasks[(i+1)*chunk],
		      i,"tasks");
#endif
  }
  LocalStackTop = -1;
  FREELOCK(&gMem->TaskStackLock);
  proc_size = (size + NUM_PROCS)/NUM_PROCS;

  /*************** associate the array among all processors **************/
  start = 0;
  strcpy(name,"Array chunks");
  for(i=0;i<NUM_PROCS;i++){
    printf("going to call Associate address node\n");
#if !defined(SPATIAL)&&!defined(DO_PREF)
    AssociateAddrNode(&gMem->A[start],
                      &gMem->A[start+ proc_size],
                      i, name);
#endif
    start = start+proc_size;
  }

  printf( "QS - OPTIONS\n");
  printf( "\tp - Number of processors \t%d\n", NUM_PROCS);
  printf( "\ts - Size of array\t\t%d\n",size);
  printf( "\tb - Bubble threshold \t\t%d\n",BubbleThresh);
  printf( "\tB - Bubble \t\t\t%d\n",bubble);
  printf( "\td - Display output\t\t%d\n",show_array);
  printf( "\tv - Verify results\t\t%d\n",verify);

  /* The work which the root process has to do */
  whoami=0;
  root_main();                  /* initialization by the root process */
  endphase(phase);              /* end of initialization phase */
  TreeBarInit(&tree,NUM_PROCS); /* initialize tree barrier */
  
  MEMSYS_ON;

  /********************************************************************/
  /* Forking processes :  Create a process for each of the processors */
  /********************************************************************/
  
  for(i=0;i<NUM_PROCS-1;i++){
    if(fork() == 0)     {
      whoami =  getpid();
      LocalStackTop = -1;
      for (i = (whoami+1) * chunk -1; i > whoami * chunk; i--) {
	LocalTaskStack[++LocalStackTop] = i;
      }
      break;
    }
  }

  /******************* Barrier after initialization *******************/
  printf("Before barrier %d\n",whoami);
  TreeBarrier(&tree,whoami);
  printf("Starting Process %d\n",whoami);
  if (whoami == 0)     {
    StatReportAll();
    StatClearAll();
  }

  newphase(++phase);
  Worker();   /**** Call the main procedure which we have to execute ****/
  endphase(phase);

  /**************** Barrier after finishing the sort ******************/
  printf("Coming out of worker %d\n",whoami);
  TreeBarrier(&tree,whoami);
  MEMSYS_OFF;
  if (whoami == 0)     {
    StatReportAll();
    StatClearAll();
  }

  /*************************** Cleanup phase ***************************/
  newphase(++phase);
  if(whoami== 0)     do_cleanup();
  endphase(phase);
  if (whoami == 0)     {
    StatReportAll();
    StatClearAll();
  }
}

/*************************************************************************
  do_cleanup : handles operations performed after the sort is complete,
             : specifically, check for correctness of sort, and optionally,
	     : print the sorted array
 **************************************************************************/
do_cleanup()
{
  int i;
  double avg_acquire = 0.0;
  double avg_release = 0.0;
  double avg_barrier = 0.0;
  double avg_flag    = 0.0;

  if(show_array) print_array(gMem->A);
  if(verify){
    for (i = 1; i < size; i++) {
      if (gMem->A[i] != i) {
        printf( "bad sort %d\n", i);
        exit(0);
      }
    }
  }
  printf( "Congrats!! Program successful!! :-)\n");
}

/**************************************************************************
  root_main() : handles the operations to be performed before the sort is
              : initiated. Specifically, initialize the local stack, and
	      : initialize the array to be sorted, and optionally display
	      : the array. This routine also pushes the intial array into the
	      : task queue.
 ****************************************************************************/

void root_main()
{
  int i,j;
  char name[32];
  int proc_size, start;

  LocalStackTop = -1;
  /* whoami is always zero here */
  for (i = (whoami+1) * chunk -1; i > whoami * chunk; i--)    {
    LocalTaskStack[++LocalStackTop] = i;
  }

  printf("\nInitializing values\n");
  FREELOCK(&gMem->TaskStackLock);
  gMem->waitqhead = gMem->waitqtail = gMem->waitqcount = 0;
  gMem->TaskStackTop = 0;
  gMem->NumWaiting = 0;
  
  printf("Initializing data\n");
  /* initialize the data */
  for(i=0;i<size; i++)
    gMem->A[i] = i;

  printf("Shuffling data\n");
  for(i=0;i<size;i++)   {
    j=random() % size;
    SWAP(gMem->A,i,j);
  }
  if(show_array) print_array(gMem->A);

  /* Put the work in */
  PushWork_startup(0, size-1);
}

/***************************************************************************
  print_array(): prints out the values of the array A
  **************************************************************************/
void print_array(unsigned *A)
{
  int i;
  for(i=0;i<size;i++)
    printf( "\t%d", A[i]);
  printf( "\n");
  return;
}

/************************************************************************
  PushWork_startup() : Used to push the main unsorted sub-array into the
                     : task stack. Same as PushWork, except that it is not
		     : to be counted in the stats (since is is used for
		     : intialization only)
 ************************************************************************/

void PushWork_startup(int i, int j)
{
  TaskElement *te = gMem->tasks + LocalTaskStack[LocalStackTop--];
  if(LocalStackTop < 0) {
    fprintf(stderr, "We have a negative LocalTaskStackTop!\n");
  }
  te->left=i;
  te->right=j;
  te->next=NULL;
#ifdef DEBUG
  printf("Pushing Main array into stack! -- %d to %d\n ", i, j);
#endif
  gMem->TaskStack = te;
  gMem->TaskStackTop++;
#ifdef DEBUG
  printf("Stack top is now %d\n", gMem->TaskStackTop);
#endif
}

/*************************************************************************
  Worker() : The main procedure executed by each processor. This just
           : pops out a task from the task stack and sorts it, as long as
	   : the sorting is not complete.
  ************************************************************************/

void Worker()
{
  unsigned wnum;
  int curr = 0, i, last;
  int tasknum;
  TaskElement *task;
  TaskElement *taskhead;
  taskhead = gMem->tasks;
  while(1){
    if(PopWork(&task) == DONE) break;
    QuickSort(task->left, task->right);
    LocalTaskStack[++LocalStackTop]=task-taskhead;
  }
}

#ifdef DEBUG
#define ANNOUNCE(x,y) printf("%s %d \n",x,y);
#else
#define ANNOUNCE(x,y) 
#endif

/*************************************************************************
  PushWork() : The  procedure that pushes a new task onto the
             : task stack.
 *************************************************************************/

void PushWork(int i,int j)
{
  register TaskElement *te = gMem->tasks + LocalTaskStack[LocalStackTop--];
  int a;
  if (LocalStackTop < 0)
    {
      printf("!!!!!!!FAILURE IN LOCAL STACK TOP!!!\n\n\n");
      HALT(-1);
    }
  
  te->left=i;
  te->right=j;
  GETLOCK(&gMem->TaskStackLock);
  ANNOUNCE("\t\t\tI have got the lock -- ", whoami);
  a=gMem->TaskStackTop++;
  
  te->next=gMem->TaskStack;
  gMem->TaskStack=te;
  
  FREELOCK(&gMem->TaskStackLock);
  ANNOUNCE("\t\t\tI have released the lock -- ",whoami);
  ANNOUNCE("\t\t\tMy TaskStackTop was -- ",a);
  
  if(LocalStackTop < 0)
    {
      printf("Local TaskStackTop negative!\n");
    }
}
/************************************************************************
  PopWork() : This routine accesses the first task from the task stack and
            : returns DONE when the sorting is complete
 *************************************************************************/
PopWork(TaskElement **task)
{
  int i;
  BLOCKLOCK(&gMem->TaskStackLock);
  while(gMem->TaskStackTop == 0){
    if(++gMem->NumWaiting == NUM_PROCS){    /* Check for empty stack */
      gMem->TaskStackTop = -1;
      /* Something to wake up the spinning folks */
      FREELOCK(&gMem->TaskStackLock);
      return(DONE);
    }
    else {
      /* Wait for some work to get pushed on... */
      FREELOCK(&gMem->TaskStackLock);
      START_SPIN;
      while(gMem->TaskStackTop == 0) ; 
      END_AGG;
      ACQ_MEMBAR;

      BLOCKLOCK(&gMem->TaskStackLock);
      if(gMem->NumWaiting == NUM_PROCS){
        FREELOCK(&gMem->TaskStackLock);
        return(DONE);
      }
      (gMem->NumWaiting)--;
    }
  }
  *task = gMem->TaskStack;
  gMem->TaskStack=gMem->TaskStack->next;
  gMem->TaskStackTop--;
  FREELOCK(&gMem->TaskStackLock);
  return(0);
}

/*****************************************************************
  FindPivot : Identify the pivot element for quicksort
  ****************************************************************/
#define FindPivot(i,j) ((gMem->A[(i)]> gMem->A[(i)+1]) ? i : (i)+1)


/*******************************************************************
  QuickSort : Perform the quick sort operation. If size of array is
            : below the threshold, then use bubble sort, else
	    : divide the job and insert one part into the task stack
 *******************************************************************/
QuickSort(register int i, register int j)
{
  register int pivot, k;

 QSORT:
  /* pivot is index of the pivot element */
  if(j-i+1 < BubbleThresh){
    if(bubble) BubbleSort(i,j);
    else LocalQuickSort(i,j);
    return;
  }
  pivot = FindPivot(i,j);
  k = Partition(i,j,gMem->A[pivot]);
  if(k-i > j-k){
    /* The lower half [i through (k-1)] contains more entries.      */
    /* Put the other half into the queue of unsorted subarrays      */
    /* and recurse on the other half.                               */
    PushWork(k,j);
    j=k-1;
    goto QSORT;          
    /*    QuickSort(i, k-1); */ /* eliminating tail recursion */
  }
  else{
    PushWork(i,k-1);
    i=k;
    goto QSORT;
    /*     QuickSort(k,j); */ /* Eliminating tail recursion */
  }
}

/**************************************************************************
  Partition :  Partition the array between elements i and j around the
            :  specified pivot
 **************************************************************************/

Partition(int i, int j, int pivot)
{
  int left, right;
  left = i;
  right = j;
  do {
    /* fprintf(stderr, "LEFT %d Right %d\n", left, right); */
    SWAP(gMem->A, left, right);
    while(gMem->A[left] < pivot) left++;
    while( (right >= i) && (gMem->A[right] >= pivot)) right--;
  }while(left <= right);
  return (left);
}

/**************************************************************************
  LocalQuickSort and BubbleSort  :  Once the threshold is reached, use
                                 :  LocalQuickSort or BubbleSort depending
				 :  on command line option.
 **************************************************************************/

LocalQuickSort(int i, int j)
{
  int pivot, k;
LQSORT:
  if (j <= i) return;
  pivot = FindPivot(i,j);     
    
  k = Partition(i,j,gMem->A[pivot]);
  if (k > i) {
    LocalQuickSort(i, k-1);
    i=k;
    goto LQSORT;
  }
  else {
    i=i+1;
    goto LQSORT;
  }
}


BubbleSort(int i, int j)
{
  register int x,y, tmp;
  for (x = i; x < j; x++) {
    for (y = j; y > i; y--) {
      if (gMem->A[y] < gMem->A[y-1]) {
        tmp = gMem->A[y];
        gMem->A[y] = gMem->A[y-1];
        gMem->A[y-1] = tmp;
      }
    }
  }
}


