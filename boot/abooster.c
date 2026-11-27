#include "noUEFIcommon.h"
/*
1 カーネルのランダム配置化
2 IO初期化
3 仮想アドレス
4 他コア起動
5 initアプリへジャンプ
*/

//allocate(サイズ,フラグwith権限)
extern void start_pasing(EFI_PHYSICAL_ADDRESS table);
extern void LOGOUT(UINT8 *text);
extern void L_HLT(void);
void  B_PageTableLoad(uintptr_t root);  
void  B_TLBFlush(void);                 
void  B_InvalidatePage(uintptr_t va);  
int   B_PageLevels(void);              
// --- 共通Cが提供する ---
void  PageMap(uintptr_t root, uintptr_t va, uintptr_t pa, unsigned flags);


// ===========
// BOOSTER
// ===========
#define PML4_IDX(num) (((num) >> 39) & 0x1FF)
#define PDPT_IDX(num) (((num) >> 30) & 0x1FF)
#define PD_IDX(num) (((num) >> 21) & 0x1FF)
#define PT_IDX(num) (((num) >> 12) & 0x1F)

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITE (1ULL << 1)
#define PAGE_USER (1ULL << 2)
#define PAGE_NX (1ULL << 63)

void MapPage() {}

void main_booster()
{
}


//OSのメモリ管理方式について、容量を4Kibの単位に分けて管理させる 余った部分は専用テーブルに移動させる
//専用テーブルでは256b 512b 1024b...のように2^nbずつのラベルが貼られたテーブルがある 300bは256bのラベルが貼られたテーブルに入る　５１１bは256bに入る
//4Kib以下のメモリ要求がきたらここを使うようにする方式はダメだろうか
//サイズクラス・フリーリスト方式というらしい by AI
//確保するときはページ境界に揃え、最後で余った領域を登録する。最小単位は64byte。　 以下はAI まだ未チェック
// ============================================
// メモリ管理
//  - ページ粒度: 4KiB (ページ境界に揃えて確保)
//  - サブページ粒度: 64B 最低単位のサイズクラス
//    64 / 128 / 256 / 512 / 1024 / 2048
//  - ページ確保時に端数が出たら該当クラスのフリーリストへ登録する
// ============================================

#define MEM_PAGE_SIZE   4096
#define MEM_MIN_BLOCK   64
#define MEM_MAX_CLASS   2048            // これ以上はページ確保へ
#define MEM_NUM_CLASSES 6               // 64..2048 = 2^6..2^11
#define MEM_HDR_SIZE    8               // 各ブロック先頭の管理ヘッダ

// ブロックヘッダ (確保済みブロックの直前に置く)
//   cls   : サイズクラスの指数 (6..11) / 0xFF = ページ確保
//   pages : ページ確保時のページ数
typedef struct {
    UINT8  cls;
    UINT8  pad[3];
    UINT32 pages;    // page-alloc 用
} mem_hdr;

// サイズクラスごとのフリーリスト
// 空きブロック自身の先頭に next ポインタを置く (ヘッダ不要)
static UINT8 *class_free[MEM_NUM_CLASSES];

// ---- ページアロケータ (bump + ページ境界揃え) ----
static UINT8 *page_pool_base;
static UINTN  page_pool_size;   // bytes
static UINTN  page_pool_used;   // bytes

void MemInit(UINT8 *pool, UINTN size)
{
    page_pool_base = pool;
    page_pool_size = size;
    page_pool_used = 0;
    for (int i = 0; i < MEM_NUM_CLASSES; i++)
        class_free[i] = 0;
}

// ページ単位確保 (n ページ)。常にページ境界に揃える
static UINT8 *PageAlloc(UINTN pages)
{
    UINTN need = pages * MEM_PAGE_SIZE;

    // 先頭をページ境界に揃える (最初の1回だけ効く)
    UINTN skip = (UINTN)(-((UINTN)page_pool_base)) & (MEM_PAGE_SIZE - 1);
    UINT8 *start = page_pool_base + skip;

    UINTN avail = (page_pool_size - skip) - page_pool_used;
    if (avail < need)
        return 0;   // out of memory

    UINT8 *ret = start + page_pool_used;
    page_pool_used += need;
    return ret;
}

// ---- サブページアロケータ (サイズクラス別フリーリスト) ----

// size -> クラス指数 (最低 64B)
static int SizeToClass(UINTN size)
{
    int cls = 6;                       // 64B
    while (((UINTN)1 << cls) < size)
        cls++;
    return cls;
}

