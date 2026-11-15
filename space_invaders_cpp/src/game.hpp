/*
 * game.hpp - ゲームの状態と進行（C版 game.h の C++ 化）
 *
 * C版との対応:
 *   - Game 構造体＋game_xxx(Game*, ...) は Game クラスのメンバ関数に
 *   - game_init はコンストラクタに。Assets は参照で持つ
 *   - #define の定数は constexpr、状態と爆発の enum は enum class に
 *   - 固定長配列は std::array に
 *   - rand() は std::mt19937（Game が保持）に
 * 中身の規則（出典は原典 spaceinvaders.py）は C版と同一。
 * 入力は Input 構造体に分離してあるので、main.cpp は SDL イベントから
 * Input を作るだけになり、スモークテストでは入力を合成できる。
 */
#pragma once

#include <array>
#include <random>

#include <SDL.h>

#include "assets.hpp"
#include "logic.hpp"

constexpr int SCREEN_W = 800;
constexpr int SCREEN_H = 600;
constexpr int SHIP_VX = 10;
constexpr int ENEMY_DEFAULT_POSITION = 65;
constexpr int ENEMY_MOVE_DOWN = 35;
constexpr int BLOCKERS_POSITION = 450;

/* ミステリーシップ出現音の専用チャンネル。main.cpp が起動時に
 * Mix_ReserveChannels(1) で予約する。通常の効果音（-1 指定）は
 * このチャンネルを使わないので、後からのフェードアウトや停止が
 * 別の音にかかることはない。 */
constexpr int MYSTERY_CHANNEL = 0;

constexpr int MAX_PBULLETS = 24;
constexpr int MAX_EBULLETS = 64;
constexpr int MAX_EXPLOSIONS = 80;

enum class GameState { Title, Play, GameOver };

/* 1フレーム分の入力。main.cpp が SDL イベントとキー状態から作る。 */
struct Input {
    bool quit = false;         /* ウィンドウを閉じる / ESC の KEYUP（原典 should_exit） */
    bool any_keyup = false;    /* タイトルで何かキーを離した（原典の開始条件） */
    bool fire_keydown = false; /* S の KEYDOWN（発射はイベント駆動） */
    bool keyup_move = false;   /* A か D の KEYUP（vx を SHIP_VX に戻す） */
    bool held_a = false;
    bool held_d = false;
};

struct Ship {
    SDL_Rect r{375, 540, 50, 50}; /* 50x50, 初期 (375, 540) */
    int vx = SHIP_VX;
};

struct Bullet {
    SDL_Rect r{0, 0, 5, 15}; /* 弾の画像は 5x15 */
    int direction = 0;       /* -1 上 / +1 下 */
    int speed = 0;
    Uint32 born = 0;
    bool active = false;
    bool enemy = false;      /* 敵弾か */
    bool flashing = false;   /* 端で image2 を出している間 */
};

struct Enemy {
    SDL_Rect r{0, 0, 40, 35}; /* 40x35 */
    int row = 0, col = 0;
    int image_index = 0;      /* アニメーション 0/1 */
    Uint32 draw_from = 0;     /* この時刻までは描かない（原典の timeOffset の癖） */
    bool alive = false;
};

enum class ExplosionType { Enemy, Ship, Bullet, MysteryScore };

struct Explosion {
    ExplosionType type = ExplosionType::Enemy;
    SDL_Rect r{0, 0, 0, 0};
    int row = 0;              /* Enemy: 色の決定に使う */
    int score = 0;            /* MysteryScore: 表示する得点 */
    Uint32 born = 0;
    bool active = false;
};

struct Mystery {
    SDL_Rect r{-80, 25, 80, 40}; /* 80x40 */
    int direction = 1;
    Uint32 timer = 0;
    bool play_sound = true;
    bool visible = false;     /* このフレームで動いた＝描く（原典の blit 条件） */
    int channel = -1;         /* mysteryentered の再生チャンネル */
};

class Game {
public:
    /* 原典の game_init。seed は乱数の種（C版の srand 相当）。 */
    Game(Assets &assets, unsigned seed);

