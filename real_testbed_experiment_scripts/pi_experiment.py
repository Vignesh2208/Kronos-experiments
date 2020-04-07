import paramiko
import os
from collections import defaultdict
import select


host_ip = {
  "dot08": ["192.168.1.8", "10.194.157.45", "b8:27:eb:e6:d6:16"],
  "dot09": ["192.168.1.9", "10.193.28.9", "b8:27:eb:97:02:96"],
  "dot10": ["192.168.1.10", "10.194.33.123", "b8:27:eb:76:53:2b"],
  "dot11": ["192.168.1.11", "10.192.95.182", "b8:27:eb:cd:ff:91"],
  "dot12": ["192.168.1.12", "10.193.57.162", "b8:27:eb:b2:89:47"],
  "dot15": ["192.168.1.15", "10.195.12.184", "b8:27:eb:33:3d:57"],
  "dot20": ["192.168.1.20", "10.193.176.211", "b8:27:eb:e0:1a:84"],
  "dot29": ["192.168.1.29", "10.194.186.163", "b8:27:eb:c5:c8:bd"],
  "dot50": ["192.168.1.50", "10.193.106.111", "b8:27:eb:a1:b2:aa"],
  "dot60": ["192.168.1.60", "10.192.65.140", "b8:27:eb:61:d9:b3"],
  "dot70": ["192.168.1.70", "10.193.252.65", "b8:27:eb:5b:cf:ee"],
  "dot80": ["192.168.1.80", "10.192.209.202", "b8:27:eb:5e:6f:f0"]
}

servers = ["dot11", "dot29", "dot80"]

cli_server_mapping = {
  "dot08": "dot11",
  "dot09": "dot29",
  "dot10": "dot80",
  "dot12": "dot29",
  "dot15": "dot11",
  "dot20": "dot80",
  "dot50": "dot80",
  "dot60": "dot11",
  "dot70": "dot29"
}


num_trials = 1000
period = 100

ping_cmd = 'ping -c 100 -i 0.2 {dest_ip} > /home/pi/ping_logs/ping_log.txt'
http2_cmd = '/home/pi/Desktop/Ns3-Experiments/run_http2_client.sh {dest_ip} {num_trials} {period}'
http11_cmd = '/home/pi/Desktop/Ns3-Experiments/run_http11_client.sh {dest_ip} {num_trials} {period} 10'


def get_files_from(client):
    ssh_pass_cmd = 'sshpass -p "raspberry" scp -r pi@{target_ip}:/home/pi/{remote_path} /home/vignesh/Ns3-Experiments/exp_logs/{local_path}'
    local_path=client + '/http2_logs'
    remote_path='http2_logs'
    os.system(ssh_pass_cmd.format(target_ip=host_ip[client][1], remote_path=remote_path, local_path=local_path))

    local_path=client + '/http11_logs'
    remote_path='http11_logs'
    os.system(ssh_pass_cmd.format(target_ip=host_ip[client][1], remote_path=remote_path, local_path=local_path))

    local_path=client + '/ping_logs'
    remote_path='ping_logs'
    os.system(ssh_pass_cmd.format(target_ip=host_ip[client][1], remote_path=remote_path, local_path=local_path))

def run_cmd_via_paramiko(IP, command, port=22, username='pi', password='raspberry'):

    s = paramiko.SSHClient()
    s.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    s.load_system_host_keys()

    s.connect(IP, port, username, password)
    transport = s.get_transport()
    channel = transport.open_session()
    channel.exec_command(command)
    return s, channel


def run_http_cli_servers(http_type=2):
    clients = []
    channels = []
    for client in cli_server_mapping:
        
        target_ip = host_ip[cli_server_mapping[client]][0]
        if http_type == 2:
            cmd = http2_cmd.format(dest_ip=target_ip, num_trials=num_trials, period=period)
        else:
            cmd = http11_cmd.format(dest_ip=target_ip, num_trials=num_trials, period=period)
        print (host_ip[client][0], ' Cmd = ', cmd)
        client, channel = run_cmd_via_paramiko(host_ip[client][1], cmd)
        clients.append(client)
        channels.append(channel)

    print ('Initiated all http commands. Waiting for them to complete')
    for i in range(0, len(channels)):
        print ('Checking output: ', i + 1)
        while True:
            if channels[i].exit_status_ready():
                clients[i].close()
                break
            rl, wl, xl = select.select([channels[i]], [], [], 0.0)

def run_pings_cli_servers():
    clients = []
    channels = []
    for client in cli_server_mapping:
        
        target_ip = host_ip[cli_server_mapping[client]][0]
        print (host_ip[client][0], ' Cmd = ', ping_cmd.format(dest_ip=target_ip))
        client, channel = run_cmd_via_paramiko(host_ip[client][1], ping_cmd.format(dest_ip=target_ip))
        clients.append(client)
        channels.append(channel)

    print ('Initiated all commands. Waiting for them to complete')
    for i in range(0, len(channels)):
        print ('Checking output: ', i + 1)
        while True:
            if channels[i].exit_status_ready():
                clients[i].close()
                break
            rl, wl, xl = select.select([channels[i]], [], [], 0.0)
            #if len(rl) > 0:
            #    print (channel.recv(1024))

def run_arbitrary_cmd(cmd):
    clients = []
    channels = []
    for client in host_ip:
        print ('Running arbitrary cmd in: ', host_ip[client][0])
        client, channel = run_cmd_via_paramiko(host_ip[client][1], cmd)
        clients.append(client)
        channels.append(channel)

    print ('Initiated all arbitrary commands. Waiting for them to complete')
    for i in range(0, len(channels)):
        print ('Checking output: ', i + 1)
        while True:
            if channels[i].exit_status_ready():
                clients[i].close()
                break
            rl, wl, xl = select.select([channels[i]], [], [], 0.0)
            if len(rl) > 0:
                print (channel.recv(1024))

if __name__ == "__main__":
    #run_arbitrary_cmd('mkdir -p /home/pi/http2_logs')
    #run_arbitrary_cmd('mkdir -p /home/pi/http11_logs')
    #run_arbitrary_cmd('mkdir -p /home/pi/ping_logs')
    #run_pings_cli_servers()
    #run_http_cli_servers(http_type=2)
    #run_http_cli_servers(http_type=1)
    for client in cli_server_mapping:
        get_files_from(client)
