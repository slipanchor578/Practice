#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <signal.h>

int main(void) {
    // コネクションが切れたソケットに書き込みを行うとSIGPIPEシグナルが発生して、サーバーが落ちる
    // つまりクライアントが切断したソケットにサーバーがいつも通りデータ送信しようとしただけでサーバーが落ちる
    // これを避けるため無視するようにする
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    int server = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    // 1で有効、0で無効
    int opt = 1;
    // 設定対象のソケット
    // ソケット全般
    // アドレスの再利用
    // 有効
    // サイズ
    // 通常、接続終了するとポートはしばらく再利用できない。そのままだと開発効率も悪くなるし
    // 本番でサーバーが落ちて、再起動してもただちに8080ポートで待ち受けできない
    // これを無視する
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, 5);

    printf("Server started. Waiting for connections...\n");

    while (1) {
        int client = accept(server, NULL, NULL);
        if (client < 0) {
            perror("accept error");
            continue;
        }

        pid_t pid = fork();

        if (pid == 0) {
            // 子プロセスは待受ソケットは必要ない
            // 親プロセスが先に閉じた時にソケットを子プロセスが握りっぱなしにしないように
            close(server);

            char buf[1024];
            int n = read(client, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
            }

            // こういうリクエストが来る
            // GET / HTTP/1.1
            // Host: localhost:8080
            // User-Agent: curl/8.5.0
            // サーバーに対して何でもいいからデータくれと言っている。普通は「text/html」とか
            // Accept: */*

            // printf("%s\n", buf);

            char methods[16];
            char path[256];

            // 空白が出るまで最大15文字読む。バッファにコピーしてくれる。最後をヌル埋めしてくれる
            // positionを覚えていて、空白の次に文字がある所から、次の空白まで、2つ目のバッファに入れてくれる
            // ここで判定しないと、空のpathとかをstrcmpしたりしてしまう
            if (sscanf(buf, "%15s %255s", methods, path) < 2) {
                // 空のリクエスト
                // バイナリデータを直接送る
                // 「GET」だけしかないとか
                // 「あいうえお」とか直接送る
                // タイムアウトでreadが空で終わった
                // パケットが途中で壊れた
                // わざと壊れたリクエストを送りつける
                printf("Invalid request format\n");
                close(client);
                exit(1);
            }
            printf("Process %d: %s %s\n", getpid(), methods, path);

            // 区切り文字に当たった時、そこを\0にして、次に読む時にNULLから読まないようにポインタを1ずらす
            // いきなり区切り文字に当たった時は空文字を返す
            // いきなりNULL(つまり本来の文字列の終端)に当たった時はNULLを返す
            // ここで「GET / HTTP/1.1\r\n」を取得して、次のstrtokでいらないので飛ばす
            char* line = strtok(buf, "\r\n");

            printf("--- Header Analysis ---\n");

            while (line != NULL) {
                line = strtok(NULL, "\r\n");
                // 空行に当たると右の判定でbreak
                // データが壊れていたりして空行が無かったりしたらNULLに当たってlineもNULLになるので
                // 左の判定も必要
                // read途中で切れてリクエストの途中の時もNULLになる
                if (line == NULL || strlen(line) == 0) break;

                // 見つかれば有効なアドレス
                // なければNULL を返す。見つかった位置 - 検索対象 で先頭位置を得ることもできる
                if (strstr(line, "User-Agent:")) {
                    printf("[Browser Info] %s\n", line);
                } else if (strstr(line, "Accept-Language:")) {
                    printf("[Language] %s\n", line);
                } else if (strstr(line, "Host:")) {
                    printf("[Host] %s\n", line);
                }
            }

            printf("--------------------\n");

            if (strcmp(path, "/favicon.ico") == 0) {
                close(client);
                exit(0);
            }

            char body[1024];
            // 文字列リテラルは.rodataに配置される
            char* status = "200 OK";
            // 完全一致で0、a < b で負の値、a > b で正の値
            if (strcmp(path, "/hello") == 0) {
                sprintf(body, "<h1>Welcome to Hello Page!</h1>");
            } else if (strcmp(path, "/") == 0) {
                FILE* fp = fopen("index.html", "r");
                if (fp == NULL) {
                    status = "404 Not Found";
                    sprintf(body, "<h1>404 Not Found</h1>");
                } else {
                    // body - 1 byte 読み取る
                    size_t len = fread(body, 1, sizeof(body) - 1, fp);
                    // 末尾に NULL埋め
                    body[len] = '\0';
                    fclose(fp);
                }
            } else {
                // "404 Not Found" も読み取りメモリ位置に配置されていて、本番時はポイントを変えるだけ
                status = "404 Not Found";
                sprintf(body, "<h1>404 Not Found</h1><p> The Page '%s' does not exist.</p>", path);
            }

            char res[2048];
            sprintf(res, "HTTP/1.1 %s\r\n"
            "Content-Type: text/html\r\n"
            "Content-Length: %ld\r\n"
            "\r\n"
            "%s", 
            status, 
            strlen(body), body);

            if (write(client, res, strlen(res)) < 0) {
                perror("write failed");
            } else {
                printf("write successfull!\n\n");
            }
         
            close(client);
            exit(0);
        } else if (pid > 0) {
            close(client);
        } else {
            perror("fork failed");
            close(client);
        }
    }

    close(server);
    return EXIT_SUCCESS;
}