/*
 * ============================================================================
 * Bridge_C.c - ABOS1 TCP Bridge Program
 * ============================================================================
 * 機能:
 *   - ABOS2からの往路接続を受信 (Server機能: 192.168.100.1:8000)
 *   - ABOS2へ応答を返信 (Client機能: 192.168.200.2:8000へ接続)
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
/* 往路設定: ABOS2からの接続を待ち受け */
#define SERVER_IP_INBOUND       "192.168.100.1"
#define SERVER_PORT_INBOUND     8000

/* 復路設定: ABOS2へ応答を送信 */
#define CLIENT_IP_OUTBOUND      "192.168.200.2"
#define CLIENT_PORT_OUTBOUND    8000
#define RESPONSE_SRC_IP         "192.168.200.1"

/* ============================================================================
 * 動作設定
 * ============================================================================ */
#define RETRY_DELAY             1       /* 接続リトライ間隔 (秒) */
#define BUFFER_SIZE             1024    /* バッファサイズ */
#define MAX_PENDING             5       /* 待ち受けキューの最大数 */
#define TRUE                    1

/* ============================================================================
 * プログラム情報
 * ============================================================================ */
const char *RESPONDER_HOST_NAME = "ABOS1";
const char *RESPONDER_LANGUAGE  = "C";

/* ============================================================================
 * 関数: generate_response
 * 機能: ABOS2へ返信する応答メッセージを生成
 * 引数:
 *   response_buffer - 応答メッセージ格納先バッファ
 *   buffer_size     - バッファサイズ
 *   client_message  - ABOS2から受信したメッセージ
 * ============================================================================ */
void generate_response(char *response_buffer, size_t buffer_size, 
                       const char *client_message) {
    snprintf(response_buffer, buffer_size,
             "Response from %s written by %s via %s --- Received: %s",
             RESPONDER_HOST_NAME, RESPONDER_LANGUAGE, 
             RESPONSE_SRC_IP, client_message);
}

/* ============================================================================
 * 関数: handle_client_connection
 * 機能: ABOS2からの接続を処理し、応答を返送
 * 引数:
 *   client_sock_fd - ABOS2との接続済みソケット
 * 処理フロー:
 *   1. ABOS2からメッセージを受信 (往路)
 *   2. 応答メッセージを生成
 *   3. ABOS2へ接続 (復路)
 *   4. 応答メッセージを送信
 * ============================================================================ */
void handle_client_connection(int client_sock_fd) {
    char client_buffer[BUFFER_SIZE];
    char response_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int resp_sock = -1;
    struct sockaddr_in resp_addr;
    struct sockaddr_in src_addr;

    /* 往路メッセージの受信 */
    bytes_read = recv(client_sock_fd, client_buffer, sizeof(client_buffer) - 1, 0);

    if (bytes_read > 0) {
        client_buffer[bytes_read] = '\0';
        printf("[RECV] Message from ABOS2 via %s:%d: %s\n", 
               SERVER_IP_INBOUND, SERVER_PORT_INBOUND, client_buffer);

        /* 応答メッセージの生成 */
        generate_response(response_buffer, sizeof(response_buffer), client_buffer);

        /* 復路接続先の設定 (ABOS2) */
        memset(&resp_addr, 0, sizeof(resp_addr));
        resp_addr.sin_family = AF_INET;
        resp_addr.sin_port = htons(CLIENT_PORT_OUTBOUND);

        if (inet_pton(AF_INET, CLIENT_IP_OUTBOUND, &resp_addr.sin_addr) <= 0) {
            fprintf(stderr, "[ERROR] Invalid address: %s\n", CLIENT_IP_OUTBOUND);
            close(client_sock_fd);
            return;
        }

        /* 復路接続のリトライループ */
        while (TRUE) {
            resp_sock = socket(AF_INET, SOCK_STREAM, 0);
            if (resp_sock < 0) {
                perror("[ERROR] Response socket creation failed");
                break;
            }

            /* 送信元IPアドレスを明示的にバインド (192.168.200.1) */
            memset(&src_addr, 0, sizeof(src_addr));
            src_addr.sin_family = AF_INET;
            src_addr.sin_port = htons(0); /* OSが自動割り当て */
            inet_pton(AF_INET, RESPONSE_SRC_IP, &src_addr.sin_addr);

            if (bind(resp_sock, (struct sockaddr *)&src_addr, sizeof(src_addr)) < 0) {
                perror("[ERROR] Failed to bind response source IP");
                close(resp_sock);
                resp_sock = -1;
                break;
            }

            printf("[INFO] Attempting response connection to ABOS2 at %s:%d\n",
                   CLIENT_IP_OUTBOUND, CLIENT_PORT_OUTBOUND);

            /* ABOS2への接続試行 */
            if (connect(resp_sock, (struct sockaddr *)&resp_addr, 
                       sizeof(resp_addr)) == 0) {
                printf("[INFO] Response connection established\n");
                break;
            }

            /* 接続失敗時の処理 */
            if (errno == ECONNREFUSED || errno == ETIMEDOUT || 
                errno == ENETUNREACH) {
                fprintf(stderr, "[WARN] Response connection failed: %s (retrying...)\n",
                        strerror(errno));
                close(resp_sock);
                resp_sock = -1;
                sleep(RETRY_DELAY);
                continue;
            } else {
                perror("[ERROR] Critical response connection error");
                close(resp_sock);
                resp_sock = -1;
                break;
            }
        }

        /* 応答メッセージの送信 */
        if (resp_sock != -1) {
            if (send(resp_sock, response_buffer, strlen(response_buffer), 0) < 0) {
                perror("[ERROR] Response send failed");
            } else {
                printf("[SEND] Response sent via %s: %s\n", 
                       RESPONSE_SRC_IP, response_buffer);
            }
            close(resp_sock);
        }
    } else if (bytes_read == 0) {
        printf("[INFO] ABOS2 disconnected gracefully\n");
    } else {
        perror("[ERROR] Failed to receive data from ABOS2");
    }

    /* 往路ソケットのクローズ */
    close(client_sock_fd);
}

