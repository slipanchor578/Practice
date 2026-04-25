#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

int queue = 0;
// データを入れたら1
int has_data = 0;
// 終了指示したら1
int quit = 0;
// 動くと1
int processing = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// quit 用の条件変数
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
// processing 用の条件変数
pthread_cond_t cond_done = PTHREAD_COND_INITIALIZER;

void* consumer(void* arg) {
    while (1) {
        // pthread_cond_wait を呼ぶ前にmutex_lock を呼ぶのが鉄則。cond_wait は内部で mutex_unlock を
        // 呼ぶため

        // これが鉄則
        // pthread_mutex_lock(&mutex);

        // while (条件) {
        //     pthread_cond_wait(&cond, &mutex);
        // }

        // pthread_mutex_unlock(&mutex);

        pthread_mutex_lock(&mutex);

        // データが無い&終了指示が無い時スリープさせる
        while (!has_data && !quit) {
            // mutexを自動でunlock
            // スレッドをスリープに
            // cond_signalが来たら、自動でmutexをlockし直して復帰
            // 処理の中にmutex_lockが組み込まれているので、mutex_lockを呼んでないけどロックできる
            // signalを送る側は、signalを送った後mutexをunlockするので、通知後にmutexをlockできる
            pthread_cond_wait(&cond, &mutex);
        }

        if (quit) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        // ここは共有データの更新なので mutex が必要
        int n = queue;
        has_data = 0;
        processing = 1;
        // mutex は共有データの更新のためだけに使うので、もう解放する。
        // producer 側は数字を入れた後 cond_wait しているので解放してあげないと次にいけない

        pthread_mutex_unlock(&mutex);

        for (int i = 1; i <= n; ++i) {
            printf("%d\n", i);
            sleep(1);
        }

        pthread_mutex_lock(&mutex);
        processing = 0;
        pthread_cond_signal(&cond_done);
        pthread_mutex_unlock(&mutex);
    }

    // 戻り値がいらない場合NULLを返すようにする
    return NULL;
}

int main(void) {

    pthread_t th;
    pthread_create(&th, NULL, consumer, NULL);

    char buf[32];

    while (1) {

        // 改行がないprintf は画面に出ないことがある。なので強制的にfflushを呼んでwriteさせる
        printf("数字を入力 (qで終了): ");
        fflush(stdout);

        // EOF からエラー時、あるいは「q」が入力された時
        if (!fgets(buf, sizeof(buf), stdin) || buf[0] == 'q') {
            pthread_mutex_lock(&mutex);
            quit = 1;
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&mutex);
            break;
        }

        int n = atoi(buf);
        if (n <= 0) continue;

        pthread_mutex_lock(&mutex);
        queue = n;
        has_data = 1;
        processing = 1;
        pthread_cond_signal(&cond);
        pthread_mutex_unlock(&mutex);

        // 出力中に「数字を入力」と表示させないためここでスリープさせる
        pthread_mutex_lock(&mutex);
        while (processing) {
            pthread_cond_wait(&cond_done, &mutex);
        }
        pthread_mutex_unlock(&mutex);
    }

    pthread_join(th, NULL);
    
    return EXIT_SUCCESS;
}

/*
    まず、前提として mutex_lock で止めたスレッドを cond_signal では起こせない
    cond_wait で止めたスレッドを mutex_unlock では起こせない
    これらは別々のキューを持っているので、mutex_unlock による通知は mutex_lock にしか飛ばないので
    cond_wait の待機キューには飛ばない

    mutex_lock, unlock だけで条件待ちする場合

    pthread_mutex_lock(&mutex);
    while (!has_data) {
        pthread_mutex_unlock(&mutex);
        sleep(1);
        pthread_mutex_lock(&mutex);
    }

    みたいな感じになるが
    unlock -> sleep -> lock がatomic ではないので、この間に条件変数が条件を満たしていても
    それを知らず、次の「!has_data」で初めて条件を満たしたことを知る。つまりCPUの無駄遣い
    というか、mutex はそもそも排他処理を実行する要素でしか無い

    T0: consumer → unlock
    T1: consumer → sleep(1)
    T2: producer → has_data=1 にする
    T3: producer → mutex_unlock
    T4: consumer → まだ sleep 中（気づかない）
    T5: consumer → wakeup
    T6: consumer → mutex_lock
    T7: consumer → has_data=1 を確認（1 秒遅れ）

    となる。しかも

    has_data = 1;
    has_data = 0; // すぐに別の処理で戻す

    みたいな感じで producer 側が一瞬条件を満たしても mutex だけでは一瞬だけ条件を満たした状況を見逃す
    通知機能がないから。完全に条件判定のタイミング依存となる。その一瞬だけ条件を満たしていなければ普通に見逃す

    これが cond_wait, signal を使った待ちの場合は

    T0: consumer → cond_wait（unlock + sleep）
    T1: producer → has_data=1
    T2: producer → cond_signal
    T3: consumer → wakeup（即時）
    T4: consumer → mutex_lock（空いた瞬間）
    T5: consumer → has_data=1 を確認（遅れなし）

    スレッドの起床とロック取得がatomicに行われるので、条件変数を見ている間に producer 側が条件変数を
    更新しているみたいなことが起きない

    似ているように見えて「排他処理」「条件待ち」は全然違う
    mutex はロック、アンロック、アンロックまでスレッドスリープはできる
    しかし、「条件を満たすまでスリープ、満たせば起床」をそれ自体ではできない。
    なので条件判定の一瞬の間に「条件を満たす -> 満たさなくなった」となった時に見逃す

    cond_wait, signal
    条件を満たすまでスレッドスリープ、起床できる
    条件を満たしてsignalを打てば、wait側に処理を移譲できるので、「その間に条件変数の値が変えられた」とかがない
    ただしmutexの排他処理はできない。求められていることが違うので、排他処理がしたければmutexを使う
    条件待ちを安全に行うためにmutexを使っているのでcond_wait とかにも排他処理があるように見えるだけ
*/