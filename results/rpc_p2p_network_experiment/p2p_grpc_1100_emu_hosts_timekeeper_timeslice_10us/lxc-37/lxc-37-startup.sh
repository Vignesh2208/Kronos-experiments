#!/bin/bash
arp -f /tmp/arp.txt
/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/cmd_executor_tk -f /tmp/lxc-37-cmds.txt -l /tmp/lxc-37/lxc-37-pid.log
