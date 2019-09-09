#!/bin/bash
declare -a StringArray=("timeslice_1ms" "timeslice_10ms" "timeslice_1us" "timeslice_10us" "timeslice_100us" )
declare -a NumProcessArray=("1_process" "10_process" "20_process" "50_process")
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
# Iterate the string array using for loop
for val in ${StringArray[@]}; do
   for num_process in ${NumProcessArray[@]}; do
	   SubDir=$DIR/results/kronos/$val/$num_process
	   echo 'Processing: ' $SubDir 
	   grep -nr "Mu Elapsed time" $SubDir/* | awk '{print $5'} > $DIR/results/kronos/$val-$num_process.txt
	   grep -nr "overhead ratio:" $SubDir/* | awk '{print $3}' > $DIR/results/kronos/$val-$num_process-overhead_ratio.txt

	   SubDir=$DIR/results/timekeeper/$val/$num_process
	   echo 'Processing: ' $SubDir 
	   grep -nr "Mu Elapsed time" $SubDir/* | awk '{print $5'} > $DIR/results/timekeeper/$val-$num_process.txt
	   grep -nr "overhead ratio:" $SubDir/* | awk '{print $3}' > $DIR/results/timekeeper/$val-$num_process-overhead_ratio.txt
    done
done
