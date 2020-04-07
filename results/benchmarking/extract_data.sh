#!/bin/bash
declare -a StringArray=("timeslice_1ms" "timeslice_10ms" "timeslice_1us" "timeslice_10us" "timeslice_100us" )
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
# Iterate the string array using for loop
for val in ${StringArray[@]}; do
   SubDir=$DIR/sysbench/kronos/performance/$val/run_1
   echo 'Processing: ' $SubDir 
   grep -nr "total time:" $SubDir/* | awk '{print $4'} > $DIR/sysbench/kronos/$val.txt

   SubDir=$DIR/sysbench/timekeeper/performance/$val/1_process
   echo 'Processing: ' $SubDir 
   grep -nr "total time:" $SubDir/* | awk '{print $4'} > $DIR/sysbench/timekeeper/$val.txt
done
declare -a NumProcessArray=("1_process" "20_process" "50_process")
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
# Iterate the string array using for loop
for val in ${StringArray[@]}; do
   for num_process in ${NumProcessArray[@]}; do
	   SubDir=$DIR/sysbench/kronos/overhead_ratio/$val/$num_process
	   echo 'Processing: ' $SubDir 
	   grep -nr "Mu Elapsed time" $SubDir/* | awk '{print $5'} > $DIR/sysbench/kronos/$val-$num_process.txt
	   grep -nr "overhead ratio:" $SubDir/* | awk '{print $3}' > $DIR/sysbench/kronos/$val-$num_process-overhead_ratio.txt

	   SubDir=$DIR/sysbench/timekeeper/overhead_ratio/$val/$num_process
	   echo 'Processing: ' $SubDir 
	   grep -nr "Mu Elapsed time" $SubDir/* | awk '{print $5'} > $DIR/sysbench/timekeeper/$val-$num_process.txt
	   grep -nr "overhead ratio:" $SubDir/* | awk '{print $3}' > $DIR/sysbench/timekeeper/$val-$num_process-overhead_ratio.txt
    done
done
