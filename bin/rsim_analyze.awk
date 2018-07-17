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

BEGIN {numprocs=-1;}
/^STAT Processor:/ {numprocs++; issued[numprocs] = $7; graduated[numprocs] = $9;}
/^STAT Execution time:/ {exec[numprocs] = $4;}
/^STAT ALU time:/ {if ($5 == 0) ALU_time[numprocs] = 0; else ALU_time[numprocs] = $7 * exec[numprocs];}
/^STAT User 1 time:/ {if ($6 == 0) User_1_time[numprocs] = 0; else User_1_time[numprocs] = $8 * exec[numprocs];}
/^STAT User 2 time:/ {if ($6 == 0) User_2_time[numprocs] = 0; else User_2_time[numprocs] = $8 * exec[numprocs];}
/^STAT User 3 time:/ {if ($6 == 0) User_3_time[numprocs] = 0; else User_3_time[numprocs] = $8 * exec[numprocs];}
/^STAT User 4 time:/ {if ($6 == 0) User_4_time[numprocs] = 0; else User_4_time[numprocs] = $8 * exec[numprocs];}
/^STAT User 5 time:/ {if ($6 == 0) User_5_time[numprocs] = 0; else User_5_time[numprocs] = $8 * exec[numprocs];}
/^STAT User 6 time:/ {if ($6 == 0) User_6_time[numprocs] = 0; else User_6_time[numprocs] = $8 * exec[numprocs];}
/^STAT User 7 time:/ {if ($6 == 0) User_7_time[numprocs] = 0; else User_7_time[numprocs] = $8 * exec[numprocs];}
/^STAT User 8 time:/ {if ($6 == 0) User_8_time[numprocs] = 0; else User_8_time[numprocs] = $8 * exec[numprocs];}
/^STAT User 9 time:/ {if ($6 == 0) User_9_time[numprocs] = 0; else User_9_time[numprocs] = $8 * exec[numprocs];}
/^STAT Barrier time:/ {if ($5 == 0) Bar_time[numprocs] = 0; else Bar_time[numprocs] = $7 * exec[numprocs];}
/^STAT Spin time:/ {if ($5 == 0) Spin_time[numprocs] = 0; else Spin_time[numprocs] = $7 * exec[numprocs];}
/^STAT Acquire time:/ {if ($5 == 0) Acquire_time[numprocs] = 0; else Acquire_time[numprocs] = $7 * exec[numprocs];}
/^STAT Release time:/ {if ($5 == 0) Release_time[numprocs] = 0; else Release_time[numprocs] = $7 * exec[numprocs];}
/^STAT RMW time:/ {if ($5 == 0) RMW_time[numprocs] = 0; else RMW_time[numprocs] = $7 * exec[numprocs];}
/^STAT Write time:/ {if ($5 == 0) Write_time[numprocs] = 0; else Write_time[numprocs] = $7 * exec[numprocs];}
/^STAT Read time:/ {if ($5 == 0) Read_time[numprocs] = 0; else Read_time[numprocs] = $7 * exec[numprocs];}
/^STAT Branch time:/ {if ($5 == 0) Branch_time[numprocs] = 0; else Branch_time[numprocs] = $7 * exec[numprocs];}
/^STAT FPU time:/ {if ($5 == 0) FPU_time[numprocs] = 0; else FPU_time[numprocs] = $7 * exec[numprocs];}
/^STAT Except time:/ {if ($5 == 0) Except_time[numprocs] = 0; else Except_time[numprocs] = $7 * exec[numprocs];}
/^STAT MEMBAR time:/ {if ($5 == 0) MEMBAR_time[numprocs] = 0; else MEMBAR_time[numprocs] = $7 * exec[numprocs];}
/^STAT BUSY TIME:/ {if ($5 == 0) BUSY_time[numprocs] = 0; else BUSY_time[numprocs] = $7 * exec[numprocs];}
/^STAT Read miss time:/ {if ($6 == 0) Read_miss_time[numprocs] = 0; else Read_miss_time[numprocs] = $8 * exec[numprocs];}
/^STAT Write miss time:/ {if ($6 == 0) Write_miss_time[numprocs] = 0; else Write_miss_time[numprocs] = $8 * exec[numprocs];}
/^STAT RMW miss time:/ {if ($6 == 0) RMW_miss_time[numprocs] = 0; else RMW_miss_time[numprocs] = $8 * exec[numprocs];}
/^STAT Branch prediction rate:/ {Branch_rate[numprocs]=$5;}
/^STAT Reads Mean \(ACT\)/ {meanread_act[numprocs]=$5;}
/^STAT Writes Mean \(ACT\)/ {meanwrite_act[numprocs]=$5;}
/^STAT RMW Mean \(ACT\)/ {meanrmw_act[numprocs]=$5;}
/^STAT Reads Mean \(EA\)/ {meanread[numprocs]=$5;}
/^STAT Writes Mean \(EA\)/ {meanwrite[numprocs]=$5;}
/^STAT RMW Mean \(EA\)/ {meanrmw[numprocs]=$5;}
/^STAT Reads Mean \(ISS\)/ {meanread_iss[numprocs]=$5;}
/^STAT Writes Mean \(ISS\)/ {meanwrite_iss[numprocs]=$5;}
/^STAT RMW Mean \(ISS\)/ {meanrmw_iss[numprocs]=$5;}
/^STAT Availability/ {avail[numprocs] = $3 * exec[numprocs]; eff[numprocs] = $5*avail[numprocs]; util[numprocs] = $7*eff[numprocs];}
END {
  numprocs++;
  exec[numprocs] = 0;
  for (i=0;i< numprocs; i++)
    {
      if (exec[i] > exec[numprocs])
	exec[numprocs]=exec[i];
      avail[numprocs]+=avail[i];
      issued[numprocs]+=issued[i];
      graduated[numprocs]+=graduated[i];
      ALU_time[numprocs]+=ALU_time[i];
      User_1_time[numprocs]+=User_1_time[i];
      User_2_time[numprocs]+=User_2_time[i];
      User_3_time[numprocs]+=User_3_time[i];
      User_4_time[numprocs]+=User_4_time[i];
      User_5_time[numprocs]+=User_5_time[i];
      User_6_time[numprocs]+=User_6_time[i];
      User_7_time[numprocs]+=User_7_time[i];
      User_8_time[numprocs]+=User_8_time[i];
      User_9_time[numprocs]+=User_9_time[i];
      Bar_time[numprocs]+=Bar_time[i];
      Spin_time[numprocs]+=Spin_time[i];
      Acquire_time[numprocs]+=Acquire_time[i];
      Release_time[numprocs]+=Release_time[i];
      RMW_time[numprocs]+=RMW_time[i];
      Write_time[numprocs]+=Write_time[i];
      Read_time[numprocs]+=Read_time[i];
      Branch_time[numprocs]+=Branch_time[i];
      FPU_time[numprocs]+=FPU_time[i];
      Except_time[numprocs]+=Except_time[i];
      MEMBAR_time[numprocs]+=MEMBAR_time[i];
      BUSY_time[numprocs]+=BUSY_time[i];
      meanread[numprocs]+=meanread[i];
      meanwrite[numprocs]+=meanwrite[i];
      meanrmw[numprocs]+=meanrmw[i];
      meanread_act[numprocs]+=meanread_act[i];
      meanwrite_act[numprocs]+=meanwrite_act[i];
      meanrmw_act[numprocs]+=meanrmw_act[i];
      meanread_iss[numprocs]+=meanread_iss[i];
      meanwrite_iss[numprocs]+=meanwrite_iss[i];
      meanrmw_iss[numprocs]+=meanrmw_iss[i];
      Read_miss_time[numprocs] += Read_miss_time[i];
      Write_miss_time[numprocs] += Write_miss_time[i];
      RMW_miss_time[numprocs] += RMW_miss_time[i];
    }
  
  execmean=exec[numprocs];
  gradmean=graduated[numprocs]/numprocs;
  ALU_time[numprocs]/=numprocs*execmean;
  User_1_time[numprocs]/=numprocs*execmean;
  User_2_time[numprocs]/=numprocs*execmean;
  User_3_time[numprocs]/=numprocs*execmean;
  User_4_time[numprocs]/=numprocs*execmean;
  User_5_time[numprocs]/=numprocs*execmean;
  User_6_time[numprocs]/=numprocs*execmean;
  User_7_time[numprocs]/=numprocs*execmean;
  User_8_time[numprocs]/=numprocs*execmean;
  User_9_time[numprocs]/=numprocs*execmean;
  Spin_time[numprocs]/=numprocs*execmean;
  Bar_time[numprocs]/=numprocs*execmean;
  Acquire_time[numprocs]/=numprocs*execmean;
  Release_time[numprocs]/=numprocs*execmean;
  RMW_time[numprocs]/=numprocs*execmean;
  Write_time[numprocs]/=numprocs*execmean;
  Read_time[numprocs]/=numprocs*execmean;
  Branch_time[numprocs]/=numprocs*execmean;
  FPU_time[numprocs]/=numprocs*execmean;
  Except_time[numprocs]/=numprocs*execmean;
  MEMBAR_time[numprocs]/=numprocs*execmean;
  BUSY_time[numprocs]/=numprocs*execmean;
  eff[numprocs]= issued[numprocs]/avail[numprocs];
  util[numprocs]= graduated[numprocs]/issued[numprocs];
  avail[numprocs]/=numprocs*execmean;
  
  meanread[numprocs]/=numprocs;
  meanwrite[numprocs]/=numprocs;
  meanrmw[numprocs]/=numprocs;

  meanread_act[numprocs]/=numprocs;
  meanwrite_act[numprocs]/=numprocs;
  meanrmw_act[numprocs]/=numprocs;

  meanread_iss[numprocs]/=numprocs;
  meanwrite_iss[numprocs]/=numprocs;
  meanrmw_iss[numprocs]/=numprocs;

  Read_miss_time[numprocs]/=numprocs*execmean;
  Write_miss_time[numprocs]/=numprocs*execmean;
  RMW_miss_time[numprocs]/=numprocs*execmean;
  
  printf("%-10sIPC\tBusy\tAcq\tRel\tRead\tWrite\tRMW\tSpin\tBarrier\n","Exec time");
  printf("%-10.3g%.3f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n\n",
	 execmean,gradmean/execmean,
	 (ALU_time[numprocs]+FPU_time[numprocs]+Branch_time[numprocs]+BUSY_time[numprocs])*100.0,
	 Acquire_time[numprocs]*100.0, Release_time[numprocs]*100.0,
	 Read_time[numprocs]*100.0,
	 Write_time[numprocs]*100.0,
	 RMW_time[numprocs]*100.0,
	 Spin_time[numprocs]*100.0,
	 Bar_time[numprocs]*100.0);

  printf("USER:\t1\t2\t3\t4\t5\t6\t7\t8\t9\n");
  printf("USER:\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\t%.2f\n\n",
	 User_1_time[numprocs]*100.0,
	 User_2_time[numprocs]*100.0,
	 User_3_time[numprocs]*100.0,
	 User_4_time[numprocs]*100.0,
	 User_5_time[numprocs]*100.0,
	 User_6_time[numprocs]*100.0,
	 User_7_time[numprocs]*100.0,
	 User_8_time[numprocs]*100.0,
	 User_9_time[numprocs]*100.0);
  
  printf("LAT   :\tRD\tWT\tRMW\n");
  printf("Addr  :\t%.2f\t%.2f\t%.2f\n\n",meanread[numprocs],meanwrite[numprocs],meanrmw[numprocs]);
  printf("Issue :\t%.2f\t%.2f\t%.2f\n\n",meanread_iss[numprocs],meanwrite_iss[numprocs],meanrmw_iss[numprocs]);
  printf("Active:\t%.2f\t%.2f\t%.2f\n\n",meanread_act[numprocs],meanwrite_act[numprocs],meanrmw_act[numprocs]);

  printf("BUSY=%.2f\tALU=%.2f\tFPU=%.2f\tBRU=%.2f\n",BUSY_time[numprocs]*100.0,ALU_time[numprocs]*100.0,FPU_time[numprocs]*100.0,Branch_time[numprocs]*100.0);
  printf("READ MISS=%.2f\tWRITE MISS=%.2f\tRMW MISS=%.2f\n",Read_miss_time[numprocs]*100.0,
	 Write_miss_time[numprocs]*100.0,RMW_miss_time[numprocs]*100.0);
  printf("Avail=%.3f\tEff=%.3f\tUtil=%.3f\n\n",avail[numprocs],eff[numprocs],util[numprocs]);

  if (0)
    {
      printf("Busy times distribution\n");
      for(i=0;i<numprocs;i++){
	temp = ((ALU_time[i] + FPU_time[i])/execmean)*100.0;
	printf("Processor %3d: \t",i);
	for(j=0;j<temp;j+=2.5)
	  printf("*");
	printf("\n");
      }
    }
}


