#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <stdint.h>
#include "internal.h"

// htmlをsendする
void send_html(int client, const char* status, const char* body) {
    char res[2048];
    // HTTP/1.1 の基本はKeep-Alive
    // なので、サーバー側から接続を切る場合はレスポンスヘッダに「Connection: close\r\n」を書く
    // じゃないとクライアント側は待ち続けたり、レスポンスが壊れたと勘違いする
    snprintf(res, sizeof(res), 
        "HTTP/1.1 %s\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s", status, strlen(body), body);

    write(client, res, strlen(res));
}

// errorをsendする
void send_error(int client, int code, const char* msg) {
    char status[64];
    char body[512];

    // 渡されたパーツでステータスを作成
    snprintf(status, sizeof(status), "%d %s", code, msg);
    // bodyを作成
    snprintf(body, sizeof(body), "<h1>%d %s</h1>", code, msg);
    // send_htmlに投げる
    send_html(client, status, body);
}

// 静的ファイルを渡す
int send_file(int client, const char* local_path) {
    FILE* fp = fopen(local_path, "rb");
    // ファイルが存在しない。無効なパス。権限がない等
    if (!fp) {
        send_error(client, 404, "Not Found");
        return -1;
    }

    struct stat st;
    // ファイルディスクリプタに紐づくinodeのメタデータを得るだけ
    // fseek + ftellより圧倒的に速い。
    if (fstat(fileno(fp), &st) != 0) {
        // fdが壊れていたり、ファイルが削除されたり、ファイルシステムのエラー時
        fclose(fp);
        send_error(client, 500, "Internal Server Error");
        return -1;
    }

    off_t filesize = st.st_size;
    const char* mime = get_mime_type(local_path);
    char header[256];

    // ヘッダ組み立て
    snprintf(header, sizeof(header), 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %jd\r\n"
            "Connection: close\r\n"
            "\r\n", mime, (intmax_t)filesize);

    // 先に送る
    if (write(client, header, strlen(header)) < 0) {
        // クライアントが切断した
        // ソケットが閉じてる
        // 基本的に外部要因らしい
        perror("write header failed");
        fclose(fp);
        return -1;
    }

    int fd = fileno(fp);
    off_t offset = 0;

    // 送信先。送信ファイル。オフセット。バイト数
    // 普通はfreadとかで読むと、カーネルからユーザー空間にコピーして、またwriteするのにカーネルにコピーする
    // sendfileならfopenで開いたデータをカーネル空間に置いたまま、いきなり相手に送れる
    ssize_t sent = sendfile(client, fd, &offset, filesize);
    // 基本外部要因で切れる
    if (sent < 0) {
        perror("sendfile failed");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}