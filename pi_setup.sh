#!/bin/bash


git clone --single-branch --branch pi_scripts https://github.com/gopchandani/qos_synthesis /home/pi

ETH0_MAC=$(cat /sys/class/net/vmnet8/address)
echo 'ETH0 MAC = ' $ETH0_MAC
ETH0_STATIC_IP=$(grep -nr $ETH0_MAC $HOME/qos_synthesis/arp_table_fix.sh | awk '{print $3}')
echo 'ETH0 STATIC IP = ' $ETH0_STATIC_IP

sudo sed "s/@IP@/$ETH0_STATIC_IP/" rc.local.template > /etc/rc.local
sudo chmod +x /etc/rc.local
chmod +x install_docker.sh
mkdir -p ~/ping_logs
mkdir -p ~/http2_logs
mkdir -p ~/http11_logs
./install_docker.sh

echo 'Pulling H2o'
sudo docker pull vign2208/h2o

echo 'Pulling HTTP2'
sudo docker pull vign2208/http2_client

echo 'Pulling HTTP11'
sudo docker pull vign2208/h11p11_client
