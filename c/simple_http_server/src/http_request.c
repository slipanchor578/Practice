#include <stdio.h>
#include <string.h>
#include "internal.h"

// リクエストパスをローカルファイルパスに変換する
int normalize_path(const char* path, char* out, size_t outsize) {
    // 先頭の「/」を外す。無ければそのまま
    const char* p = (path[0] == '/') ? path + 1 : path;
    // 「localhost:8080/」の時とかpはNULLなのでindex.htmlを書き込む
    if (*p == '\0') {
        snprintf(out, outsize, "index.html");
        return 0;
    }

    // 「..」が含まれていたら拒否
    if (strstr(p, "..")) {
        return -1;
    }

    // 現状favicon.icoは用意していない
    if (strcmp(p, "favicon.ico") == 0) {
        return 1;
    }

    // 通常パスを書き込む
    snprintf(out, outsize, "%s", p);
    return 0;
}

// GETメソッドかどうかを判定。現状GETメソッド以外は弾く
int check_method(int client, const char* method) {
    if (strcmp(method, "GET") == 0) {
        return 0;
    }

    send_error(client, 405, "Method Not Allowed");
    return -1;
}

// HTTP/1.1 はクライアント側のHostヘッダの送信を要求する
const char* get_host(const char* buf) {
    // リクエストヘッダ
    const char* p = buf;
    while (1) {
        const char* end = strstr(p, "\r\n");
        // 行末の「\r\n」がなければヘッダが壊れているので終了
        if (!end) break;

        // Hostがない
        // 空行の時はendが動かない。よってend - p = 0 となる
        // ループ終了
        size_t len = end - p;
        if (len == 0) break;

        // Hostがあった場合
        if (strncmp(p, "Host:", 5) == 0) {
            // pは「H」を指しているので「:」の次に移動
            const char* host = p + 5;
            // 仕様上「Host:localhost:1234」のように詰めたり逆にスペース入れまくりでもいける
            while (*host == ' ') ++host;
            return host;
        }

        // pは「\r」を指しているので「\n」を飛ばして、次の行の先頭に移す
        p = end + 2;
    }
    return NULL;
}

// リクエストヘッダを解析する
void parse_headers(const char* buf) {
    // ヘッダの先頭。解析中の位置
    const char* p = buf;
    printf("--- Header Analysis ---\n");

    printf("元のヘッダ:\n%s\n", buf);

    while (1) {
        // strstrは検索対象を完全一致するまで探してくれる
        // この場合「\r」を見つけた時点でreturnとかはない
        // 完全一致で探して、先頭の位置（この場合『\r』）のポインタを返す
        const char* end = strstr(p, "\r\n");
        // 無ければ抜ける
        if (!end) break;

        // 行の長さを計算
        // 例えば「GET / HTTP/1.1\r\n」だったら「14」を返す
        size_t len = end - p;

        // 空行の時
        // 直前のループで「Accept: */*\r\n」を解析したとして、その次に「\r\n」が並んでいるとする
        // この場合ポインタの位置調整でpの先頭が「\r\n」の「\r」となる
        // この時strtrするといきなり一致するのでpと同じポインタを返し、lenも0になる
        if (len == 0) {
            printf("[End of Headers]\n");
            break;
        }

        // ヘッダ行をコピー
        char line[1024];
        // もしヘッダの1行の長さがバッファと同じ、あるいはそれより長い時
        // 最大数 - 1 に詰める。NULL終端のため
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        // コピーしたものを操作するので安全
        memcpy(line, p, len);
        // [G,E,T, ,/, ,H,T,T,P,/,1,.,1,\0] となる
        line[len] = '\0';

        // キー名だけチェックしたいのでstrncmp
        if (strncmp(line, "User-Agent:", 11) == 0) {
            printf("[Browser Info] %s\n", line);
        } else if (strncmp(line, "Accept-Language:", 16) == 0) {
            printf("[Language] %s\n", line);
        } else if (strncmp(line, "Host:", 5) == 0) {
            printf("[Host] %s\n", line);
        }
        
        // 次のループへ
        p = end + 2;
    }
    printf("--------------------\n");
}