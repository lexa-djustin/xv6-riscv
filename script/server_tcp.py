import socket
from time import sleep

def start_server(host='193.168.1.2', port=12345):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 256)
    server_socket.bind((host, port))
    server_socket.listen(5)

    socket_rv_size = server_socket.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print(f"Server socket buffer size {socket_rv_size}")

    print(f"Sever listens on {host}:{port}")

    while True:
        client_socket, client_address = server_socket.accept()

        # client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 2048)

        socket_rv_size = client_socket.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
        print(f"Client socket buffer size {socket_rv_size}")

        print(f"New message from {client_address}")

        message_counter = 1

        while True:
            # sleep(10)

            data = client_socket.recv(1024)

            if not data:
                print(f"Client {client_address} closed the connection.")
                break

            if data:
                print(data.decode('utf-8'))
                # print(f"Received: {data.decode('utf-8')}")
            #
            #     response = f"Server received a message #{message_counter}!".encode('utf-8')
            #     client_socket.sendall(response)
            message_counter += 1

        print(f"Connection with {client_address} closed.\n")
        client_socket.close()

if __name__ == "__main__":
    start_server()
