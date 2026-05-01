#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>
#include "http.h"
#include "internal.h"

// リクエストを受け取る
int read_request(int client, char* buf, size_t bufsize) {
    // 何byte読み取ったか
    int total = 0;

    while (1) {
        // bufはリクエストを保存するバッファ。
        // totalを足した位置を開始位置にするのは、例えば1回目で1byte読み込んだ時、次に保存する位置はbuf[1]でないと
        // リクエストが壊れるから
        // 必ず最後はNULL終端させるために、buf - total から更に1byte引いている
        int n = read(client, buf + total, bufsize - total - 1);
        // クライアントが切断した or エラー
        if (n <= 0) {
            return -1;
        }

        // 読み込み量を保存
        total += n;
        // 必ずデータの次はNULL終端しておく
        buf[total] = '\0';

        // もし「ヘッダ\r\n\r\n」であれば、ヘッダの終端 + 空行 でリクエスト終了なので抜ける
        if (strstr(buf, "\r\n\r\n") != NULL) {
            return total;
        }

        // totalが99でbufが100byteの場合、次の読み取りでバッファとNULL終端を貫通するので強制的に抜ける
        if ((size_t)total >= bufsize - 1) {
            return total;
        }
    }
}

void start_server(int port) {
    // クライアントが切断している状態でwriteとかするとSIGPIPEシグナルでサーバーが落ちるので無視
    signal(SIGPIPE, SIG_IGN);
    // waitしていないのでゾンビプロセスになる所を、SIGCHILDを無視して即座に終了させる
    // ゾンビプロセスのままだとPIDが解放されないので最悪forkできなくなる
    signal(SIGCHLD, SIG_IGN);

    int server = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    int opt = 1;
    // ポートを即座に利用可能にする。再起動しても。
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, 5);

    printf("Server started. Waiting for connections...\n");

    while (1) {
        int client = accept(server, NULL, NULL);
        if (client < 0) {
            // perror は自動でstderrに出力される
            perror("accept error");
            continue;
        }

        pid_t pid = fork();

        if (pid == 0) {
            close(server);

            char request_buf[8192];
            // リクエスト受け取り
            int n = read_request(client, request_buf, sizeof(request_buf));
            if (n < 0) {
                close(client);
                exit(0);
            }

            char methods[16];
            char path[256];
            // メソッドとパスが無い時は弾く
            if (sscanf(request_buf, "%15s %255s", methods, path) < 2) {
                fprintf(stderr, "Invalid request format\n");
                close(client);
                exit(0);
            }
            printf("Process %d: %s %s\n", getpid(), methods, path);

            // とりあえずGETメソッド以外は弾いている
            if (check_method(client, methods) < 0) {
                // 切断
                close(client);
                exit(0);
            }

            // HTTP/1.1 を名乗るなら、Hostヘッダのないリクエストには応答してはいけない
            const char* host = get_host(request_buf);
            if (!host) {
                send_error(client, 400, "Bad Request");
                close(client);
                exit(0);
            }

            // /xxx/yyy/zzz.ext なら xxx/yyy/zzz.ext を取得
            char local[256];
            int r = normalize_path(path, local, sizeof(local));
            if (r < 0) {
                send_error(client, 400, "Bad Request");
                close(client);
                exit(0);
            }
            if (r == 1) {
                close(client);
                exit(0);
            }

            // ヘッダ解析
            parse_headers(request_buf);

            // /hello の場合
            if (strcmp(path, "/hello") == 0) {
                send_html(client, "200 OK", "<h1>Welcome to Hello Page!</h1>");
                close(client);
                exit(0);
            }

            // それ以外
            if (send_file(client, local) < 0) {
                    close(client);
                    exit(0);
                }

                close(client);
                exit(0);
        } else if (pid > 0) {
            // メインプロセスは即座にcloseして待機
            close(client);
        } else {
            perror("fork failed");
            close(client);
        }
    }

    close(server);
}