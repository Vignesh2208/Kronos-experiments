#!/bin/bash
arp -f /tmp/arp.txt
sudo tracer -r 1.000000 -n 10000 -f "/tmp/lxc-213-cmds.txt"
