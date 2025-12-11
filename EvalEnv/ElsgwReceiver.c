#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* ============================================================================
[<35;55;3M * ネットワーク設定 (ABOS1上で、ELSGWからのマルチキャストUDP通信を待ち受け)
 * ============================================================================ */
#define ABOS1_IP            "192.168.100.1"     /* ABOS1のeth0 IP */
#define MULTICAST_GROUP     "239.64.0.3"        /* ELSGWのマルチキャストグループ */
#define LISTEN_PORT         52000               /* ELSGWの連携ポート */

/* ============================================================================
 * 動作設定
 * ============================================================================ */
#define BUFFER_SIZE         1024    /* バッファサイズ */
#define TRUE                1

/* ============================================================================
 * 関数: print_hex_dump
 * 機能: バイナリデータを16進数でダンプ表示
 * ============================================================================ */
void print_hex_dump(const unsigned char *data, size_t len) {
    size_t i;
    printf("[HEX] ");
    for (i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) {
            printf("\n[HEX] ");
        }
    }
    if (len % 16 != 0) {
        printf("\n");
    }
}

/* ============================================================================
 * 関数: main
 * 機能: マルチキャストUDP受信サーバーを起動
 * ============================================================================ */
int main(int argc, char *argv[]) {
    int sock_fd = -1;
    struct sockaddr_in local_addr, sender_addr;
    struct ip_mreq mreq;
    socklen_t sender_addr_len;
    unsigned char buffer[BUFFER_SIZE];
    ssize_t recv_len;
    char sender_ip[INET_ADDRSTRLEN];
    int opt = 1;
    int packet_count = 0;

    printf("============================================================\n");
    printf("  ELSGW Receiver (Multicast UDP Mode) - ABOS1\n");
    printf("  Multicast Group: %s:%d\n", MULTICAST_GROUP, LISTEN_PORT);
    printf("  Local Interface: %s\n", ABOS1_IP);
    printf("============================================================\n");
    
    /* UDP ソケット作成 */
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("[ERROR] Failed to create UDP socket");
        return 1;
    }
    
    /* ポート再利用設定（複数プロセスが同じポートで受信可能にする） */
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, 
                   &opt, sizeof(opt)) < 0) {
        perror("[ERROR] Failed to set SO_REUSEADDR");
        close(sock_fd);
        return 1;
    }
    
    /* ローカルアドレス設定（全インターフェースで受信） */
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(LISTEN_PORT);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);  /* 0.0.0.0 で受信 */
    
    /* バインド */
    if (bind(sock_fd, (struct sockaddr *)&local_addr, 
             sizeof(local_addr)) < 0) {
        perror("[ERROR] Failed to bind UDP socket");
        close(sock_fd);
        return 1;
    }
    
    printf("[INFO] UDP socket bound to 0.0.0.0:%d\n", LISTEN_PORT);
    
    /* マルチキャストグループに参加 */
    memset(&mreq, 0, sizeof(mreq));
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);  /* グループアドレス */
    mreq.imr_interface.s_addr = inet_addr(ABOS1_IP);         /* 受信インターフェース */
    
    if (setsockopt(sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                   &mreq, sizeof(mreq)) < 0) {
        perror("[ERROR] Failed to join multicast group");
        close(sock_fd);
        return 1;
    }
    
    printf("[INFO] Joined multicast group: %s\n", MULTICAST_GROUP);
    printf("[INFO] Using interface: %s\n", ABOS1_IP);
    printf("\n[INFO] Ready to receive ELSGW API packets\n");
    printf("============================================================\n\n");
    
    /* パケット受信ループ */
    while (TRUE) {
        sender_addr_len = sizeof(sender_addr);
        
        /* マルチキャストパケット受信（ブロッキング） */
        recv_len = recvfrom(sock_fd, buffer, sizeof(buffer), 0,
                            (struct sockaddr *)&sender_addr, 
                            &sender_addr_len);
        
        if (recv_len < 0) {
            if (errno != EINTR) {
                perror("[ERROR] recvfrom failed");
            }
            continue;
        }
        
        /* 送信元 IP 取得 */
        inet_ntop(AF_INET, &sender_addr.sin_addr, sender_ip, INET_ADDRSTRLEN);
        
        /* パケットカウント */
        packet_count++;
        
        /* 受信データ表示 */
        printf("\n[RECV] ======================================== [#%d]\n", 
               packet_count);
        printf("[RECV] From: %s:%d\n", sender_ip, ntohs(sender_addr.sin_port));
        printf("[RECV] Size: %zd bytes\n", recv_len);
        
        /* 16進数ダンプ */
        print_hex_dump(buffer, recv_len);
        
        /* ASCII 表示（表示可能文字のみ） */
        printf("[ASCII] ");
        for (ssize_t i = 0; i < recv_len; i++) {
            if (buffer[i] >= 32 && buffer[i] <= 126) {
                printf("%c", buffer[i]);
            } else {
                printf(".");
            }
        }
        printf("\n");
        
        printf("[RECV] ========================================\n\n");
        
        /* ★ ここに受信パケットの解析・処理を追加 ★ */
        /* 例: プロトコル解析、コマンド実行、ログ記録など */
    }
    
    /* クリーンアップ（到達しない） */
    if (setsockopt(sock_fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, 
                   &mreq, sizeof(mreq)) < 0) {
        perror("[WARN] Failed to leave multicast group");
    }
    close(sock_fd);
    return 0;
}
