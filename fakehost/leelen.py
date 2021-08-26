#!/usr/bin/env python3

import socket
import threading


def advertise(host : str='127.0.0.2', port : int=6789):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', port))

    advertisement = host + '?1*A20L-GLJ10-18-2-3772-155-V1.51_20160930'
    advertisement = advertisement.encode()

    while True:
        data, addr = sock.recvfrom(1024)
        print('advertise: receive', data, 'from', addr)
        if b'?' in data:
            continue
        sock.sendto(advertisement, addr)


def ack(host : str='127.0.0.2', port : int=6050):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind((host, port))

    while True:
        data, addr = sock.recvfrom(1024)
        print('ack: receive', data, 'from', addr)
        data = b'\x10\x01' + data[2:]
        sock.sendto(data, addr)


a = threading.Thread(target=advertise, args=())
a.start()
b = threading.Thread(target=ack, args=())
b.start()
