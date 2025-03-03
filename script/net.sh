set -e

# Удаляем интерфейсы, если уже существуют
ip link del tap7 2>/dev/null || true
ip link del tap8 2>/dev/null || true
ip link del br0  2>/dev/null || true

# 1. Создаём TAP-интерфейсы
ip tuntap add dev tap7 mode tap
ip tuntap add dev tap8 mode tap

# 2. Создаём bridge
ip link add name br0 type bridge
ip addr add 192.168.100.1/24 dev br0
ip link set dev br0 address aa:bb:cc:dd:ee:ff

# 3. Добавляем интерфейсы в мост
ip link set tap7 master br0
ip link set tap8 master br0

# 4. Включаем интерфейсы и мост
ip link set tap7 up
ip link set tap8 up
ip link set br0 up
