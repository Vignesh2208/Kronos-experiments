import sys
import argparse
import os

parser = argparse.ArgumentParser()
parser.add_argument('--nLXCs', metavar='nLXCs', type=int, default=12,
                    help='Number of LXCs')
parser.add_argument('--target', metavar='target', type=int, default=2,
                    help='Target LXC')

args = parser.parse_args()

#PING_CMD = "ping -c 10 {ip}"
PING_CLI_CMD = 'python /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/campus_network/udp_ping_client.py {server_ip}'
PING_SERVER_CMD = 'python /home/kronos/ns-allinone-3.29/ns-3.29/examples/vt_experiments/common/startup_cmds/campus_network/udp_ping_server.py {client_ip}'

lxc_ips = []

os.system('rm *.txt')

for i in xrange(0, args.nLXCs):
    lxc_ips.append('10.1.1.{node_number}'.format(node_number=i + 1))
    

print 'LXC IPs = ', lxc_ips
target = args.target
with open('lxc-1-cmds.txt', 'w') as f:
    f.write(PING_SERVER_CMD.format(client_ip=lxc_ips[target-1]) + '\n')

with open('lxc-%d-cmds.txt' %(target), 'w') as f:
    f.write(PING_CLI_CMD.format(server_ip=lxc_ips[0]) + '\n')

#for i in xrange(0, args.nLXCs):
#    if i == 1:
#        with open('lxc-%d-cmds.txt' %(i+1), 'w') as f:
#            f.write(PING_CMD.format(ip=lxc_ips[0]) + '\n')
       #for j in xrange(0, args.nLXCs) :
       #    if i != j:
       #        f.write(PING_CMD.format(ip=lxc_ips[j]) + '\n')
