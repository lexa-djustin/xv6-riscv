import socket

IP = "193.168.1.2"
PORT = 8080
BUFFER_SIZE = 1024

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((IP, PORT))
print(f"[+] UDP server listening on {IP}:{PORT}")

while True:
    data, addr = sock.recvfrom(BUFFER_SIZE)
    print(f"[>] Received from {addr}: {data.decode(errors='replace')}")
