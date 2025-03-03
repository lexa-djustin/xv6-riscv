#!/usr/bin/env python3
import os
import fcntl
import struct

TUNSETIFF = 0x400454ca
IFF_TAP   = 0x0002
IFF_NO_PI = 0x1000
TAP_NAME = "tap8"

def open_tap(name):
    fd = os.open("/dev/net/tun", os.O_RDWR)
    ifr = struct.pack("16sH", name.encode(), IFF_TAP | IFF_NO_PI)
    fcntl.ioctl(fd, TUNSETIFF, ifr)
    return fd

def format_mac(raw_mac):
    return ":".join(f"{b:02x}" for b in raw_mac)

def main():
    fd = open_tap(TAP_NAME)
    print(f"[{TAP_NAME}] Listening for Ethernet frames...")

    while True:
        frame = os.read(fd, 2048)
        dst_mac = format_mac(frame[0:6])
        src_mac = format_mac(frame[6:12])
        eth_type = frame[12:14].hex()
        payload = frame[14:].rstrip(b"\x00")
        payload_str = payload.decode('utf-8', errors='replace')
        print(f"[{TAP_NAME}] Src: {src_mac} → Dst: {dst_mac}, Type: 0x{eth_type}, Payload: '{payload_str}'")

if __name__ == "__main__":
    main()
