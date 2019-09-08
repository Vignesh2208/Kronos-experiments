import sys
import argparse
import os



nProcessesperLXC = 10
nServerLXCs = 10
nSwitches = 10
nLXCsperSwitch = 10
nClients  = nLXCsperSwitch*nSwitches*nProcessesperLXC - nServerLXCs*nProcessesperLXC

server_ips = []
os.system('rm *.txt')

numTrials = 100
period = 100

CLI_CMD = '/home/kronos/Downloads/grpc/examples/cpp/helloworld/greeter_client {server_ip} {port} {numTrials} {period}\n'
SERVER_CMD = '/home/kronos/Downloads/grpc/examples/cpp/helloworld/greeter_server {port}\n'


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
            start_port_no = 50051
            for k in xrange(0, nProcessesperLXC):
                f.write(SERVER_CMD.format(port=str(start_port_no + k)))

print 'Server IPs = ', server_ips
for i in xrange(1, nSwitches + 1):
    k = 1
    for j in xrange(1, nClientLXCsperSwitch + 1):
        if k == i:
            k = k + 1
        server_lxc_no = ((k-1)*nLXCsperSwitch + 1)
        k = k + 1
        with open('lxc-%d-cmds.txt' %((i - 1)*nLXCsperSwitch + j + nServerLXCsperSwitch), 'w') as f:
            start_port_no = 50051
            for m in xrange(0, nProcessesperLXC):
                f.write(CLI_CMD.format(server_ip=get_lxc_ip(server_lxc_no), port=str(start_port_no + m), numTrials=numTrials, period=period)) 
