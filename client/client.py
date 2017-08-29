#!/usr/bin/env python2

import sys
import socket
from time import sleep

def main(args):
    kSubscribe = 1
    kUnsubscribe = 2
    kPing = 3

    sock = None
    try:
        proto = args[1].lower()
        ip = args[2].lower()
        port = int(args[3])

        if proto == "udp":
            proto = socket.SOCK_DGRAM
        elif proto == "tcp":
            proto = socket.SOCK_STREAM
        else:
            print("Unknown protocol:", proto)
            exit(1)

        sock = socket.socket(socket.AF_INET, proto)
        sock.connect((ip, port))
        
        print("Connected to", ip, port)
        count = 0
        while True:
            try:
#                data = sock.recv(1024)
#                if not data:
#                    break
                if count == 0:
                    sock.send(bytes(bytearray([kSubscribe])))
                else:
                    sock.send(bytes(bytearray([kPing])))
                count += 1
                sleep(1)
            except socket.error as e:
                print (e)
                break

    except KeyboardInterrupt:
        print("Cancel...")
    finally:
        if sock:
            sock.send(bytes(bytearray([kUnsubscribe])))
            sock.close()
            print("Disconnected")

if __name__ == "__main__":
    main(sys.argv)
