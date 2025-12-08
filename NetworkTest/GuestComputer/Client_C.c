/*
 * ============================================================================
 * Client_C.c - ABOS2 TCP Client Program
 * ============================================================================
 * 機能:
 *   - ABOS1へメッセージを送信 (Client機能: 192.168.100.1:8000へ接続)
 *   - ABOS1からの応答を受信 (Server機能: 192.168.200.2:8000で待ち受け)
 *
 * 通信経路:
 *   [往路] ABOS2 (192.168.100.2) -> ABOS1 (192.168.100.1:8000)
 *   [復路] ABOS1 (192.168.200.1) -> ABOS2 (192.168.200.2:8000)
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* ============================================================================
 * ネットワーク設定
 * ============================================================================ */
/* 往路設定: ABOS1への送信 */
#define SERVER_IP_OUTBOUND      "192.168.100.1"
#define SERVER_PORT_OUTBOUND    8000
#define CLIENT_IP_OUTBOUND_SRC  "192.168.100.2"

/* 復路設定: ABOS1からの応答を待ち受け */
#define CLIENT_IP_INBOUND       "192.168.200.2"
#define CLIENT_PORT_INBOUND     8000

/* ============================================================================
 * 動作設定
 * ============================================================================ */
#define MESSAGE_INTERVAL        1       /* メッセージ送信サイクル間隔 (秒) */
#define RETRY_DELAY             1       /* 接続リトライ間隔 (秒) */
#define BUFFER_SIZE             1024    /* バッファサイズ */
#define MAX_PENDING             5       /* 待ち受けキューの最大数 */
#define TRUE                    1

/* ============================================================================
 * プログラム情報
 * ============================================================================ */
const char *CLIENT_HOST_NAME = "ABOS2";
const char *CLIENT_LANGUAGE  = "C";

/* ============================================================================
 * 関数: generate_message
 * 機能: ABOS1へ送信するメッセージを生成
 * 引数:
 *   buffer      - メッセージ格納先バッファ
 *   buffer_size - バッファサイズ
 * ============================================================================ */
void generate_message(char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "Hello from %s written by %s",
             CLIENT_HOST_NAME, CLIENT_LANGUAGE);
}

/* ============================================================================
 * 関数: main
 * 機能: ABOS1への定期的なメッセージ送信と応答受信
 * 処理フロー:
 *   1. ABOS1へ接続 (往路)
 *   2. メッセージを送信
 *   3. 接続をクローズ
 *   4. ABOS1からの応答を待ち受け (復路)
 *   5. 応答を受信
 *   6. 指定間隔で繰り返し
 * ============================================================================ */
