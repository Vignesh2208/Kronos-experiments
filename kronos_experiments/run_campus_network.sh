#!/bin/bash

#echo 'Running 12 LXCs HTTP2'
#cd /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/campus_network && python generate_lxc_cmds.py --nServers=2 --nLXCsperSwitch=1 --nSwitches=12 --isHTTP2=1
#cd /home/kronos/ns-allinone-3.29/ns-3.29 && sudo ./waf --run "examples/kronos_experiments/campus_network --enableKronos=true --runTimeSecs=20 --nSimHostsperSwitch=200 --nLXCsperSwitch=1 --logFolder=campus_http2_1LXCs_200_hosts_per_switch_20secs"

set -e

#echo 'Running 60 LXCs HTTP2'
#cd /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/campus_network && python generate_lxc_cmds.py --nServers=2 --nLXCsperSwitch=5 --nSwitches=12 --isHTTP2=1
#cd /home/kronos/ns-allinone-3.29/ns-3.29 && sudo ./waf --run "examples/kronos_experiments/campus_network --enableKronos=true --runTimeSecs=20 --nSimHostsperSwitch=200 --nLXCsperSwitch=5 --logFolder=campus_http2_5LXCs_200_hosts_per_switch_20secs"

#echo 'Running 120 LXCs HTTP2'
#cd /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/campus_network && python generate_lxc_cmds.py --nServers=2 --nLXCsperSwitch=10 --nSwitches=12 --isHTTP2=1
#cd /home/kronos/ns-allinone-3.29/ns-3.29 && sudo ./waf --run "examples/kronos_experiments/campus_network --enableKronos=true --runTimeSecs=20 --nSimHostsperSwitch=200 --nLXCsperSwitch=10 --logFolder=campus_http2_10LXCs_200_hosts_per_switch_20secs"



#echo 'Running 12 LXCs HTTP11'
#cd /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/campus_network && python generate_lxc_cmds.py --nServers=2 --nLXCsperSwitch=1 --nSwitches=12 --isHTTP2=0
#cd /home/kronos/ns-allinone-3.29/ns-3.29 && sudo ./waf --run "examples/kronos_experiments/campus_network --enableKronos=true --runTimeSecs=5 --nSimHostsperSwitch=200 --nLXCsperSwitch=1 --logFolder=campus_http11_1LXCs_200_hosts_per_switch_20secs_noBackground"


echo 'Running 60 LXCs HTTP11'
cd /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/campus_network && python generate_lxc_cmds.py --nServers=2 --nLXCsperSwitch=5 --nSwitches=12 --isHTTP2=0
cd /home/kronos/ns-allinone-3.29/ns-3.29 && sudo ./waf --run "examples/kronos_experiments/campus_network --enableKronos=true --runTimeSecs=20 --nSimHostsperSwitch=200 --nLXCsperSwitch=5 --logFolder=campus_http11_5LXCs_200_hosts_per_switch_20secs"

#echo 'Running 120 LXCs HTTP11'
#cd /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/campus_network && python generate_lxc_cmds.py --nServers=2 --nLXCsperSwitch=10 --nSwitches=12 --isHTTP2=0
#cd /home/kronos/ns-allinone-3.29/ns-3.29 && sudo ./waf --run "examples/kronos_experiments/campus_network --enableKronos=true --runTimeSecs=20 --nSimHostsperSwitch=200 --nLXCsperSwitch=10 --logFolder=campus_http11_10LXCs_200_hosts_per_switch_20secs"
