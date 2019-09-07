import sys
import argparse
import os

parser = argparse.ArgumentParser()
parser.add_argument('--nServers', metavar='nServers', type=int, default=1,
                    help='Number of Servers')
parser.add_argument('--nLXCsperSwitch', dest='nLXCsperSwitch', type=int, default=1,
                    help='Total number of LXCs per LAN')
parser.add_argument('--nSwitches', dest='nSwitches', type=int, default=2,
                    help='Total number of LANs')
parser.add_argument('--isHTTP2', dest='isHTTP2', type=int, default=1,
                    help='is Client Type HTTP2')

args = parser.parse_args()

#assert args.nServers <= args.nLXCsperSwitch
nClients  = args.nLXCsperSwitch*args.nSwitches - args.nServers

server_ips = []

os.system('rm *.txt')
os.system('rm /tmp/*.txt')
os.system('rm -rf /tmp/lxc-*')

numTrials = 100
period = 100
numThreads = 10

HTTP_SERVER_CMD = 'sudo h2o -c /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/NSDI-2020/h2o.conf'
HTTP2_CLIENT_CMD = '/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/client_http2 {server_ip} {numTrials} {period}'
HTTP11_CLIENT_CMD = '/home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/client_http11_timeout {server_ip} {numTrials} {period} {numThreads}'

for i in xrange(0, args.nServers):
    server_ips.append('10.1.1.{node_number}'.format(node_number=i + 1))
    with open('lxc-%d-cmds.txt' %(i+1), 'w') as f:
        f.write(HTTP_SERVER_CMD)

print 'Server IPs = ', server_ips
for i in xrange(0, nClients):
    server_ip_idx = i % args.nServers
    with open('lxc-%d-cmds.txt' %(args.nServers + i+1), 'w') as f:
        if args.isHTTP2 ==1:
            f.write(HTTP2_CLIENT_CMD.format(server_ip=server_ips[server_ip_idx], numTrials=str(numTrials), period=str(period) ))
        else:
            f.write(HTTP11_CLIENT_CMD.format(server_ip=server_ips[server_ip_idx], numTrials=str(numTrials), period=str(period), numThreads=str(numThreads) ))
