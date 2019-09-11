#!/bin/bash
curl -sSL https://get.docker.com | sh
sudo apt-get update
sudo apt-get install -y apt-transport-https \
                       ca-certificates \
                       software-properties-common
curl -fsSL https://yum.dockerproject.org/gpg | sudo apt-key add -
apt-key fingerprint 58118E89F3A912897C070ADBF76221572C52609D
sudo echo "deb https://download.docker.com/linux/raspbian/ stretch stable" >> /etc/apt/sources.list
sudo apt-get update
sudo systemctl start docker.service

