/*
  mainsim.cc

  Startup file for the simulator.
  
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


#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/time.h>

#include <signal.h>
#include <unistd.h>
#include <values.h>

#include <sys/utsname.h>

#include "Processor/instruction.h"
#include "Processor/state.h"
#include "Processor/decode.h"
#include "Processor/memory.h"
#include "Processor/mainsim.h"
#include "Processor/memprocess.h"
#include "Processor/traps.h"
#include "Processor/simio.h"
#include "Processor/units.h"
extern "C"
{
#include "MemSys/simsys.h"
#include "MemSys/cache.h"
#include "MemSys/arch.h"
#include "MemSys/mshr.h"
#include "MemSys/net.h"
#include "MemSys/bus.h"
#include "MemSys/directory.h"
#include "MemSys/misc.h"
}

/***********************************************************************/
/******************** variable declarations ****************************/
/***********************************************************************/


int MAX_ACTIVE_NUMBER = 128; /* this means 64 instructions */
int MAX_ACTIVE_INSTS;
int MAX_SPEC = 8;
int NO_OF_DECODES_PER_CYCLE = 4;
int NO_OF_GRADUATES_PER_CYCLE = -1;
int NO_OF_EXCEPT_FLUSHES_PER_CYCLE = -1;
int soft_rate = 0;
int STALL_ON_FULL = 0;
int MAX_MEM_OPS = DEFMAX_MEM_OPS;   /* defined in state.h = 32 */
int MAX_ALUFPU_OPS = 0;
int NUM_WINS = 8;
SpecStores spec_stores = SPEC_EXCEPT;
int Speculative_Loads = 0;
int SC_NBWrites = 0;
int Processor_Consistency = 0;

int drop_all_sprefs = 0;

int INTERLEAVING_FACTOR = 4; /* interleaving of memory */

instr *instr_array;
int num_instructions;
int max_cycles=500000000;

int FASTER_PROC=1;
int FASTER_PROC_L1=1;
int FASTER_NET = 1;

int DEBUG_TIME;
FILE *corefile;

int FAST_UNITS = 0;

int MemWarnings = 1;

int Prefetch=0;
int PrefetchWritesToL2=0;
double parelapsedtime;

int partial_stats_time = 3600;

/* Static scheduling variable intiailization */
int stat_sched=0;
int simulate_ilp = 1;

/***********************************************************************/
/* alarmhandler: determines what to do on an alarm signal              */
/*             : can be machine specific                               */
/*             : prints out partial statistics                         */
/***********************************************************************/

#if defined(SIGNAL_4ARGS) /* use this for SunOS with gcc */
extern "C" void alarmhandler(int,int,sigcontext *, char *)
#elif defined(SIGNAL_DOTS) /* use this for SunOS with cc */
extern "C" void alarmhandler(int,...)
#else /* use this for Solaris, Irix, HP-UX, ... */
extern "C" void alarmhandler(int)
#endif
{
  for (int i=0; i<state::numprocs;i++)
    {
      state *proc;
      state::AllProcessors->PeekElt(proc,i);
      proc->time_to_dump=1;  /* Note: We are setting a flag here as this is an
				asynch signal, and we may get incomplete stats
				if we spew out the entire stats right here
				This flag is used in the graduate cycle to
				print out stats */
    }
#if defined(USESIGNAL) /* use this for HP (won't hurt for SunOS) */
  signal(SIGALRM,alarmhandler);
#endif
  alarm(partial_stats_time); /* Reschedule alarm after time step */
}


/***********************************************************************/
/* segvhandler : determines what to do on an segv signal               */
/*             : can be machine specific                               */
/***********************************************************************/

