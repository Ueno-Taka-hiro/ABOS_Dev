#!/bin/bash
################################################################################
# SetBridgeNetwork.sh - ABOS1 Network Configuration Script
################################################################################
# ABOS1 (Bridge) のネットワーク設定
#   eth0: 192.168.100.1/24 (ABOS2からの接続受信)
#   eth1: 192.168.200.1/24 (ABOS2への応答送信)
#   eth2: 10.0.2.x/24     (QEMU User Mode自動設定)
################################################################################

# NICのクリーンアップ
for dev in eth0 eth1 eth2; do
    ip link set dev $dev down 2>/dev/null
    ip addr flush dev $dev 2>/dev/null
done
ip route flush table main 2>/dev/null

# eth0設定 (100.x網)
ip link set dev eth0 up
sleep 1
ip address add 192.168.100.1/24 dev eth0
ip route add 192.168.100.0/24 dev eth0 2>/dev/null

# eth1設定 (200.x網)
ip link set dev eth1 up
sleep 1
ip address add 192.168.200.1/24 dev eth1
ip route add 192.168.200.0/24 dev eth1 2>/dev/null

# eth2設定 (外部通信)
ip link set dev eth2 up

sleep 3
echo "ABOS1: eth0=192.168.100.1/24, eth1=192.168.200.1/24, eth2=auto"
