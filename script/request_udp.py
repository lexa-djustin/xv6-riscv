import socket
from time import sleep

for n in range(10):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    message = f'Hello, UDP {n}!'
    sock.sendto(message.encode(), ('193.168.1.1', 8080))
    sock.close()
