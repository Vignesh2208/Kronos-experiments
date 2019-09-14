#!/bin/bash

sudo cp -v wpa_supplicant.conf /etc/wpa_supplicant/wpa_supplicant.conf
sudo cp -v interfaces /etc/network/interfaces/interfaces

sudo reboot
