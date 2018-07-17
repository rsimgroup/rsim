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

# sigmaexposed is passed in by the caller
BEGIN {refs=L1refs=L2refs=misses=procs=misslat=L2misslat=hits=L1hits= L1miss= L2hits= L2miss= Localmem=Remotemem=0}
/^Demand read/ {type=$3;
numrefsfull=$6;
paren1 = index(s,"(");
split(numrefsfull,numrefssub,"(");
typerefs = numrefssub[1];
meansfull = $8;
split(meansfull,meanssub,"/");
mean=meanssub[2];
refs += typerefs;
if (typerefs != 0)
  {
    if (type ~ /L1HIT/)
      {
	hits += typerefs;
	L1hits += typerefs;
	L1refs += typerefs;
	procs++;
      }
    else if (type ~ /L1COAL/)
      {
	L1refs += typerefs;
      }
    else if (type ~ /WBCOAL/)
      {
	misses += typerefs;
	misslat += typerefs * mean;
	L1miss += typerefs;
	L1refs += typerefs;
      }
    else if (type ~ /L2HIT/)
      {
	misses += typerefs;
	L1miss += typerefs;
	L2hits += typerefs;
	misslat += typerefs * mean;
	L1refs += typerefs;
	L2refs += typerefs;
      }
    else if (type ~ /L2COAL/)
      {
	L1miss += typerefs;
	L1refs += typerefs;
	L2refs += typerefs;
      }
    else if (type ~ /DIR_LH_NOCOHE/)
      {
	misses += typerefs;
	L1miss += typerefs;
	L2miss += typerefs;
	L2misslat += typerefs * mean;
	Localmem += typerefs;
	misslat += typerefs * mean;
	L1refs += typerefs;
	L2refs += typerefs;
	
      }
    else if (type ~ /DIR_LH_RCOHE/)
      {
	misses += typerefs;
	L1miss += typerefs;
	L2miss += typerefs;
	L2misslat += typerefs * mean;
	Remotemem += typerefs;
	misslat += typerefs * mean;
	L1refs += typerefs;
	L2refs += typerefs;
	
      }
    else if (type ~ /DIR_RH_NOCOHE/)
      {
	misses += typerefs;
	L1miss += typerefs;
	L2miss += typerefs;
	L2misslat += typerefs * mean;
	Remotemem += typerefs;
	misslat += typerefs * mean;
	L1refs += typerefs;
	L2refs += typerefs;
	
      }
    else if (type ~ /DIR_RH_LCOHE/)
      {
	misses += typerefs;
	L1miss += typerefs;
	L2miss += typerefs;
	L2misslat += typerefs * mean;
	Remotemem += typerefs;
	misslat += typerefs * mean;
	L1refs += typerefs;
	L2refs += typerefs;
	
      }
    else if (type ~ /DIR_RH_RCOHE/)
      {
	misses += typerefs;
	L1miss += typerefs;
	L2miss += typerefs;
	L2misslat += typerefs * mean;
	Remotemem += typerefs;
	misslat += typerefs * mean;	  
	L1refs += typerefs;
	L2refs += typerefs;
      }
    else if (type ~ /CACHE_TO_CACHE/)
      {
	misses += typerefs;
	L1miss += typerefs;
	L2miss += typerefs;
	L2misslat += typerefs * mean;
	Remotemem += typerefs;
	misslat += typerefs * mean;	  
	L1refs += typerefs;
	L2refs += typerefs;
      }
  }
}
END {
  printf("Misses: %d\n",misses);
  printf("Miss latency: %.2f\n",misslat / misses -1.0);
  printf("Overlapped miss latency: %.2f\n",(misslat-sigmaexposed) / misses -1.0);
  printf("Fraction unoverlapped: %.2f\\%%\n",sigmaexposed*100.0/misslat);
  printf("Refs: %d\nL1refs: %d\nL2refs: %d\n",refs,L1refs,L2refs);
  printf("L1hits: %.2f (%d)\nL1misses: %.2f (%d)\n",
	 L1hits / L1refs*100.0,L1hits,
 	 L1miss / L1refs*100.0,L1miss);
  printf("L2hits: %.2f (%d)\nL2misses: %.2f (%d)\n",
	 L2hits / L2refs*100.0,L2hits,
 	 L2miss / L2refs*100.0,L2miss);
  printf("Local mems: %.2f (%d)\nRemote mems: %.2f (%d)\n",
	 Localmem / (Localmem + Remotemem)*100.0,Localmem,
	 Remotemem / (Localmem + Remotemem)*100.0,Remotemem);
}
