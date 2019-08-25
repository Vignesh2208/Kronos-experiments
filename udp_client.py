#!/usr/bin/python
import socket
import sys
import time

serverIP = sys.argv[1]
msgFromClient       = "Hello UDP Server"
bytesToSend         = str.encode(msgFromClient)
serverAddressPort   = (serverIP, 20001)
bufferSize          = 1024

# Create a UDP socket at client side
UDPClientSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
print "Waiting for 5 secs ..."
time.sleep(5)
while True:
	# Send to server using created UDP socket
	print "Client Sending Message ..."
	UDPClientSocket.sendto(bytesToSend, serverAddressPort)
	print "Waiting for response from server ..."
	msgFromServer = UDPClientSocket.recvfrom(bufferSize)
	msg = "Message from Server {}".format(msgFromServer[0])
	print msg
	time.sleep(1.0)
