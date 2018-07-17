include ../make_common_dirs

CPPFLAGS = $(INCLUDES) -DSTORE_ORDERING

CC=cc
C++=CC


CFLAGS = -n32 -O2 -TARG:proc=r8000 
C++FLAGS = -n32 -O2 -TARG:proc=r8000 

include ../make_common_vars

OBJS = active.o branchpred.o branchqelt.o branchresolve.o capconf.o \
config.o except.o exec.o freelist.o funcs.o graduate.o inames.o \
instheap.o mainsim.o memprocess.o memunit.o pipestages.o shmalloc.o \
simio.o stallq.o startup.o state.o tagcvt.o traps.o traptable.o \
units.o act.o architecture.o associate.o pool.o cache.o cachehelp.o \
cache2.o cpu.o directory.o driver.o evlst.o globals.o l1cache.o l2cache.o \
mesh.o module.o mshr.o names.o net.o pipeline.o route.o \
setup_cohe.o smnet.o stat.o userq.o util.o wb.o wbuffer.o \
bus.o

include ../make_common_rules
