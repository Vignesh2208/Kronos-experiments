import socket
import sys
import time

serverIP = sys.argv[1]



# create an ipv4 (AF_INET) socket object using the tcp protocol (SOCK_STREAM)

print "Waiting for 1 secs ..."
time.sleep(1)
# Send to server using created UDP socket
# connect the client

while True:
	client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	print "Connecting ..."
	client.connect((serverIP, 9999))
	print "Connected ..."
	print "Client Sending Message ..."
        start_time = float(time.time())
	client.send('Hello\r\n')
	print "Waiting for response ..."
	response = client.recv(4096)

        finish_time = float(time.time())
	print "Got: ", response
	print "Time Elapsed: ", finish_time - start_time
	client.close()
	time.sleep(0.1)
	