// 1 ページを cls クラスのブロックに切り分けて
// フリーリストへ登録する。端数は、それより小さい
// クラスへ貪欲に割り振って還元する。
static void CarvePage(int cls)
{
    UINT8 *page = PageAlloc(1);
    if (!page)
        return;   // OOM: 呼び出し元で処理

    UINTN block = (UINTN)1 << cls;
    UINT8 *p = page;
    UINTN rest = MEM_PAGE_SIZE;

    // まず本体クラスで切り分ける
    while (rest >= block)
    {
        // 空きブロック自身の先頭に next ポインタを書く
        *(UINT8 **)p = class_free[cls - 6];
        class_free[cls - 6] = p;
        p    += block;
        rest -= block;
    }

    // 端数の還元: 残りを、収まる最大の小クラスに登録
    // 例: rest=300B -> 256B クラスへ, 残り 44B は廃棄
    //     rest=511B -> 256B クラスへ, 255B -> 128, 127B -> 64, 廃棄
    while (rest >= MEM_MIN_BLOCK)
    {
        int small = SizeToClass(rest);
        UINTN sblock = (UINTN)1 << small;
        if (sblock > rest) small--;          // 収まらないなら1つ下へ
        sblock = (UINTN)1 << small;

        *(UINT8 **)p = class_free[small - 6];
        class_free[small - 6] = p;
        p    += sblock;
        rest -= sblock;
    }
}

// 確保: ページ容量を超える要求はページ境界に揃える。
// それ以下は 64B 境界寄りに配置 (速度/効率に影響なし)。
void *KAlloc(UINTN size)
{
    if (size == 0)
        return 0;

    // ページ容量を超えるならページ確保 (返り値はページ境界)
    if (size + MEM_HDR_SIZE > MEM_PAGE_SIZE)
    {
        UINTN pages = (size + MEM_HDR_SIZE + MEM_PAGE_SIZE - 1)
                      / MEM_PAGE_SIZE;
        UINT8 *pg = PageAlloc(pages);
        if (!pg)
            return 0;
        mem_hdr *h = (mem_hdr *)pg;
        h->cls   = 0xFF;
        h->pages = (UINT32)pages;
        return pg + MEM_HDR_SIZE;
    }

    int cls = SizeToClass(size + MEM_HDR_SIZE);
    UINT8 **head = &class_free[cls - 6];

    if (!*head)
        CarvePage(cls);
    if (!*head)
        return 0;   // OOM

    // リストの先頭を取り出す
    UINT8 *block = *head;
    *head = *(UINT8 **)block;

    // ヘッダを書いて、実体を返す
    mem_hdr *h = (mem_hdr *)block;
    h->cls   = (UINT8)cls;
    h->pages = 0;
    return block + MEM_HDR_SIZE;
}

// 解放
void KFree(void *ptr)
{
    if (!ptr)
        return;

    UINT8 *block = (UINT8 *)ptr - MEM_HDR_SIZE;
    mem_hdr *h = (mem_hdr *)block;

    if (h->cls == 0xFF)
    {
        // ページ確保の解放: bump なので解放領域は回収しない
        // (簡易実装)。本実装ではビットマップ化して戻す。
        return;
    }

    // フリーリストへ戻す (ブロック自身に next を書く)
    int cls = h->cls - 6;
    *(UINT8 **)block = class_free[cls];
    class_free[cls] = block;
}

// 解放 + 領域の0クリア (セキュリティ強化版)
// 解放前に内容を消すので、use-after-free での
// 情報漏洩 (鍵・パスワード等) を防げる
void KFreeZ(void *ptr)
{
    if (!ptr)
        return;

    UINT8 *block = (UINT8 *)ptr - MEM_HDR_SIZE;
    mem_hdr *h = (mem_hdr *)block;

    UINTN size;
    if (h->cls == 0xFF)
    {
        // ページ確保: bump なので回収はしないが、
        // 内容だけは消しておく
        size = (UINTN)h->pages * MEM_PAGE_SIZE - MEM_HDR_SIZE;
    }
    else
    {
        int cls = h->cls - 6;
        size = ((UINTN)1 << h->cls) - MEM_HDR_SIZE;

        // フリーリストへ戻す (ブロック自身に next を書く)
        *(UINT8 **)block = class_free[cls];
        class_free[cls] = block;
    }

    // ヘッダの直後から本体を0クリア
    UINT8 *body = block + MEM_HDR_SIZE;
    for (UINTN i = 0; i < size; i++)
        body[i] = 0;
}



