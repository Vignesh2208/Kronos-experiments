import sys
import argparse
import os

parser = argparse.ArgumentParser()
parser.add_argument('--nLXCs', metavar='nLXCs', type=int, default=12,
                    help='Number of LXCs')
args = parser.parse_args()

PING_CMD = "ping -c 10 {ip}"

lxc_ips = []

os.system('rm *.txt')

for i in xrange(0, args.nLXCs):
    lxc_ips.append('10.1.1.{node_number}'.format(node_number=i + 1))
    

print 'LXC IPs = ', lxc_ips
for i in xrange(0, args.nLXCs):
    with open('lxc-%d-cmds.txt' %(i+1), 'w') as f:
       for j in xrange(0, args.nLXCs) :
           if i != j:
               f.write(PING_CMD.format(ip=lxc_ips[j]) + '\n')
