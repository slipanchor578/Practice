#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {

    // 通信に使うファイルディスクリプタをOSに作らせる
    // 成功時は3以上の正の数。失敗時は-1でerrnoがセットされる
    // AF_INET = IPv4, ちなみにAF_UNIXにするとプロセス間通信用のfdを作成できる
    // SOCK_STREAM = TCP, SOCK_DGRAM = UDP となる
    // TCPなので第3引数は0でいい？複数プロトコルに対応させる場合はここを操作する？
    // これでソケットができる
    int server = socket(AF_INET, SOCK_STREAM, 0);

    // サーバーがどのIPアドレス、ポートで待ち受けるかをOSに登録するための構造体を作成する
    // まずは0初期化
    struct sockaddr_in addr = {0};
    // TCP接続
    addr.sin_family = AF_INET;
    // 8080ポートで受付。
    // CPUはリトルエンディアン。ネットワークはビッグエンディアンなので、これを変換するために
    // htons(host to network short)を使う。
    addr.sin_port = htons(8080);
    // どのネットワークインターフェースでも受け付けるという意味
    // 127.0.0.1(ローカル)
    // 192.168.x.x(LAN)
    // 10.x.x.x(グローバル)など、どのIPアドレスからの接続も受け付ける
    // 0.0.0.0:8080
    addr.sin_addr.s_addr = INADDR_ANY;

    // サーバーのソケットをOSに、このIPとポートで待ち受けると登録する意味
    // IPはさっき決めたs_addr = 0.0.0.0、ポートは8080
    // もしバインド時にポートが使われていたらエラーになる
    // このソケットに住所(IP、ポート)を割り当てる
    // sockaddrは汎用的なソケットアドレス構造体。sockaddr_in はIPv4専用なのでキャストして渡す
    // 第3引数は構造体のサイズ
    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    // このソケットでTCP接続を受け入れる。最大5件までの接続を待ち受け。リクエストを5件まで貯める
    // 5件目までキューに入っている時に6件目のアクセスが来るとOSレベルで拒否されるらしい
    // TCP接続が成功してないのでhttp接続されない。拒否された時に404エラーとかが出ることはない
    listen(server, 5);

    // アクセスが来るまで、ここで割り込み待ち
    // 戻り値はクライアント用のfd
    // このサーバーに届いた接続要求を1件取り出して、そのクライアント専用のソケットを作って返してという意味
    // もしクライアントのIPとポートが知りたければ、クライアント用のsockaddr構造体を用意しておいてセットすれば
    // accept成功時にセットされる
    int client = accept(server, NULL, NULL);

    // HTTPリクエストを読み取り。GET / HTTP/1.1 みたいな
    char buf[1024];
    read(client, buf, sizeof(buf));

    // 隣接する文字列リテラルはコンパイル時に自動で連結される
    // HTTPの行区切りは必ずCRLF(\r\n)を付ける
    // そして、ヘッダ終端には単独で「\r\n」を付ける。2つ連続で「\r\n\r\n」が続く直後がBodyの開始位置となる
    char res[] = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Hello, World!";

    // レスポンスを返す
    write(client, res, strlen(res));
    // 1回のリクエスト処理が終わればcloseする
    close(client);
    // 常駐させるなら閉じない
    close(server);

    return EXIT_SUCCESS;
}