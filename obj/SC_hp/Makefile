include ../make_common_dirs

INCLUDES = -I$(HOME)/incl

CPPFLAGS = -DUSESIGNAL -DUNELF -DNO_IEEE_FP $(INCLUDES) -DSTORE_ORDERING

CC=gcc
C++=g++

CFLAGS = -O1 -lV3
C++FLAGS = -O3 -lV3

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

include ../make_noelf_rules
