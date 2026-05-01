#include <string.h>
#include <stddef.h>
#include "internal.h"

// mimeタイプを判定する関数
const char* get_mime_type(const char* path) {
    // 拡張子、mimeの構造体のテーブルを作っておく。値は変更不可能
    static const struct {
        const char* ext;
        const char* mime;
    } table[] = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"json", "application/json"}
    };

    // xxx.txt の.txt を得る
    const char* ext = strrchr(path, '.');
    // 拡張子のないパス
    // 汎用バイナリデータ。ブラウザは画面に表示せずダウンロード扱いする
    if (!ext) return "application/octet-stream";
    // ずらす
    ++ext;
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        if (strcmp(ext, table[i].ext) == 0) {
            // 一致したmimeを返す
            return table[i].mime;
        }
    }
    // 対応していなければ
    return "application/octet-stream";
}