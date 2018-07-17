#############################################################################
# This file is part of RSIM Utilities                                       #
#                                                                           #
######################## LICENSE TERMS AND CONDITIONS #######################
#                                                                           #
#  Copyright Notice                                                         #
#       1997 Rice University                                                #
#                                                                           #
#  1. The "Software", below, refers to RSIM (Rice Simulator for ILP         #
#  Multiprocessors) version 1.0 and includes the RSIM Simulator, the        #
#  RSIM Applications Library, Example Applications ported to RSIM,          #
#  and RSIM Utilities.  Each licensee is addressed as "you" or              #
#  "Licensee."                                                              #
#                                                                           #
#  2. Rice University is copyright holder for the RSIM Simulator and RSIM   #
#  Utilities. The copyright holders reserve all rights except those         #
#  expressly granted to the Licensee herein.                                #
#                                                                           #
#  3. Permission to use, copy, and modify the RSIM Simulator and RSIM       #
#  Utilities for any non-commercial purpose and without fee is hereby       #
#  granted provided that the above copyright notice appears in all copies   #
#  (verbatim or modified) and that both that copyright notice and this      #
#  permission notice appear in supporting documentation. All other uses,    #
#  including redistribution in whole or in part, are forbidden without      #
#  prior written permission.                                                #
#                                                                           #
#  4. The RSIM Applications Library is free software; you can               #
#  redistribute it and/or modify it under the terms of the GNU Library      #
#  General Public License as published by the Free Software Foundation;     #
#  either version 2 of the License, or (at your option) any later           #
#  version.                                                                 #
#                                                                           #
#  The Library is distributed in the hope that it will be useful, but       #
#  WITHOUT ANY WARRANTY; without even the implied warranty of               #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU         #
#  Library General Public License for more details.                         #
#                                                                           #
#  You should have received a copy of the GNU Library General Public        #
#  License along with the Library; if not, write to the Free Software       #
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,    #
#  USA.                                                                     #
#                                                                           #
#  5. LICENSEE AGREES THAT THE EXPORT OF GOODS AND/OR TECHNICAL DATA FROM   #
#  THE UNITED STATES MAY REQUIRE SOME FORM OF EXPORT CONTROL LICENSE FROM   #
#  THE U.S.  GOVERNMENT AND THAT FAILURE TO OBTAIN SUCH EXPORT CONTROL      #
#  LICENSE MAY RESULT IN CRIMINAL LIABILITY UNDER U.S. LAWS.                #
#                                                                           #
#  6. RICE UNIVERSITY NOR ANY OF THEIR EMPLOYEES MAKE ANY WARRANTY,         #
#  EXPRESS OR IMPLIED, OR ASSUME ANY LEGAL LIABILITY OR RESPONSIBILITY      #
#  FOR THE ACCURACY, COMPLETENESS, OR USEFULNESS OF ANY INFORMATION,        #
#  APPARATUS, PRODUCT, OR PROCESS DISCLOSED AND COVERED BY A LICENSE        #
#  GRANTED UNDER THIS LICENSE AGREEMENT, OR REPRESENT THAT ITS USE WOULD    #
#  NOT INFRINGE PRIVATELY OWNED RIGHTS.                                     #
#                                                                           #
#  7. IN NO EVENT WILL RICE UNIVERSITY BE LIABLE FOR ANY DAMAGES,           #
#  INCLUDING DIRECT, INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES          #
#  RESULTING FROM EXERCISE OF THIS LICENSE AGREEMENT OR THE USE OF THE      #
#  LICENSED SOFTWARE.                                                       #
#                                                                           #
#############################################################################


LD = /usr/ccs/bin/ld 		# Explicitly specify Sun linker
LDFLAGS =

AS = /usr/ccs/bin/as		# Explicitly specify Sun assembler
ASFLAGS = -xarch=v8plus

all: crt0.obj librsim.a

# Currently, we can't have a sync library, so we're just going
# merge those files into crt0.obj. At some point, add libsync.a
# above. (more details in sync makefile)

install: all libc.a

crt0.obj: crt.o libsync.o
	$(LD) -o crt0.obj -r crt.o libsync.o

crt.o: crt0.s_permanent
	$(AS) $(ASFLAGS) -o crt.o crt0.s_permanent

# See sync makefile for why there's no libsync.a right now
libsync.a:
	cd sync; $(MAKE) ../libsync.a

libsync.o:
	cd sync; $(MAKE) ../libsync.o

librsim.a:
	cd librsim; $(MAKE) ../librsim.a

libc.a:
	cp /usr/lib/libc.a .
	-ar d libc.a fprintf.o printf.o fwrite.o open.o read.o close.o write.o lseek.o sbrk.o fopen.o fseek.o fclose.o scanf.o fscanf.o sscanf.o sprintf.o atof.o strtod.o fputs.o fflush.o errno.o rewind.o atexit.o exit.o abort.o cuexit.o thr_stub.o 
	ranlib libc.a

clean:
	rm -f crt0.obj crt.o
	cd sync; $(MAKE) clean
	cd librsim; $(MAKE) clean
