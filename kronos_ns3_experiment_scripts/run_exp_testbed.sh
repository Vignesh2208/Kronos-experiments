#!/bin/bash

set -e 

cd /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/simple_p2p_network && python generate_lxc_cmds_exp_testbed.py http11
cd /home/kronos/ns-allinone-3.29/ns-3.29 && sudo ./waf --run "examples/kronos_experiments/exp_testbed --enableKronos=true --runTimeSecs=10 --nSimHostsperSwitch=1 --logFolder=http11_logs_1"

