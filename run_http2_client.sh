#!/bin/bash
mkdir -p ~/http2_logs
sudo docker run --env SERVER_IP=$1 --env NUM_TRIALS=$2 --env PERIOD=$3  -v ~/http2_logs:/tmp vign2208/http2_client
