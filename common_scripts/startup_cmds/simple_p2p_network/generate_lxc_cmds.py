import sys
import argparse
import os

parser = argparse.ArgumentParser()
parser.add_argument('--nServers', metavar='nServers', type=int, default=250,
                    help='Number of Servers')
parser.add_argument('--nLXCsperSwitch', dest='nLXCsperSwitch', type=int, default=100,
                    help='Total number of LXCs per LAN')
parser.add_argument('--nSwitches', dest='nSwitches', type=int, default=5,
                    help='Total number of LANs')
parser.add_argument('--isHTTP2', dest='isHTTP2', type=int, default=1,
                    help='is Client Type HTTP2')

args = parser.parse_args()

#assert args.nServers <= args.nLXCsperSwitch
nClients  = args.nLXCsperSwitch*args.nSwitches - args.nServers

server_ips = []

os.system('rm *.txt')

numTrials = 100
period = 100
numThreads = 10

#HTTP_SERVER_CMD = 'sudo h2o -c /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/NSDI-2020/h2o.conf'
#HTTP2_CLIENT_CMD = '/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/client_http2 {server_ip} {numTrials} {period}'
#HTTP11_CLIENT_CMD = '/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/client_http11_timeout {server_ip} {numTrials} {period} {numThreads}'
HTTP_SERVER_CMD = 'python /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/tcp_server.py'
HTTP2_CLIENT_CMD = 'python /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/tcp_client.py {server_ip}'
HTTP11_CLIENT_CMD = 'python /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/tcp_client.py {server_ip}'

def get_lxc_ip(lxc_no):
    network_num = int(lxc_no / 256) + 1
    node_num = lxc_no % 256
    return '10.1.{network_num}.{node_num}'.format(network_num=network_num,node_num=node_num)
    


nServersperSwitch = args.nServers/args.nSwitches
nClientsperSwitch = args.nLXCsperSwitch - nServersperSwitch
for i in xrange(0, args.nSwitches):
    for j in xrange(1, nServersperSwitch + 1):
        server_ips.append(get_lxc_ip(i*args.nLXCsperSwitch + j))
        with open('lxc-%d-cmds.txt' %(i*args.nLXCsperSwitch + j), 'w') as f:
            f.write(HTTP_SERVER_CMD)

print 'Server IPs = ', server_ips
k = 0
for i in xrange(0, args.nSwitches):
    k = 0
    num_processed = i*(nServersperSwitch/args.nSwitches)
    count = 0 
    for j in xrange(1, nClientsperSwitch + 1):
        
        num_processed += 1
        count += 1
        server_lxc_no = (k*args.nLXCsperSwitch + num_processed)
        if count == nServersperSwitch/args.nSwitches :
            num_processed = i*nServersperSwitch/args.nSwitches
            k += 1
            count = 0

        with open('lxc-%d-cmds.txt' %(i*args.nLXCsperSwitch + j + nServersperSwitch), 'w') as f:
            if args.isHTTP2 == 1:
                #f.write(HTTP2_CLIENT_CMD.format(server_ip=get_lxc_ip(server_lxc_no), numTrials=str(numTrials), period=str(period) ))
                f.write(HTTP2_CLIENT_CMD.format(server_ip=get_lxc_ip(server_lxc_no)))
            else:
                #f.write(HTTP11_CLIENT_CMD.format(server_ip=get_lxc_ip(server_lxc_no), numTrials=str(numTrials), period=str(period), numThreads=str(numThreads) ))
                f.write(HTTP11_CLIENT_CMD.format(server_ip=get_lxc_ip(server_lxc_no)))
