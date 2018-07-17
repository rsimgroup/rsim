include ../make_common_dirs

CPPFLAGS = $(INCLUDES) -DSTORE_ORDERING

CC = cc
C++ = CC


CFLAGS = -xO2 -xtarget=ultra1/140 -fsimple -libmil -dalign +w
C++FLAGS = -O4 -xtarget=ultra1/140 -fsimple -libmil -dalign -unroll=4 +w

include ../make_common_vars

PD_OBJS = $(PREDECODE_SRCFILES:$(PREDECODE_SRCDIR)/%.cc=%.o)
OBJS = $(PROC_SRCFILES:$(PROC_SRCDIR)/%.cc=%.o) $(MEMSYS_SRCFILES:$(MEMSYS_SRCDIR)/%.c=%.o)

include ../make_common_rules
