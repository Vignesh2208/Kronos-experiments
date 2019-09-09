#!/bin/bash
arp -f /tmp/arp.txt
sudo tracer -r 1.000000 -n 10000 -f "/tmp/lxc-328-cmds.txt"