    /* 1フレーム分の更新。now は SDL_GetTicks()。戻り値 false で終了。 */
    bool update(const Input &in, Uint32 now);
    /* 現在の状態を描く。 */
    void render(SDL_Renderer *renderer);

    /* スモークテスト表示用の読み取り */
    GameState state() const { return state_; }
    int score() const { return score_; }
    int enemies_alive() const { return ledger_.count; }
    int round_num() const { return round_num_; }
    int lives_spare() const { return lives_spare_; }

private:
    void reset_round(int score, Uint32 now);
    void start_game(Uint32 now);
    void enter_game_over(Uint32 now);
    void build_blockers();

    void fire(Uint32 now);
    void fleet_update(Uint32 now);
    void ship_update(const Input &in);
    void bullets_update(Uint32 now);
    void mystery_update(Uint32 now);
    void play_music(Uint32 now);
    void make_enemies_shoot(Uint32 now);
    void create_new_ship(Uint32 now);
    void check_collisions(Uint32 now);
    void bullets_vs_blockers(bool enemy, Uint32 now);
    /* 自機弾と敵弾で配列の長さが違うため、実体は大きさのテンプレートに分ける
     * （C版はポインタと長さで共通化していた箇所） */
    template <std::size_t N>
    void bullets_vs_blockers_impl(std::array<Bullet, N> &arr, bool enemy,
                                  Uint32 now);
    void explosions_update(Uint32 now);
    void kill_enemy_no_score(Enemy &e);
    void spawn_explosion(ExplosionType type, SDL_Rect r, int row, int score,
                         Uint32 now);
    void spawn_bullet(bool enemy, int x, int y, int direction, int speed,
                      Uint32 now);

    Enemy &enemy_at(int row, int col) { return enemies_[row * FLEET_COLS + col]; }
    int pbullet_count() const;
    int explosion_count() const;
    int rand_below(int n); /* [0, n) の一様な乱数（C版の rand() % n 相当） */

    void draw_tex(SDL_Renderer *ren, Tex tex, int x, int y, int w, int h);
    void draw_blockers(SDL_Renderer *ren);
    void draw_hud(SDL_Renderer *ren);
    void draw_explosions(SDL_Renderer *ren, Uint32 now);
    void draw_play_scene(SDL_Renderer *ren, Uint32 now);

    Assets &assets_;
    std::mt19937 rng_;

    GameState state_ = GameState::Title;

    int score_ = 0;
    int round_num_ = 0;       /* 原典の全域 ROUND_NUM */
    int enemy_position_ = ENEMY_DEFAULT_POSITION;
    int lives_spare_ = 3;     /* 残機アイコンの数（初期3、被弾で減る） */

    Ship ship_;
    bool ship_alive_ = true;
    bool make_new_ship_ = false;
    Uint32 ship_timer_ = 0;

    std::array<Enemy, FLEET_ROWS * FLEET_COLS> enemies_{};
    FleetLedger ledger_;
    int fleet_direction_ = 1;
    int move_number_ = 15;    /* 初期値 15（原典の癖） */
    int left_moves_ = 30, right_moves_ = 30;
    int row_counter_ = 4;     /* 原典の全域 ROW_counter、初期値 4 */
    Uint32 fleet_timer_ = 0;
    int fleet_bottom_ = 0;

    std::array<Bullet, MAX_PBULLETS> pbullets_{};
    std::array<Bullet, MAX_EBULLETS> ebullets_{};
    Mystery mystery_;
    std::array<Explosion, MAX_EXPLOSIONS> explosions_{};

    std::array<std::array<std::array<bool, BARRIER_COLS>, BARRIER_ROWS>, 4>
        blockers_{};
    std::array<bool, 200> ground_{}; /* y=595 の地面（1セル 4px） */

    Uint32 shoot_timer_ = 0;  /* 原典 self.timer（敵の発射間隔の基準） */
    Uint32 note_timer_ = 0;
    int note_index_ = 0;
    Uint32 game_timer_ = 0;   /* 原典 gameTimer（最後の撃墜時刻、遷移の基準） */
    Uint32 game_over_timer_ = 0; /* 原典 gameOverTimer（F-5 の専用化を踏襲） */
};
