/*
 * logic.hpp - 画面に依存しない純ロジック（C版 logic.h の C++ 化）
 *
 * C版との対応:
 *   - #define の定数は constexpr に
 *   - FleetLedger の配列は std::array に
 *   - ledger_xxx(FleetLedger*, ...) の関数は FleetLedger のメンバ関数に
 *   - 出力用のポインタ引数は参照に
 * 規則の中身（出典は原典 spaceinvaders.py）は C版と同一。
 */
#pragma once

#include <array>

constexpr int FLEET_ROWS = 5;
constexpr int FLEET_COLS = 10;

struct FleetLedger {
    std::array<std::array<bool, FLEET_COLS>, FLEET_ROWS> alive{};
    int count = 0;                   /* 生きている敵の総数 */
    std::array<bool, FLEET_COLS> column_alive{};
    int alive_column_count = 0;
    int left_alive_column = 0;       /* 原典 _leftAliveColumn */
    int right_alive_column = 0;      /* 原典 _rightAliveColumn */
    int left_add_move = 0;           /* 原典 leftAddMove */
    int right_add_move = 0;          /* 原典 rightAddMove */
    int min_row = 0;                 /* 原典 minRow */
    int max_row = 0;                 /* 原典 maxRow */
    int move_time = 200;             /* 敵集団の移動間隔 ms（原典 moveTime） */
    int level = 700;                 /* 敵の発射間隔 ms（原典の全域 LEVEL） */

    /* 原典の EnemiesGroup 生成直後の状態にする。move_time=200, level=700 は
     * 原典の初期値（Enemy の生成で LEVEL=700 に戻される癖を含む）。 */
    void init();

    /* 敵を1体倒したときの台帳更新。原典 EnemiesGroup.kill() +
     * remove_internal 経由の update_speed() を合わせたもの。
     * 既に死んでいるセルでも安全。 */
    void kill(int row, int col);

    bool column_dead(int col) const;

    /* 原典 random_bottom() の選択部分。
     * pick は [0, alive_column_count) の乱数。生きている列を小さい順に
     * 数えて pick 番目の列を選び、その列の一番下（最大の行）の敵を返す。
     * 敵が1体もいなければ false。 */
    bool bottom_enemy(int pick, int &out_row, int &out_col) const;

private:
    /* 原典 update_speed()。残数で move_time と level が変わる表と、
     * minRow / maxRow の再計算。0体では早期 return（修正済み原典の形）。 */
    void update_speed();
};

/* 得点表。row は 0..4（ミステリーシップは mystery_score_for_pick で別扱い）。 */
int score_for_row(int row);

/* ミステリーシップの得点 {50, 100, 150, 300}。pick は [0,4) の乱数。 */
int mystery_score_for_pick(int pick);

/* 発射の規則（原典 check_input の分岐そのまま）。
 * 戻り値は撃つ弾のビットマスク（VOLLEY_CENTER/LEFT/RIGHT）。
 * out_sound は鳴らす音: 0=なし, 1=shoot, 2=shoot2。
 * 原典の癖: ラウンド6以上では、弾が残っていても3発が追加で撃たれる。 */
constexpr int VOLLEY_CENTER = 1;
constexpr int VOLLEY_LEFT   = 2;
constexpr int VOLLEY_RIGHT  = 4;
int weapon_volley(int score, int round_num, bool bullets_empty, bool ship_alive,
                  int &out_sound);

/* 敵集団の波状移動で ROW_counter を1つ進める規則。
 * counter を -1 し、min_row-1 以下になったら max_row に包み直して
 * wrapped を真にする（moveNumber の加算は呼び出し側の仕事）。
 * 包み直しの条件は、原典の修正（一致判定 == から <= へ）を反映したもの。 */
int fleet_counter_advance(int counter, int min_row, int max_row, bool &wrapped);

/* トーチカの設計表（原典 make_blockers1 の barrierDesign）。
 * 16行×26列、先頭行は空。1 のセルに 3x3 のブロックが置かれる。
 * 1基あたり318セル（原典で実測）。 */
constexpr int BARRIER_ROWS = 16;
constexpr int BARRIER_COLS = 26;
inline constexpr std::array<std::array<unsigned char, BARRIER_COLS>, BARRIER_ROWS>
BARRIER_DESIGN = {{
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1},
    {1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1},
    {1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1},
}};

/* 1基あたりのブロック数（原典では4基で 1272 個、地面 800 個と合わせて 2072）。 */
int barrier_cell_count();
