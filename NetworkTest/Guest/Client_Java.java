/*
 * ============================================================================
 * Client_Java.java - ABOS2 TCP Client Program
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

import java.io.*;
import java.net.*;

/**
 * ABOS2のTCP Clientプログラム
 * ABOS1へメッセージを送信し、応答を受信する
 */
public class Client_Java {
    /* ========================================================================
     * ネットワーク設定
     * ======================================================================== */
    /** 往路設定: ABOS1への送信 */
    private static final String SERVER_IP_OUTBOUND = "192.168.100.1";
    private static final int SERVER_PORT_OUTBOUND = 8000;
    private static final String CLIENT_IP_OUTBOUND_SRC = "192.168.100.2";

    /** 復路設定: ABOS1からの応答を待ち受け */
    private static final String CLIENT_IP_INBOUND = "192.168.200.2";
    private static final int CLIENT_PORT_INBOUND = 8000;

    /* ========================================================================
     * 動作設定
     * ======================================================================== */
    /** メッセージ送信サイクル間隔 (ミリ秒) */
    private static final int MESSAGE_INTERVAL = 1000;
    /** 接続リトライ間隔 (ミリ秒) */
    private static final int RETRY_DELAY = 1000;
    /** 待ち受けキューの最大数 */
    private static final int MAX_PENDING = 5;

    /* ========================================================================
     * プログラム情報
     * ======================================================================== */
    private static final String CLIENT_HOST_NAME = "ABOS2";
    private static final String CLIENT_LANGUAGE = "Java";

    /**
     * ABOS1へ送信するメッセージを生成
     * 
     * @return メッセージ文字列
     */
    private static String generateMessage() {
        return String.format("Hello from %s written by %s",
                           CLIENT_HOST_NAME, CLIENT_LANGUAGE);
    }

    /**
     * メインメソッド
     * ABOS1への定期的なメッセージ送信と応答受信を行う
     * 
     * @param args コマンドライン引数 (未使用)
     */
    public static void main(String[] args) {
        System.out.println("============================================================");
        System.out.printf("  Client/Server (%s, %s) Starting\n",
                         CLIENT_HOST_NAME, CLIENT_LANGUAGE);
        System.out.println("============================================================");

        /* メッセージ送受信ループ */
        while (true) {
            /* ================================================================
             * 往路処理: ABOS1へメッセージを送信
             * ================================================================ */
            Socket clientSocket = null;
            String message = generateMessage();
            boolean connected = false;

            /* 往路接続のリトライループ */
            while (!connected) {
                try {
                    System.out.printf("[INFO] Attempting to connect to ABOS1 at %s:%d\n",
                                    SERVER_IP_OUTBOUND, SERVER_PORT_OUTBOUND);

                    /* 送信元IPアドレスを明示的に指定 (192.168.100.2) */
                    InetAddress localAddress = InetAddress.getByName(CLIENT_IP_OUTBOUND_SRC);
                    clientSocket = new Socket(SERVER_IP_OUTBOUND, SERVER_PORT_OUTBOUND,
                                            localAddress, 0);
                    connected = true;
                    
                    System.out.println("[INFO] Outbound connection established");

                    /* メッセージの送信 */
                    try (PrintWriter writer = new PrintWriter(
                            clientSocket.getOutputStream(), true)) {
                        
                        writer.println(message);
                        System.out.printf("[SEND] Message sent via %s: %s\n",
                                        CLIENT_IP_OUTBOUND_SRC, message);
                    }

                } catch (IOException e) {
                    System.err.printf("[WARN] Connection failed: %s (retrying...)\n",
                                    e.getMessage());
                    
                    if (clientSocket != null) {
                        try {
                            clientSocket.close();
                        } catch (IOException ignore) {
                            /* クローズ失敗は無視 */
                        }
                        clientSocket = null;
                    }

                    /* リトライ前の待機 */
                    try {
                        Thread.sleep(RETRY_DELAY);
                    } catch (InterruptedException ie) {
                        Thread.currentThread().interrupt();
                        System.err.println("[ERROR] Connection retry interrupted");
                        return;
                    }
                }
            }

            /* 往路接続のクローズ */
            if (clientSocket != null) {
                try {
                    clientSocket.close();
                } catch (IOException ignore) {
                    /* クローズ失敗は無視 */
                }
            }
            System.out.println("[INFO] Outbound connection closed");

            /* ================================================================
             * 復路処理: ABOS1からの応答を受信
             * ================================================================ */
            System.out.println("[INFO] Starting inbound server to wait for response");

            try {
                /* Server機能の起動 (192.168.200.2:8000で待ち受け) */
                InetAddress bindAddress = InetAddress.getByName(CLIENT_IP_INBOUND);
                
                try (ServerSocket serverSocket = new ServerSocket(CLIENT_PORT_INBOUND,
                                                                 MAX_PENDING, bindAddress)) {
                    
                    System.out.printf("[INFO] Listening on %s:%d\n",
                                    CLIENT_IP_INBOUND, CLIENT_PORT_INBOUND);

                    /* ABOS1からの接続を受け入れ */
                    try (Socket acceptSocket = serverSocket.accept();
                         BufferedReader reader = new BufferedReader(
                             new InputStreamReader(acceptSocket.getInputStream()))) {

                        System.out.println("[INFO] Response connection accepted from ABOS1");

                        /* 応答メッセージの受信 */
                        String response = reader.readLine();
                        
                        if (response != null) {
                            System.out.printf("[RECV] Response received via %s: %s\n",
                                            CLIENT_IP_INBOUND, response);
                        } else {
                            System.out.println("[WARN] ABOS1 closed the response connection");
                        }

                    } catch (IOException e) {
                        System.err.printf("[ERROR] Inbound receive failed: %s\n",
                                        e.getMessage());
                    }

                } catch (IOException e) {
                    System.err.printf("[ERROR] Failed to start inbound server: %s\n",
                                    e.getMessage());
                }

            } catch (UnknownHostException e) {
                System.err.printf("[ERROR] Invalid inbound address: %s\n", e.getMessage());
            }

            /* 次のサイクルまで待機 */
            System.out.printf("[INFO] Waiting %d ms before next cycle\n", MESSAGE_INTERVAL);
            try {
                Thread.sleep(MESSAGE_INTERVAL);
            } catch (InterruptedException ie) {
                Thread.currentThread().interrupt();
                System.err.println("[ERROR] Message interval interrupted");
                return;
            }
        }
    }
}
