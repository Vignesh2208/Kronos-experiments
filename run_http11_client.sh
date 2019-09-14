#!/bin/bash
mkdir -p ~/http11_logs
sudo docker run --env SERVER_IP=$1 --env NUM_TRIALS=$2 --env PERIOD=$3 --env NUM_THREADS=$4 -v ~/http11_logs:/tmp vign2208/http11_client
