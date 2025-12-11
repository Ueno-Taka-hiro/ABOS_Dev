#!/bin/bash
################################################################################
# setup_host_bridge_for_elsgw.sh - Host Network Configuration Script
################################################################################
# 目的: QEMUゲスト(ABOS1)との通信のため、Host PC側にブリッジネットワークを設定。
#
# 使用するTAPデバイス: tap00 (QEMU eth0接続), tap01 (QEMU eth1接続)
#
# Usage: ./setup_host_bridge_for_elsgw.sh {up|down|ls}
################################################################################

# ブリッジとTAPデバイスの定義 (QEMUスクリプトと一致させる)
BR_100="br100"
BR_200="br200"
# tap00 は ABOS1のeth0 (192.168.100.x) に接続
TAP_100="tap00"
# tap01 は ABOS1のeth1 (192.168.200.x) に接続
TAP_200="tap01" 

# Clientシミュレータ（Host）が使用するIPアドレスを定義
HOST_IP_100="192.168.100.100"
HOST_IP_200="192.168.200.100"

create_network() {
    echo "Creating bridges: $BR_100, $BR_200"
    
    # 既存のブリッジを念のためダウン/削除
    cleanup_network_lite

    # ブリッジ作成
    sudo ip link add name $BR_100 type bridge
    sudo ip link set dev $BR_100 up
    sudo ip link add name $BR_200 type bridge
    sudo ip link set dev $BR_200 up
    
    # TAPインターフェースをブリッジに追加
    # ※QEMUが起動する前に、これらのTAPデバイスはQEMUによって作成されている必要があります。
    
    echo "Adding TAP devices to bridges..."
    
    # tap00 を br100 に追加
    for tap in $TAP_100; do
        if ip link show $tap &>/dev/null; then
            sudo ip link set dev $tap up
            sudo ip link set dev $tap master $BR_100
        else
            echo "WARN: TAP device $tap does not exist. (Is QEMU running?)"
        fi
    done
    
    # tap01 を br200 に追加
    for tap in $TAP_200; do
        if ip link show $tap &>/dev/null; then
            sudo ip link set dev $tap up
            sudo ip link set dev $tap master $BR_200
        else
            echo "WARN: TAP device $tap does not exist. (Is QEMU running?)"
        fi
    done
    
    # --- ブリッジにIPアドレスを設定 (Host OSのIP) ---
    echo "Assigning Host IP addresses..."
    # br100 に 192.168.100.100 を設定 (ABOS1のeth0と通信)
    sudo ip address add $HOST_IP_100/24 dev $BR_100
    # br200 に 192.168.200.100 を設定 (ABOS1のeth1と通信)
    sudo ip address add $HOST_IP_200/24 dev $BR_200
    
    echo "Network setup complete. Host IPs: $HOST_IP_100, $HOST_IP_200"
}

# 削除処理を軽量化 (TAPデバイスの削除はQEMUのdownscriptまたは手動に依存)
cleanup_network_lite() {
    # ブリッジからIPアドレスを削除
    sudo ip address flush dev $BR_100 2>/dev/null
    sudo ip address flush dev $BR_200 2>/dev/null

    # TAPインターフェースをブリッジから削除
    for tap in $TAP_100 $TAP_200; do
        sudo ip link set dev $tap nomaster 2>/dev/null
        # TAPデバイス自体は削除しない (QEMUが使用するため)
    done
}

cleanup_network() {
    echo "Cleaning up bridges: $BR_100, $BR_200"
    
    cleanup_network_lite
    
    # ブリッジ削除
    sudo ip link set dev $BR_100 down 2>/dev/null
    sudo ip link delete $BR_100 type bridge 2>/dev/null
    sudo ip link set dev $BR_200 down 2>/dev/null
    sudo ip link delete $BR_200 type bridge 2>/dev/null
    
    echo "Network cleanup complete"
}

show_status() {
    echo "=== Bridge Status ==="
    for br in $BR_100 $BR_200; do
        if ip link show $br &>/dev/null; then
            echo "Bridge: $br (IP: $(ip addr show dev $br 2>/dev/null | grep 'inet ' | awk '{print $2}'))"
            ip link show master $br 2>/dev/null | grep -E "^[0-9]+:" | sed 's/^/  /'
        fi
    done
    echo
    echo "=== TAP Interface Status ==="
    for dev in $TAP_100 $TAP_200; do
        ip addr show $dev 2>/dev/null | grep -E "^[0-9]+:|inet "
    done
}

# メイン処理
case "${1:-}" in
    up)
        # QEMU起動前にTAPデバイスが作成されていることを確認してください
        create_network
        ;;
    down)
        cleanup_network
        ;;
    ls)
        show_status
        ;;
    *)
        echo "Usage: $0 {up|down|ls}"
        exit 1
        ;;
esac
