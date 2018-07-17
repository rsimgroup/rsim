#  @(#)Makefile 1.2beta 95/01/08 
# 
#  ====================================================
#  Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
# 
#  Developed at SunPro, a Sun Microsystems, Inc. business.
#  Permission to use, copy, modify, and distribute this
#  software is freely granted, provided that this notice 
#  is preserved.
#  ====================================================
# 



INCLS = -Ignuheaders -Ignuheaders/assert -Ignuheaders/gnu -Ignuheaders/io -Ignuheaders/io/sys -Ignuheaders/locale -Ignuheaders/malloc -Ignuheaders/math -Ignuheaders/misc/sys -Ignuheaders/posix/gnu -Ignuheaders/posix -Ignuheaders/posix/sys -Ignuheaders/posix -Ignuheaders/signal -Ignuheaders/stdio -Ignuheaders/stdlib -Ignuheaders/string -Ignuheaders/sys -Ignuheaders/sysdeps/generic -Ignuheaders/sysdeps/ieee754 -Ignuheaders/sysdeps/sparc -Ignuheaders/sysdeps/stub -Ignuheaders/sysdeps/unix/common -Ignuheaders/sysdeps/unix/sysv -Ignuheaders/sysdeps/unix/sysv/sysv4 -Ignuheaders/sysdeps/unix/sysv/sysv4/solaris2 -Ignuheaders/time/


CFLAGS = +w -DHAVE__SYS_SIGLIST=1 -DHAVE___CTYPE=1 -DHAVE_PSIGNAL=1  -DHAVE_sys_termios_h -D_LIBC $(INCLS) -xO4 -xtarget=ultra1/170

#-xarch=v8plus
#NOTE: We intentionally leave this out because sun linker v2.0 apparently
#doesn't link with v8+ libraries -- (gnu does, though) 7/5/97

CC = cc

.o: $(src)
	$(CC) $(CFLAGS)

src = fdopen.c feof.c ferror.c clearerr.c fileno.c newstream.c fopen.c \
freopen.c fopncook.c fmemopen.c setbuf.c setvbuf.c setbuffer.c \
setlinebuf.c fseek.c ftell.c rewind.c fgetpos.c fsetpos.c vfprintf.c \
vprintf.c printf_fp.c reg-printf.c printf-prs.c _itoa.c vsnprintf.c \
vsprintf.c vasprintf.c tempname.c fprintf.c printf.c snprintf.c \
sprintf.c asprintf.c dprintf.c vdprintf.c __vfscanf.c \
vfscanf.c vscanf.c __vsscanf.c vsscanf.c fscanf.c scanf.c sscanf.c \
fread.c fwrite.c printf_fp.c ungetc.c obstream.c fgetc.c \
getc.c getchar.c getw.c fputc.c putc.c putchar.c putw.c fgets.c gets.c \
fputs.c puts.c getdelim.c getline.c __getdelim.c __getline.c perror.c \
psignal.c tmpfile.c tmpnam.c tempnam.c fclose.c fflush.c \
memstream.c internals.c sysd-stdio.c \
pipestream.c stdio_init.c libc_fatal.c C-numeric.c add_n.c cmp.c \
divmod.c divmod_1.c lshift.c mul.c mul_1.c mul_n.c rshift.c sub_n.c \
submul_1.c dbl2mpn.c __isinf.c __isnan.c mp_clz_tab.c initstdio.c \
libfuncs.c atof.c atoi.c atol.c strtod.c strtol.c strtoul.c \
init_input.c getopt.c


obj = fdopen.o feof.o ferror.o clearerr.o fileno.o newstream.o fopen.o \
freopen.o fopncook.o fmemopen.o setbuf.o setvbuf.o setbuffer.o \
setlinebuf.o fseek.o ftell.o rewind.o fgetpos.o fsetpos.o vprintf.o \
reg-printf.o printf-prs.o _itoa.o vsnprintf.o vsprintf.o vfprintf.o \
tempname.o fprintf.o printf.o snprintf.o sprintf.o asprintf.o \
dprintf.o vdprintf.o vasprintf.o __vfscanf.o vfscanf.o vscanf.o \
__vsscanf.o vsscanf.o fscanf.o scanf.o sscanf.o fread.o fwrite.o \
printf_fp.o ungetc.o obstream.o fgetc.o getc.o getchar.o \
getw.o fputc.o putc.o putchar.o putw.o fgets.o gets.o fputs.o puts.o \
getdelim.o getline.o __getdelim.o __getline.o perror.o psignal.o \
tmpfile.o tmpnam.o tempnam.o fclose.o fflush.o memstream.o \
internals.o sysd-stdio.o pipestream.o stdio_init.o libc_fatal.o \
C-numeric.o add_n.o cmp.o divmod.o divmod_1.o lshift.o mul.o mul_1.o \
mul_n.o rshift.o sub_n.o submul_1.o dbl2mpn.o __isinf.o __isnan.o \
mp_clz_tab.o initstdio.o libfuncs.o atof.o atoi.o atol.o strtod.o \
strtol.o strtoul.o init_input.o getopt.o

all: ../librsim.a 

../librsim.a : $(obj) 
	ar cru ../librsim.a $(obj)
	ranlib ../librsim.a

../librsim_dbg.a : $(obj) 
	ar cru $@ $(obj)
	ranlib $@

clean: 
	rm -f $(obj) ../librsim.a
