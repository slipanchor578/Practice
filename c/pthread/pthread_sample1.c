#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
// #include <unistd.h>
#include <stdint.h>

void* thread_val(void* arg) {
    // アドレスとしての「100番地」から整数値としての「100」に戻す
    intptr_t val = (intptr_t)arg;
    // 値に戻したので普通に計算できる
    val *= 2;
    // アドレス扱いして値を返す。値はレジスタに乗っているのでスレッドを抜けても問題ない
    // これが&valとかだと、スレッドを抜ける時にスタック領域が使えなくなるので、後で呼び出し側でアクセスすると
    // セグフォする
    // ただし、レジスタに乗せる都合64bitアーキテクチャなら最大8byteまでのデータしか返せない
    // なのでmallocで確保した先にデータを書き込んで、その先頭アドレスを返す方法
    return (void*)val;
}

void* thread_malloc(void* arg) {
    int* res = malloc(sizeof(int));
    *res = 999;
    // 普通にヒープへのアドレスを返すパターン
    // データが8byteを超えたらこうする
    return (void*)res;
}

// 別にpthreadに渡す関数でなくとも、普通の関数もアドレスに誤認させて値を返せる
// ただし普通に「return x」で値渡しすればいいだけの話
void* test(void) {
    int x = 100;
    return (void*)(intptr_t)x;
}

int main(void) {

    pthread_t th1, th2;
    void* ret1 = NULL;
    int* ret2 = NULL;

    // 100 は普通int32なので4byte
    // これをintptr_tでポインタと同じ8byteでの100に変更
    // 最後に「100番地」のように解釈させるためvoid*でキャストして渡す
    pthread_create(&th1, NULL, thread_val, (void*)(intptr_t)100);
    pthread_join(th1, &ret1);
    // 理屈としてはアドレス値として返ってきているので、同じサイズのintptr_tでキャストして値として表示
    printf("パターンA (数値): %ld\n", (intptr_t)ret1);

    pthread_create(&th2, NULL, thread_malloc, NULL);
    // pthread_join は呼び出し先のデータのアドレスを、呼び出し元のポインタの参照先に入れたい
    // そのためにはポインタが配置されているアドレスを特定しないといけない
    // ポインタ変数の場所を渡すには「(void**)」でキャストする
    pthread_join(th2, (void**)&ret2);
    if (ret2 != NULL) {
        printf("パターンB (住所): %d\n", *ret2);
        free(ret2);
    }

    // やろうと思えば「int」で返す所を「void*」で返せる。ただしpthread以外では意味はない
    printf("%ld\n", (intptr_t)test());

    return EXIT_SUCCESS;
}