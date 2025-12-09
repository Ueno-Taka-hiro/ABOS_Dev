#!/bin/bash
################################################################################
# SetHostNetwork.sh - Host Network Configuration Script
################################################################################
# ホストPC側のブリッジネットワーク設定 (iproute2使用)
#   br100: tap00, tap10 (192.168.100.x網)
#   br200: tap01, tap11 (192.168.200.x網)
#
# Usage: ./SetHostNetwork.sh {up|down|ls}
################################################################################

BR_100="br100"
BR_200="br200"
TAP_100="tap00 tap10"
TAP_200="tap01 tap11"

create_network() {
    echo "Creating bridges: $BR_100, $BR_200"
    
    # ブリッジ作成
    sudo ip link add name $BR_100 type bridge
    sudo ip link set dev $BR_100 up
    sudo ip link add name $BR_200 type bridge
    sudo ip link set dev $BR_200 up
    
    # TAPインターフェースをブリッジに追加
    for tap in $TAP_100; do
        sudo ip link set dev $tap up
        sudo ip link set dev $tap master $BR_100
    done
    
    for tap in $TAP_200; do
        sudo ip link set dev $tap up
        sudo ip link set dev $tap master $BR_200
    done
    
    echo "Network setup complete"
}

cleanup_network() {
    echo "Cleaning up bridges: $BR_100, $BR_200"
    
    # TAPインターフェースをブリッジから削除
    for tap in $TAP_100 $TAP_200; do
        sudo ip link set dev $tap nomaster 2>/dev/null
        sudo ip link set dev $tap down 2>/dev/null
    done
    
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
            echo "Bridge: $br"
            ip link show master $br 2>/dev/null | grep -E "^[0-9]+:" | sed 's/^/  /'
        fi
    done
    echo
    echo "=== Interface Status ==="
    for dev in $BR_100 $BR_200 $TAP_100 $TAP_200; do
        ip addr show $dev 2>/dev/null | grep -E "^[0-9]+:|inet "
    done
}

# メイン処理
case "${1:-}" in
    up)
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
