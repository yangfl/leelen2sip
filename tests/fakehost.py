#!/usr/bin/env python3

import random
import socket
import struct
import threading


# code sid
MSG_HEAD = struct.Struct('<II')
sid = 0x12345678
theirs = '9999-9999'
ours = '9999-0009'

HOST = '127.0.0.2'
PORT = 6050


def advertise(host : str=HOST, port : int=6789):
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


sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind((HOST, PORT))
sock.connect(('127.0.0.1', PORT))


def ack(host : str=HOST, port : int=PORT):
    global sid

    while True:
        data, addr = sock.recvfrom(1024)
        print('ack: receive', data, 'from', addr)
        code, sid = MSG_HEAD.unpack_from(data)
        if code == 0x110:
            continue
        data = b'\x10\x01' + data[2:]
        sock.sendto(data, addr)


a = threading.Thread(target=advertise, args=())
a.start()
b = threading.Thread(target=ack, args=())
b.start()


while True:
    t = input('Action (s/b): ').lower()
    if t == 's':
        sid = random.randint(1, 0xffffffff)
        msg = MSG_HEAD.pack(1, sid) + f'''From={ours}?1
To={theirs}
Audio=PCMA/8000
AudioPort=7078
Video=JPEG/90000
VideoPort=9078'''.encode()
    elif t == 'b':
        msg = MSG_HEAD.pack(0x214, sid) + f'''From={ours}?1
To={theirs}'''.encode()
    else:
        print('Unknown input')
        continue
    print('sending:', msg)
    sock.send(msg)