/* ============================================================================
 * 関数: main
 * 機能: Server機能を起動し、ABOS2からの接続を待ち受け
 * ============================================================================ */
int main(int argc, char *argv[]) {
    int listen_sock = -1;
    int client_sock_fd = -1;
    struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;
    socklen_t addrlen;
    int opt = 1;

    printf("============================================================\n");
    printf("  Bridge Server/Client (ABOS1, C) Starting\n");
    printf("============================================================\n");
    
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
      
      /* 待ち受けアドレスの設定 (192.168.100.1:8000) */
      memset(&serv_addr, 0, sizeof(serv_addr));
      serv_addr.sin_family = AF_INET;
      serv_addr.sin_port = htons(SERVER_PORT_INBOUND);
        
        if (inet_pton(AF_INET, SERVER_IP_INBOUND, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "[ERROR] Invalid SERVER_IP_INBOUND: %s\n", 
                SERVER_IP_INBOUND);
        close(listen_sock);
        return 1;
        }

        /* バインドとリッスン */
        if (bind(listen_sock, (struct sockaddr *)&serv_addr, 
                sizeof(serv_addr)) == 0 &&
            listen(listen_sock, MAX_PENDING) == 0) {
            printf("[INFO] Listening on %s:%d\n", 
                   SERVER_IP_INBOUND, SERVER_PORT_INBOUND);
            break;
        }

        /* 待ち受け失敗時のリトライ */
        perror("[WARN] Failed to start server (retrying...)");
        close(listen_sock);
        listen_sock = -1;
        sleep(RETRY_DELAY);
    }

    /* 接続受け入れループ */
    while (TRUE) {
        addrlen = sizeof(client_addr);

        /* ABOS2からの接続を待機 */
        client_sock_fd = accept(listen_sock, (struct sockaddr *)&client_addr, 
                               &addrlen);

        if (client_sock_fd < 0) {
            /* 一時的なエラーは無視して継続 */
            if (errno != EINTR && errno != EAGAIN) {
                perror("[ERROR] Accept failed");
                sleep(RETRY_DELAY);
            }
            continue;
        }

        printf("[INFO] Connection accepted from ABOS2\n");

        /* 接続処理 (往路受信と復路送信) */
        handle_client_connection(client_sock_fd);
    }

    /* クリーンアップ (通常は到達しない) */
    if (listen_sock != -1) {
        close(listen_sock);
    }

    return 0;
   }
