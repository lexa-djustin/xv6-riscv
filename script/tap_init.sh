#!/bin/bash

set -e  # Stop execution if any command fails

## Add IP address to tap2 interface
#echo "Adding IP address 193.168.1.1/24 to tap2 interface..."
#ip addr add 193.168.1.1/24 dev tap2
#
# Add IP address to tap2 interface
echo "Adding IP address 193.168.1.1/24 to tap2 interface..."
ip addr add 193.168.1.2/24 dev tap2

# Bring up tap2 interface
echo "Bringing up tap2 interface..."
ip link set tap2 up

## Remove local route for tap2
#echo "Removing local route 193.168.1.1/32 for tap2..."
#ip route del local 193.168.1.1/32 dev tap2
#
## Remove local route for tap2
#echo "Removing local route 193.168.1.2/32 for tap2..."
#ip route del local 193.168.1.2/32 dev tap2

## Add tap2 interface to bridge br0
#echo "Adding tap2 interface to bridge br0..."
#sudo ip link set tap2 master br0

echo "Tap interface is ready."
