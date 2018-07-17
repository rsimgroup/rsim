/**************************************************************************
  quicksort.h -- header file for the quicksort application code
  ****************************************************************************/

#ifndef _qs_h_
#define _qs_h_ 1

/****************************************************************/
/* Include files */

/* Global variables */
#define MAX_SORT_SIZE  1024*1024
#define DEFAULT_SIZE   4096
#define MAX_TASK_QUEUE 16*1024
#define DEFAULT_BUBBLE 64
#define BEEP           7
#define DONE           (-1) 

/* Macro to swap i and j elements of array A */
#define SWAP(A,i,j) {register int tmp35325; \
tmp35325=(A)[(i)];(A)[(i)]=(A)[(j)];(A)[(j)]=tmp35325;}


/********** Task queue element structure definition *************************
  -- contains beginning and ending entries and a pointer to the next element
  ***************************************************************************/
typedef struct pq {
  unsigned left;
  unsigned right;
  struct pq *next;
} TaskElement;

/**********************************************************************
  Global memory used in the quick sort program
  ********************************************************************/
typedef struct GlobalMemory {
  int pad0[32];

  /*** Note the use of volatile declaration for synchronizing variable,
       as well as the use of padding to prevent undue false sharing
       effects  ***/       
  volatile unsigned TaskStackTop;
  volatile int waitqhead;
  volatile int waitqtail;
  volatile int waitqcount;
  int pad1[32];
  volatile int TaskStackLock;
  int padxx[32];
  TaskElement *TaskStack;
  int padx[32];
  volatile unsigned NumWaiting;
  int pad2[32];
  unsigned A[MAX_SORT_SIZE];       
  int pad3[32];
  TaskElement tasks[MAX_TASK_QUEUE];
} GlobalMemory;

int LocalTaskStack[MAX_TASK_QUEUE];
int LocalStackTop;

/******************* Function declarations ***************************/
void root_main();
void Worker();
void PushWork(int i, int j);
void PushWork_startup(int i, int j);
void print_array(unsigned *A);
void Signal();
void Wait();

extern int NUM_PROCS, size, BubbleThresh;
extern GlobalMemory *gMem;
extern int bubble;

#endif
