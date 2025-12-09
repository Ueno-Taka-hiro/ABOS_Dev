/*
 * ============================================================================
 * Bridge_Java.java - ABOS1 TCP Bridge Program
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

import java.io.*;
import java.net.*;

/**
 * ABOS1のTCP Bridgeプログラム
 * ABOS2からのメッセージを受信し、応答を返送する
 */
public class Bridge_Java {
    /* ========================================================================
     * ネットワーク設定
     * ======================================================================== */
    /** 往路設定: ABOS2からの接続を待ち受け */
    private static final String SERVER_IP_INBOUND = "192.168.100.1";
    private static final int SERVER_PORT_INBOUND = 8000;

    /** 復路設定: ABOS2へ応答を送信 */
    private static final String CLIENT_IP_OUTBOUND = "192.168.200.2";
    private static final int CLIENT_PORT_OUTBOUND = 8000;
    private static final String RESPONSE_SRC_IP = "192.168.200.1";

    /* ========================================================================
     * 動作設定
     * ======================================================================== */
    /** 接続リトライ間隔 (ミリ秒) */
    private static final int RETRY_DELAY = 1000;
    /** 待ち受けキューの最大数 */
    private static final int MAX_PENDING = 5;

    /* ========================================================================
     * プログラム情報
     * ======================================================================== */
    private static final String RESPONDER_HOST_NAME = "ABOS1";
    private static final String RESPONDER_LANGUAGE = "Java";

    /**
     * ABOS2へ返信する応答メッセージを生成
     * 
     * @param clientMessage ABOS2から受信したメッセージ
     * @return 応答メッセージ
     */
    private static String generateResponse(String clientMessage) {
        return String.format(
            "Response from %s written by %s via %s --- Received: %s",
            RESPONDER_HOST_NAME, RESPONDER_LANGUAGE, RESPONSE_SRC_IP, clientMessage
        );
    }

    /**
     * ABOS2へ応答メッセージを送信
     * 接続失敗時は自動的にリトライする
     * 
     * @param responseMessage 送信する応答メッセージ
     */
    private static void sendResponse(String responseMessage) {
        System.out.printf("[INFO] Attempting response connection to ABOS2 at %s:%d\n",
                         CLIENT_IP_OUTBOUND, CLIENT_PORT_OUTBOUND);

        Socket respSocket = null;
        boolean sent = false;

        while (!sent) {
            try {
                /* 送信元IPアドレスを明示的に指定 (192.168.200.1) */
                InetAddress localAddress = InetAddress.getByName(RESPONSE_SRC_IP);
                respSocket = new Socket(CLIENT_IP_OUTBOUND, CLIENT_PORT_OUTBOUND,
                                       localAddress, 0);
                
                System.out.println("[INFO] Response connection established");

                /* 応答メッセージの送信 */
                try (OutputStream out = respSocket.getOutputStream();
                     PrintWriter writer = new PrintWriter(out, true)) {
                    
                    writer.println(responseMessage);
                    System.out.printf("[SEND] Response sent via %s: %s\n",
                                    RESPONSE_SRC_IP, responseMessage);
                    sent = true;
                }

            } catch (IOException e) {
                System.err.printf("[WARN] Response connection failed: %s (retrying...)\n",
                                e.getMessage());
                
                if (respSocket != null) {
                    try {
                        respSocket.close();
                    } catch (IOException ignore) {
                        /* クローズ失敗は無視 */
                    }
                    respSocket = null;
                }

                /* リトライ前の待機 */
                try {
                    Thread.sleep(RETRY_DELAY);
                } catch (InterruptedException ie) {
                    Thread.currentThread().interrupt();
                    System.err.println("[ERROR] Response retry interrupted");
                    return;
                }
            } finally {
                if (respSocket != null && sent) {
                    try {
                        respSocket.close();
                    } catch (IOException ignore) {
                        /* クローズ失敗は無視 */
                    }
                }
            }
        }
    }

    /**
     * メインメソッド
     * Server機能を起動し、ABOS2からの接続を待ち受ける
     * 
     * @param args コマンドライン引数 (未使用)
     */
    public static void main(String[] args) {
        System.out.println("============================================================");
        System.out.printf("  Bridge Server/Client (%s, %s) Starting\n",
                         RESPONDER_HOST_NAME, RESPONDER_LANGUAGE);
        System.out.println("============================================================");

        try {
            /* Server機能の起動 (192.168.100.1:8000で待ち受け) */
            InetAddress bindAddress = InetAddress.getByName(SERVER_IP_INBOUND);
            ServerSocket serverSocket = new ServerSocket(SERVER_PORT_INBOUND,
                                                        MAX_PENDING, bindAddress);
            
            System.out.printf("[INFO] Listening on %s:%d\n",
                            SERVER_IP_INBOUND, SERVER_PORT_INBOUND);

            /* 接続受け入れループ */
            while (true) {
                try (Socket clientSocket = serverSocket.accept();
                     BufferedReader reader = new BufferedReader(
                         new InputStreamReader(clientSocket.getInputStream()))) {

                    System.out.println("[INFO] Connection accepted from ABOS2");

                    /* ABOS2からのメッセージ受信 */
                    String clientMessage = reader.readLine();
                    
                    if (clientMessage != null) {
                        System.out.printf("[RECV] Message from ABOS2 via %s:%d: %s\n",
                                        SERVER_IP_INBOUND, SERVER_PORT_INBOUND,
                                        clientMessage);

                        /* 応答メッセージの生成と送信 */
                        String responseMessage = generateResponse(clientMessage);
                        sendResponse(responseMessage);
                        
                    } else {
                        System.out.println("[WARN] ABOS2 disconnected during receive");
                    }

                } catch (SocketTimeoutException e) {
                    /* タイムアウトは無視して継続 */
                    System.err.println("[WARN] Socket timeout, continuing...");
                    
                } catch (IOException e) {
                    System.err.printf("[ERROR] Error handling client connection: %s\n",
                                    e.getMessage());
                }
            }

        } catch (IOException e) {
            System.err.printf("[ERROR] Failed to start server on %s:%d: %s\n",
                            SERVER_IP_INBOUND, SERVER_PORT_INBOUND, e.getMessage());
            System.exit(1);
        }
    }
}