#if defined(SIGNAL_4ARGS) /* use this for SunOS with gcc */
extern "C" void segvhandler(int,int,sigcontext *, char *)
#elif defined(SIGNAL_DOTS) /* use this for SunOS with cc */
extern "C" void segvhandler(int,...)
#else /* use this for Solaris, Irix, HP-UX, ... */
extern "C" void segvhandler(int)
#endif
{
#if !defined(USESIGNAL) /* use this for Solaris, IRIX */
  sigset(SIGSEGV,SIG_IGN);
#else /* define USESIGNAL for HP, SunOS */
  signal(SIGSEGV,SIG_IGN);
#endif
  fprintf(simerr,"RECEIVED A SEG FAULT!!!!!\n");
  exit(-1);
  DriverInterrupt();
}


/***********************************************************************/
/* fpfailure   : determines what to do on an fperr signal, which may   */
/*             : arise because of an exception in the simulator or     */
/*             : the simulated application                             */
/*             : can be machine specific                               */
/***********************************************************************/

int fpfailed=0;
int fptest=0;

#if defined(SIGNAL_4ARGS) /* use this for SunOS with gcc */
extern "C" void fpfailure(int,int,sigcontext *, char *)
#elif defined(SIGNAL_DOTS) /* use this for SunOS with cc */
extern "C" void fpfailure(int,...)
#else /* use this for Solaris, Irix, HP-UX, ... */
extern "C" void fpfailure(int)
#endif
{
  if (fptest)
    fpfailed=1;
  else // INTERNAL RSIM error
    {
      fprintf(simerr,"Internal RSIM floating point error!\n");
      exit(-SIGFPE-128);
    }
  
#if defined(USESIGNAL) /* use for HP; optional for SunOS */
  signal(SIGFPE,fpfailure);
#endif
}


char *args[]={"a.out",NULL};

/***********************************************************************/
/* read_instructions :  read the list of instructions from the         */
/*                   :  predecoded file onto the instruction array     */
/*                   :  can be machine-format-dependent                */
/***********************************************************************/


int read_instructions()
{
  struct stat buf;
  char filename[80];
  strcpy(filename,args[0]);
  strcat(filename,".dec");
  if (stat(filename,&buf))
    return 0;

  int ninstrs = buf.st_size / sizeof(instr);
  instr_array = new instr[ninstrs+10];
  FILE *fp=fopen(filename,"r");
  if (fp == NULL)
    return 0;
  fread((char *)instr_array,sizeof(instr),ninstrs,fp);
  fclose(fp);
  return ninstrs;
}

HashTable<unsigned, unsigned> *SharedPageTable;

extern char *optarg;
extern int optind;

char *mailto = NULL, *subject = NULL;
char *fname0 = NULL, *fname1 = NULL, *fname2 = NULL, *fname3 = NULL, *fname4 = NULL;
char arr1[1024],arr2[1024],arr3[1024];
char *dirname = NULL;


/***********************************************************************/
/* before_exit : Called before exiting; prints out an exit message     */
/*             : and possibly mails a completion notification          */
/***********************************************************************/

