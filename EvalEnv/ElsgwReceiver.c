#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* ============================================================================
 * ネットワーク設定 (ABOS1上で、ELSGWからのTCP接続を待ち受け)
 * ============================================================================ */
#define SERVER_IP_INBOUND   "192.168.100.1" /* ABOS1のeth0 IP */
#define SERVER_PORT_INBOUND 52000         /* ELSGWの連携ポートに合わせる */

/* ============================================================================
 * 動作設定
 * ============================================================================ */
#define RETRY_DELAY         1       /* 接続リトライ間隔 (秒) */
#define BUFFER_SIZE         1024    /* バッファサイズ */
#define MAX_PENDING         5       /* 待ち受けキューの最大数 */
#define TRUE                1

/* ============================================================================
 * 関数: handle_client_connection
 * 機能: ELSGWからの接続を処理し、メッセージを受信
 * ============================================================================ */
void handle_client_connection(int client_sock_fd) {
    char client_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    /* 接続元IPの取得 */
    if (getpeername(client_sock_fd, (struct sockaddr *)&client_addr, &addrlen) == 0) {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("[INFO] Connection accepted from ELSGW at %s\n", client_ip);
    } else {
        strcpy(client_ip, "Unknown");
    }

    /* メッセージの受信ループ (APIは短文と想定し、ここでは一度のrecvで処理) */
    bytes_read = recv(client_sock_fd, client_buffer, sizeof(client_buffer) - 1, 0);

    if (bytes_read > 0) {
        client_buffer[bytes_read] = '\0';
        printf("[RECV] ELSGW API Packet Received:\n");
        printf(">> %s\n", client_buffer);
        
        // ★ ここに受信したパケットの処理（例えば、ログ記録や機器制御への反映）を追加
        
    } else if (bytes_read == 0) {
        printf("[INFO] ELSGW disconnected gracefully\n");
    } else {
        perror("[ERROR] Failed to receive data from ELSGW");
    }

    /* 受信ソケットのクローズ */
    close(client_sock_fd);
}

/* ============================================================================
 * 関数: main
 * 機能: Server機能を起動し、ELSGWからの接続を待ち受け
 * ============================================================================ */
int main(int argc, char *argv[]) {
    int listen_sock = -1;
    int client_sock_fd = -1;
    struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;
    socklen_t addrlen;
    int opt = 1;

    printf("============================================================\n");
    printf("  ELSGW Receiver Server (ABOS1, C) Starting\n");
    printf("  Listening on %s:%d\n", SERVER_IP_INBOUND, SERVER_PORT_INBOUND);
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
      
      /* 待ち受けアドレスの設定 (192.168.100.1:52000) */
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
            printf("[INFO] Server listening successfully.\n");
            break;
        }

        /* 待ち受け失敗時のリトライ */
        perror("[WARN] Failed to start server (retrying...)");
        close(listen_sock);
        listen_sock = -1;
        sleep(RETRY_DELAY);
    }

    /* 接続受け入れループ (Ctrl+cで終了するまで継続) */
    while (TRUE) {
        addrlen = sizeof(client_addr);

        /* ELSGWからの接続を待機 */
        client_sock_fd = accept(listen_sock, (struct sockaddr *)&client_addr, 
                                &addrlen);

        if (client_sock_fd < 0) {
            if (errno != EINTR && errno != EAGAIN) {
                perror("[ERROR] Accept failed");
                sleep(RETRY_DELAY);
            }
            continue;
        }

        /* 接続処理 (受信) */
        handle_client_connection(client_sock_fd);
    }

    /* クリーンアップ (Ctrl+cで終了しない限り到達しない) */
    if (listen_sock != -1) {
        close(listen_sock);
    }

    return 0;
}
