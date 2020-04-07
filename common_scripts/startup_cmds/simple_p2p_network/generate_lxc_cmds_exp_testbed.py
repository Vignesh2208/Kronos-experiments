import sys
import argparse
import os



nProcessesperLXC = 1
nServerLXCs = 3
nSwitches = 3
nLXCsperSwitch = 4
nClients  = nLXCsperSwitch*nSwitches*nProcessesperLXC - nServerLXCs*nProcessesperLXC

server_ips = []
os.system('rm *.txt')

numTrials = 100
period = 100

PING_CMD = 'ping -c 100 -i 0.2 -D {server_ip}'


HTTP_SERVER_CMD = 'sudo h2o -c /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/NSDI-2020/h2o.conf'
HTTP2_CLIENT_CMD = '/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/client_http2 {server_ip} {numTrials} {period}'
HTTP11_CLIENT_CMD = '/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/client_http11_timeout {server_ip} {numTrials} {period} 10'

cmd_type = sys.argv[1]



def get_lxc_ip(lxc_no):
    network_num = int(lxc_no / 256) + 1
    node_num = lxc_no % 256
    return '10.1.{network_num}.{node_num}'.format(network_num=network_num,node_num=node_num)
    


nServerLXCsperSwitch = nServerLXCs/nSwitches
nClientLXCsperSwitch = nLXCsperSwitch - nServerLXCsperSwitch
for i in xrange(0, nSwitches):
    for j in xrange(1, nServerLXCsperSwitch + 1):
        server_ips.append(get_lxc_ip(i*nLXCsperSwitch + j))
        with open('lxc-%d-cmds.txt' %(i*nLXCsperSwitch + j), 'w') as f:
            for k in xrange(0, nProcessesperLXC):
                f.write(HTTP_SERVER_CMD)

print 'Server IPs = ', server_ips
for i in xrange(1, nSwitches + 1):
    k = 1
    for j in xrange(1, nClientLXCsperSwitch + 1):
        server_lxc_no = ((k-1)*nLXCsperSwitch + 1)
        k = k + 1
        with open('lxc-%d-cmds.txt' %((i - 1)*nLXCsperSwitch + j + nServerLXCsperSwitch), 'w') as f:
            for m in xrange(0, nProcessesperLXC):
                if cmd_type == 'ping_cmd':
                    f.write(PING_CMD.format(server_ip=get_lxc_ip(server_lxc_no)))
                elif cmd_type == 'http2':
                    f.write(HTTP2_CLIENT_CMD.format(server_ip=get_lxc_ip(server_lxc_no), numTrials=numTrials, period=period))
                elif cmd_type == 'http11':
                    f.write(HTTP11_CLIENT_CMD.format(server_ip=get_lxc_ip(server_lxc_no), numTrials=numTrials, period=period))