#if defined(ONEXIT) /* use for SunOS */
extern "C" void before_exit(int s,...)
{
  fprintf(simerr, "About to exit with status %d after time %d\n",s,(int)YS__Simtime);
#else
extern "C" void before_exit()
{
  fprintf(simerr, "About to exit after time %d\n",(int)YS__Simtime);
#endif
  fflush(simout);
  fflush(simerr);
  if (mailto && fname1 && fname2)
    {
      char sbuf[1024];
      if (!subject)
	subject="RSIM_res";
      // mail a completion notification
      char *out=tmpnam(NULL);
      FILE *fp = fopen((const char *)out,"w");
      struct utsname un;
      uname(&un);
      fprintf(fp,"Processor %s:\n",un.nodename);

      fprintf(fp,"RSIM %s has completed\n",subject);
      fprintf(fp,"Application output file: %s\n\n",fname1);
      fprintf(fp,"Simulator detailed statistics: %s\n\n",fname3);
      fprintf(fp,"Simulator concise statistics: %s\n\n",fname2);
      fclose(fp);
      
      sprintf(sbuf,"mailx -s %s %s < %s",subject,mailto,out);
      system(sbuf);
      unlink(out);
    }
}

/***********************************************************************/
/* mem_map1    : defines a hashing function used in PageTable          */
/***********************************************************************/
     
static unsigned mem_map1(unsigned k, unsigned sz)
{
  return k&(sz-1);
}

/***********************************************************************/
/* mem_map2    : defines a hashing function used in PageTable          */
/*             : can be machine specific                               */
/***********************************************************************/



static unsigned mem_map2(unsigned k,unsigned sz) // a ROL of 13 or some such
{
  return (((k<< 13) | (k >> (BITS(unsigned)-13))) & (sz-1))*2+1;
}


/***********************************************************************/
/* UserMain    : the main procedure (in YACSIM programs); mainly sets  */
/*             : variables, parses the command line, and calls         */
/*             : initialization functions                              */
/***********************************************************************/

extern "C" void UserMain(int argc, char **argv)
{
  int c1;
  char c;
  char filename[80];

  /* Set the function to be called before exit */
#if defined(ONEXIT) /* SunOS */
  on_exit(before_exit,NULL);
#else /* Solaris, Irix, HP-UX */
  atexit(before_exit);
#endif

  double max_driver_time = 0.0; // forever
  int fd;

  DEBUG_TIME = 0;

  lowsimmed = lowshared; // initialize this for specifying which addresses go to MemSim and which don't 
  
  /*******************************************************************/
  /* Parse command line and initialize variables                     */
  /*******************************************************************/
  
  while ((c1=getopt(argc,argv,"D:S:0:1:2:3:z:e:A:c:t:f:i:a:uU:g:w:E:G:Xq:m:L:pPJKN6H:TxkF:y:nWh")) != -1)
    {
      /* USED:                            UNUSED:  
	 01236			  
	 ADEFGHJKLNPSTUWXZ	          BCIMOQRVY
	 acefghikmnpqtuwxyz               bdjlorsv */
      
      c=c1;
      switch(c)
	{
	case 'D': // D and S are used to redirect all files
	  dirname = optarg;
	  break;
	case 'S':
	  subject = optarg;
	  break;
	case '0': // redirect stdin
	  fname0=optarg;
	  break;
	case '1': // redirect stdout
	  fname1=optarg;
	  break;
	case '2': // redirect stderr
	  fname2=optarg;
	  break;
	case '3': // redirect simout separately from stdout
	  fname3=optarg;
	  break;
	case 'z': // redirect simin separately from stdin
	  fname4=optarg;
	  break;
	case 'e':
	  mailto = optarg;
	  break;
	  
	case 'A': // ALARM TIME, in minutes
	  partial_stats_time=60*atoi(optarg); // convert to seconds
	  break;
	case 'c': // # of max Cycles to run
	  max_driver_time=atof(optarg); // cycles=atoi(optarg);
	  break;
	case 't':
	  DEBUG_TIME = atoi(optarg);
	  break;
	  
	case 'f': // File to interpret (without the .out or .dec suffix)
	  strcpy(filename,optarg);
	  strcat(filename,".out");
	  args[0]=filename;
	  break;

	case 'i': // instruction issue width (# of issues per cycle)
	  NO_OF_DECODES_PER_CYCLE=atoi(optarg);
	  break;
	case 'a': // # of Active instructions, or window size
	  MAX_ACTIVE_NUMBER=2*atoi(optarg);
	  if (MAX_ACTIVE_NUMBER > MAX_MAX_ACTIVE_NUMBER)
	    {
	      fprintf(simerr,"Too many active instructions; going to size %d\n",
		      MAX_MAX_ACTIVE_NUMBER/2);
	      MAX_ACTIVE_NUMBER = MAX_MAX_ACTIVE_NUMBER;
	    }
	  break;
	case 'u':
	  FAST_UNITS=1;
	  break;
	case 'g': // # of graduates per cycle
	  NO_OF_GRADUATES_PER_CYCLE=atoi(optarg);
	  break;
	case 'E':
	  NO_OF_EXCEPT_FLUSHES_PER_CYCLE=atoi(optarg);
	  break;
	case 'q': // stall on issue queue, memory queue full conditions
	  STALL_ON_FULL = 1;
	  sscanf(optarg,"%d,%d",&MAX_ALUFPU_OPS,&MAX_MEM_OPS);
	  break;
	case 'X': // static scheduling -- supported only with RC, discussed in manual
	  STALL_ON_FULL = 1;
	  MAX_ALUFPU_OPS = 1; /* stall as soon as first one won't
				 be able to issue */
	  stat_sched=1; /* static scheduling is like STALL_ON_FULL, except
			   that address generation is also counted with
			   the ALU/FPU counter */
	  break;
	case 'm': // # of outstanding Mem ops, if its used
	  MAX_MEM_OPS=atoi(optarg);
	  break;
	case 'L':
	  spec_stores=SpecStores(atoi(optarg)); /* 0 -- stall, 1 -- limbo, 2 -- except */
	  break;
	case 'p':
	  Prefetch=1;
	  break;
	case 'P':
	  Prefetch=1;
	  PrefetchWritesToL2=1;
	  break;
	case 'J': // prefetches "jump" over L1
	  Prefetch=2;
	  break;
	case 'K':
	  Speculative_Loads=1;
	  break;
#ifdef STORE_ORDERING
	case 'N':
	  SC_NBWrites=1;
	  break;
	case '6':
	  Processor_Consistency=1;
	  break;
#endif

	case 'H': // number of MSHRS
	  sscanf(optarg,"%d,%d",&L1_MAX_MSHRS,&L2_MAX_MSHRS);
	  break;
	case 'T':
	  DISCRIMINATE_PREFETCH=1;
	  break;
	case 'x':
	  drop_all_sprefs=1;
	  break;

	case 'k':
	  simulate_ilp=0;
	  break;
	case 'F': // faster processor
	  FASTER_PROC=atoi(optarg);
	  break;
	case 'y': // faster processor
	  FASTER_PROC_L1=atoi(optarg);
	  break;
	case 'n':
	  // this is used when you want to sim private accesses too
	  // currently supported only for UP runs
	  lowsimmed = 0;
	  MemWarnings = 0;
	  break;
	  
	case 'W':
	  MemWarnings = 0;
	  break;
	case 'h':
	default:
	  fprintf(simerr,"Please refer to the RSIM manual for a detailed\ndescription of the RSIM command line options.\n");
	  return;
	  break;
	}
    }

  if (!simulate_ilp) // if ILP simulation is turned off
    {
      MAX_ACTIVE_NUMBER=2; // -a1
      NO_OF_DECODES_PER_CYCLE=1; // -i1
      NO_OF_GRADUATES_PER_CYCLE=1; // -g1
      INSTANT_ADDRESS_GENERATE=1; // also give this the benefit of instant address generation
      MAX_MEM_OPS = 1; // should show these effects
      ALU_UNITS = FPU_UNITS = ADDR_UNITS = 1;
      L1_NUM_PORTS = 1;
      L1TAG_PORTS[CACHE_PORTSENTRY] = 1;
      MEM_UNITS = 1;
    }

  MAX_ACTIVE_INSTS = MAX_ACTIVE_NUMBER/2;
  
  if (NO_OF_GRADUATES_PER_CYCLE == 0)
    {
      NO_OF_GRADUATES_PER_CYCLE = MAX_ACTIVE_NUMBER;
    }
  else if (NO_OF_GRADUATES_PER_CYCLE == -1)
    {
      NO_OF_GRADUATES_PER_CYCLE = NO_OF_DECODES_PER_CYCLE;
    }
  
  if (NO_OF_EXCEPT_FLUSHES_PER_CYCLE == 0)
    {
      // leave it like this so we won't delay
    }
  else if (NO_OF_EXCEPT_FLUSHES_PER_CYCLE == -1)
    {
      NO_OF_EXCEPT_FLUSHES_PER_CYCLE = NO_OF_GRADUATES_PER_CYCLE;
    }

  soft_rate = NO_OF_EXCEPT_FLUSHES_PER_CYCLE; // kept separate if desired
  
  if (dirname && subject)
    {
      strcpy(arr1,dirname);
      if (*(strrchr(dirname,'/')+1) != '\0')
	strcat(arr1,"/");
      strcat(arr1,subject);
      fname1=arr1; /* Right now, this is the base name -- add _out below */

      strcpy(arr2,arr1);
      strcat(arr2,"_err");
      fname2=arr2;

      strcpy(arr3,arr1);
      strcat(arr3,"_stat");
      fname3=arr3;

      strcat(fname1,"_out");

#ifdef COREFILE
      fprintf(simerr,"We'll redirect stdout to %s\n",fname1);
      fprintf(simerr,"We'll redirect stderr to %s\n",fname2);
      fprintf(simerr,"We'll redirect simout to %s\n",fname3);
#endif
    }
  
  if (fname0)
    {
      fd=open(fname0,O_RDONLY);
      if (dup2(fd,0) < 0)
	{
	  fprintf(simerr,"Failure redirecting stdin to %s\n",fname0);
	  exit(-1);
	}
    }
  
  if (fname1)
    {
      fd=open(fname1,O_WRONLY|O_CREAT|O_TRUNC,0666);
      if (dup2(fd,1) < 0)
	{
	  fprintf(simerr,"Failure redirecting simout to %s\n",fname1);
	  exit(-1);
	}
    }
  if (fname2)
    {
      fd=open(fname2,O_WRONLY|O_CREAT|O_TRUNC,0666);
      if (dup2(fd,2) < 0)
	{
	  fprintf(simerr,"Failure redirecting simerr to %s\n",fname2);
	  exit(-1);
	}
    }

  SimIOInit();
  if (fname3)
    {
      /* redirect simout separately from stdout */
      RedirectSimIO(1,fname3);
    }
  else
    {
      fname3=fname1; /* simout same as stdout */
    }

  if (fname4)
    {
      /* redirect simin separately from stdin */
      RedirectSimIO(0,fname4);
    }

  fprintf(simerr,"RSIM command line: ");
  for (int ac=0; ac<argc; ac++)
    {
      fprintf(simerr,"%s ",argv[ac]);
    }
  fprintf(simerr,"\n");

  /* now that config file is chosen, read it */
  if (!(fname0 || fname4)) /* input has not been redirected */
    {
      fprintf(simerr,"Please enter RSIM configuration file options. Enter\n");
      fprintf(simerr,"STOPCONFIG 1\n");
      fprintf(simerr,"on a separate line in order to stop specifying configuration.\n");
    }
  ParseConfigFile();
  
  fprintf(simerr,"RSIM:\tActive list: %d\n",MAX_ACTIVE_INSTS);
  fprintf(simerr,"\tSpeculations: %d\n",MAX_SPEC);
  fprintf(simerr,"\tIssue rate: %d\n",NO_OF_DECODES_PER_CYCLE);
  fprintf(simerr,"\tMax memory queue size: %d\n",MAX_MEM_OPS);
  fprintf(simerr,"\n\n");
  
  state::AllProcessors = new circq<state *>(MAX_MEMSYS_PROCS);
  SharedPageTable = new HashTable<unsigned, unsigned> (mem_map1,mem_map2);
  
#if defined(USESIGNAL) /* HP-UX, SunOS */
  signal(SIGALRM,alarmhandler);
  signal(SIGFPE,fpfailure);
#else /*Solaris, Irix, HP-UX */
  sigset(SIGALRM,alarmhandler);
  sigset(SIGFPE,fpfailure);
#endif
  
  signal(SIGSEGV,segvhandler);
  /* since this is for internal failures, no need to use sigset */
  
  argv[optind-1]=filename;

  /********************************************************************/
  /* Read the instructions from decoded binary into instruction array */
  /********************************************************************/

  int num = read_instructions();

  num_instructions = num;

  if (num <= 0)
    {
      fprintf(simerr,"Error with this file\n");
      exit(-1);
    }
  
#ifdef COREFILE
  fprintf(simerr,"Instructions: %d\n",num);
#endif


  /******************************************************************/
  /* Set up the table with the units and functions corresponding to */
  /* this architecture and instruction-set-architecture             */
  /******************************************************************/
  
  UnitArraySetup();
  FuncTableSetup();
  TrapTableInit();

  /********************************************************************/
  /*Initialize the sys architecture; defined in MemSys/architecture.c */
  /********************************************************************/
 
  SystemInit();

  /********************************************************************/
  /* Initialize the uniprocessor architecture of Processor/state.c    */
  /********************************************************************/
			
  state *pptr = new state; // &proc;

  corefile = pptr->corefile; //  initial value.... fopen("corefile","w");

  alarm(partial_stats_time); // start off the stats 
  
  /**********************************************************************/
  /* startup routine -- sets up the simulated stack, heap appropriately */
  /**********************************************************************/
  

  if (startup(argv+optind-1,pptr) == -1)
    {
      fprintf(simerr,"Error with this file\n");
      exit(-1);
    }

  /****************************************************************/
  /* *************** call main RSIM_EVENT *********************** */
  /****************************************************************/
  

  EVENT *rsim_event = NewEvent("RSIM Process -- all processors",RSIM_EVENT,NODELETE,0);
  ActivitySchedTime((ACTIVITY *)rsim_event,0.5,INDEPENDENT);

  /* This is started at 0.5 to make sure that the smnet requests for
     this time are handled _before_ the processor is handled. This is
     to avoid spurious cases when an extra cycle is added in just for
     the processor to see the smnet request */
  
  /* start the YACSIM driver */

  double elapsedtime = time(0);
  DriverRun(max_driver_time);

#ifdef DEBUG_POOL
   YS__PoolStats(&YS__MsgPool);
   YS__PoolStats(&YS__EventPool);
   YS__PoolStats(&YS__QueuePool);
   YS__PoolStats(&YS__SemPool);
   YS__PoolStats(&YS__QelemPool);
   YS__PoolStats(&YS__StatPool);
   YS__PoolStats(&YS__HistPool);
   YS__PoolStats(&YS__PrcrPool);
   YS__PoolStats(&YS__PktPool);
   
   YS__PoolStats(&YS__CachePool);
   YS__PoolStats(&YS__BusPool);
   YS__PoolStats(&YS__ModulePool);
   YS__PoolStats(&YS__ReqPool);
   YS__PoolStats(&YS__SMPortPool);
   YS__PoolStats(&YS__DirstPool);
   YS__PoolStats(&YS__DirEPPool);
#endif
  
#if defined(USESIGNAL)
  signal(SIGALRM,SIG_IGN);
#else
  sigset(SIGALRM,SIG_IGN);
#endif
  

  int exit_code = pptr->exit;

  if (exit_code == 1)
    exit_code = 0;
  
#ifdef COREFILE
  fprintf(corefile,"\n\nSIMULATION EXITED AT TIME %d WITH CODE %d\n",pptr->curr_cycle,exit_code);
#endif
  
  elapsedtime = time(0) - elapsedtime;
  fprintf(simerr,"Total elapsed time of simulation %f\n",elapsedtime);
  fprintf(simerr,"Simulated time %f\n",GetSimTime());
  while (state::AllProcessors->Delete(pptr))
    {
      pptr->report_phase();
#ifdef COREFILE
      fflush(pptr->corefile);
      fclose(pptr->corefile);
      pptr->corefile=NULL;
#endif
    }

  fflush(simout);
  fflush(simerr);
  return;
}