int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr_out;
    struct sockaddr_in bind_addr_in;
    struct sockaddr_in client_bind_addr;
    struct sockaddr_in client_addr_in;
    char send_buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE];
    int opt = 1;

    printf("============================================================\n");
    printf("  Client/Server (ABOS2, C) Starting\n");
    printf("============================================================\n");

    /* 往路接続先の設定 (ABOS1) */
    memset(&serv_addr_out, 0, sizeof(serv_addr_out));
    serv_addr_out.sin_family = AF_INET;
    serv_addr_out.sin_port = htons(SERVER_PORT_OUTBOUND);
    
    if (inet_pton(AF_INET, SERVER_IP_OUTBOUND, &serv_addr_out.sin_addr) <= 0) {
        perror("[ERROR] Invalid outbound address");
        return 1;
    }

    /* 復路待ち受けアドレスの設定 (ABOS2) */
    memset(&bind_addr_in, 0, sizeof(bind_addr_in));
    bind_addr_in.sin_family = AF_INET;
    bind_addr_in.sin_port = htons(CLIENT_PORT_INBOUND);
    
    if (inet_pton(AF_INET, CLIENT_IP_INBOUND, &bind_addr_in.sin_addr) <= 0) {
        perror("[ERROR] Invalid inbound bind address");
        return 1;
    }

    /* メッセージ送受信ループ */
    while (TRUE) {
        int client_sock_fd = -1;
        int listen_sock = -1;
        int accept_sock = -1;
        socklen_t addrlen;

        /* ====================================================================
         * 往路処理: ABOS1へメッセージを送信
         * ==================================================================== */
        
        /* 往路接続のリトライループ */
        while (TRUE) {
            client_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (client_sock_fd < 0) {
                perror("[ERROR] Socket creation failed");
                return 1;
            }

            /* 送信元IPアドレスを明示的にバインド (192.168.100.2) */
            memset(&client_bind_addr, 0, sizeof(client_bind_addr));
            client_bind_addr.sin_family = AF_INET;
            client_bind_addr.sin_port = 0; /* OSが自動割り当て */
            inet_pton(AF_INET, CLIENT_IP_OUTBOUND_SRC, &client_bind_addr.sin_addr);

            if (bind(client_sock_fd, (struct sockaddr *)&client_bind_addr, 
                    sizeof(client_bind_addr)) < 0) {
                perror("[ERROR] Client bind failed");
                close(client_sock_fd);
                client_sock_fd = -1;
                sleep(RETRY_DELAY);
                continue;
            }

            printf("[INFO] Attempting to connect to ABOS1 at %s:%d\n",
                   SERVER_IP_OUTBOUND, SERVER_PORT_OUTBOUND);

            /* ABOS1への接続試行 */
            if (connect(client_sock_fd, (struct sockaddr *)&serv_addr_out, 
                       sizeof(serv_addr_out)) == 0) {
                printf("[INFO] Outbound connection established\n");
                break;
            }

            /* 接続失敗時のリトライ */
            perror("[WARN] Connection failed (retrying...)");
            close(client_sock_fd);
            client_sock_fd = -1;
            sleep(RETRY_DELAY);
        }

        /* メッセージの生成と送信 */
        generate_message(send_buffer, sizeof(send_buffer));

        if (send(client_sock_fd, send_buffer, strlen(send_buffer), 0) < 0) {
            perror("[ERROR] Send failed to ABOS1");
        } else {
            printf("[SEND] Message sent via %s: %s\n", 
                   CLIENT_IP_OUTBOUND_SRC, send_buffer);
        }

        /* 往路接続のクローズ */
        close(client_sock_fd);
        printf("[INFO] Outbound connection closed\n");

        /* ====================================================================
         * 復路処理: ABOS1からの応答を受信
         * ==================================================================== */
        
        printf("[INFO] Starting inbound server to wait for response\n");

        /* Server機能の起動とリトライループ */
        while (TRUE) {
            listen_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (listen_sock < 0) {
                perror("[ERROR] Listen socket creation failed");
                return 1;
            }

            /* ポート再利用設定 */
            if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, 
                          &opt, sizeof(opt)) < 0) {
                perror("[WARN] Failed to set SO_REUSEADDR");
            }

            /* バインドとリッスン */
            if (bind(listen_sock, (struct sockaddr *)&bind_addr_in, 
                    sizeof(bind_addr_in)) == 0 && 
                listen(listen_sock, MAX_PENDING) == 0) {
                printf("[INFO] Listening on %s:%d\n", 
                       CLIENT_IP_INBOUND, CLIENT_PORT_INBOUND);
                break;
            }

            /* 待ち受け失敗時のリトライ */
            perror("[WARN] Failed to start inbound server (retrying...)");
            close(listen_sock);
            listen_sock = -1;
            sleep(RETRY_DELAY);
        }

        /* ABOS1からの接続を受け入れ */
        addrlen = sizeof(client_addr_in);
        accept_sock = accept(listen_sock, (struct sockaddr *)&client_addr_in, 
                           &addrlen);
        close(listen_sock); /* 接続受け入れ後、待ち受けソケットをクローズ */

        if (accept_sock < 0) {
            perror("[ERROR] Accept failed for inbound connection");
        } else {
            printf("[INFO] Response connection accepted from ABOS1\n");

            /* 応答メッセージの受信 */
            memset(recv_buffer, 0, BUFFER_SIZE);
            ssize_t bytes_received = recv(accept_sock, recv_buffer, 
                                         BUFFER_SIZE - 1, 0);

            if (bytes_received > 0) {
                recv_buffer[bytes_received] = '\0';
                printf("[RECV] Response received via %s: %s\n", 
                       CLIENT_IP_INBOUND, recv_buffer);
            } else if (bytes_received == 0) {
                printf("[INFO] ABOS1 closed the response connection\n");
            } else {
                perror("[ERROR] Inbound receive failed");
            }

            close(accept_sock);
        }

        /* 次のサイクルまで待機 */
        printf("[INFO] Waiting %d seconds before next cycle\n", MESSAGE_INTERVAL);
        sleep(MESSAGE_INTERVAL);
    }
    
    return 0;
}
