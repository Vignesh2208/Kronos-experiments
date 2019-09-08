#!/bin/bash
ifconfig
arp -f /tmp/arp.txt
arp -v
sudo tracer -r 1.000000 -n 10000 -f "/tmp/lxc-9-cmds.txt"
