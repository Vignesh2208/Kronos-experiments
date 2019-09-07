#!/bin/bash
declare -a StringArray=("timeslice_1ms" "timeslice_10ms" "timeslice_1us" "timeslice_10us" "timeslice_100us" )
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
# Iterate the string array using for loop
for val in ${StringArray[@]}; do
   SubDir=$DIR/sysbench/kronos/$val
   echo 'Processing: ' $SubDir 
   grep -nr "total time:" $SubDir/* | awk '{print $4'} > $DIR/sysbench/kronos/$val.txt

   SubDir=$DIR/sysbench/timekeeper/$val
   echo 'Processing: ' $SubDir 
   grep -nr "total time:" $SubDir/* | awk '{print $4'} > $DIR/sysbench/timekeeper/$val.txt
done
