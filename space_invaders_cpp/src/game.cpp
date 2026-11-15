/*
 * game.cpp - ゲームの進行と描画（C版 game.c の C++ 化）。
 * 原典 spaceinvaders.py の各メソッドの写しで、規則は C版と同一。
 * 対応関係はコメントに原典の名前を残してある。
 */
#include "game.hpp"

#include <cstdio>
#include <string>

#include <SDL_mixer.h>

namespace {

const SDL_Color COL_WHITE  = {255, 255, 255, 255};
const SDL_Color COL_GREEN  = {78, 255, 87, 255};
const SDL_Color COL_BLUE   = {80, 255, 239, 255};
const SDL_Color COL_PURPLE = {203, 0, 255, 255};
const SDL_Color COL_RED    = {237, 28, 36, 255};

/* Enemy.load_images の対応: 行ごとの2枚（index 0 が先） */
Tex enemy_texture(int row, int index)
{
    static constexpr std::array<std::array<Tex, 2>, FLEET_ROWS> table = {{
        {Tex::Enemy1_2, Tex::Enemy1_1},
        {Tex::Enemy2_2, Tex::Enemy2_1},
        {Tex::Enemy2_2, Tex::Enemy2_1},
        {Tex::Enemy3_1, Tex::Enemy3_2},
        {Tex::Enemy3_1, Tex::Enemy3_2},
    }};
    return table[row][index & 1];
}

/* EnemyExplosion.get_image の対応: 行ごとの色 */
Tex explosion_texture(int row)
{
    static constexpr std::array<Tex, FLEET_ROWS> table = {
        Tex::ExplosionPurple, Tex::ExplosionBlue, Tex::ExplosionBlue,
        Tex::ExplosionGreen, Tex::ExplosionGreen,
    };
    return table[row];
}

bool overlap(const SDL_Rect &a, const SDL_Rect &b)
{
    return SDL_HasIntersection(&a, &b) == SDL_TRUE;
}

SDL_Rect blocker_cell_rect(int base, int row, int col)
{
    return SDL_Rect{60 + 200 * base + col * 3,
                    BLOCKERS_POSITION + 25 + row * 3, 3, 3};
}

} // namespace

int Game::pbullet_count() const
{
    int n = 0;
    for (const Bullet &b : pbullets_)
        if (b.active) n++;
    return n;
}

int Game::explosion_count() const
{
    int n = 0;
    for (const Explosion &e : explosions_)
        if (e.active) n++;
    return n;
}

int Game::rand_below(int n)
{
    std::uniform_int_distribution<int> dist(0, n - 1);
    return dist(rng_);
}

void Game::spawn_explosion(ExplosionType type, SDL_Rect r, int row, int score,
                           Uint32 now)
{
    for (Explosion &ex : explosions_) {
        if (!ex.active) {
            ex.type = type;
            ex.r = r;
            ex.row = row;
            ex.score = score;
            ex.born = now;
            ex.active = true;
            return;
        }
    }
}

/* 空きスロットに弾を1発入れる。自機弾と敵弾で配列の長さが違うため
 * テンプレートで受ける（C版はポインタと長さで共通化していた箇所）。 */
template <std::size_t N>
void spawn_into(std::array<Bullet, N> &arr, bool enemy, int x, int y,
                int direction, int speed, Uint32 now)
{
    for (Bullet &b : arr) {
        if (!b.active) {
            b.r.x = x;
            b.r.y = y;
            b.r.w = 5;
            b.r.h = 15;
            b.direction = direction;
            b.speed = speed;
            b.born = now;
            b.enemy = enemy;
            b.flashing = false;
            b.active = true;
            return;
        }
    }
}

void Game::spawn_bullet(bool enemy, int x, int y, int direction, int speed,
                        Uint32 now)
{
    if (enemy)
        spawn_into(ebullets_, enemy, x, y, direction, speed, now);
    else
        spawn_into(pbullets_, enemy, x, y, direction, speed, now);
}

/* 原典 make_blockers1（4基分）＋地面。地面は原典では4基分が同じ位置に
 * 重なって生成されるため、見た目と判定が等価な1本にまとめた。 */
void Game::build_blockers()
{
    for (int b = 0; b < 4; b++)
        for (int r = 0; r < BARRIER_ROWS; r++)
            for (int c = 0; c < BARRIER_COLS; c++)
                blockers_[b][r][c] = BARRIER_DESIGN[r][c] != 0;
    ground_.fill(true);
}

/* 原典 reset(score)。残機（lives_spare_）はここでは変えない。 */
void Game::reset_round(int score, Uint32 now)
{
    score_ = score;
    ship_.r.x = 375;
    ship_.r.y = 540;
    ship_.r.w = 50;
    ship_.r.h = 50;
    ship_.vx = SHIP_VX;
    ship_alive_ = true;
    make_new_ship_ = false;
    ship_timer_ = now;

    for (int row = 0; row < FLEET_ROWS; row++) {
        for (int col = 0; col < FLEET_COLS; col++) {
            Enemy &e = enemy_at(row, col);
            e.r.x = 157 + col * 50;
            e.r.y = enemy_position_ + row * 45;
            e.r.w = 40;
            e.r.h = 35;
            e.row = row;
            e.col = col;
            e.image_index = 0;
            /* Enemy の timer = 生成時刻 + (行×-50 + 列×120)。
             * 描画条件は「now - timer > 200」のため、この時刻を過ぎるまで
             * その敵は描かれない（出現がずれる原典の癖）。 */
            e.draw_from = static_cast<Uint32>(
                static_cast<Sint32>(now) + (row * -50 + col * 120) + 200);
            e.alive = true;
        }
    }
    ledger_.init(); /* move_time=200, level=700 を含む初期化 */
    row_counter_ = 4; /* 原典の修正に合わせ、開始時に初期値へ戻す */
    fleet_direction_ = 1;
    move_number_ = 15;
    left_moves_ = 30;
    right_moves_ = 30;
    /* EnemiesGroup の timer は timeOffset = 22000 * ROUND_NUM だけ過去。
     * ラウンド2以降は「追いつきの速進」になる原典の挙動のまま。 */
    fleet_timer_ = static_cast<Uint32>(
        static_cast<Sint32>(now) - 22000 * round_num_);
    fleet_bottom_ = enemy_position_ + (FLEET_ROWS - 1) * 45 + 35;

    mystery_.r.x = -80;
    mystery_.r.y = 25;
    mystery_.r.w = 80;
    mystery_.r.h = 40;
    mystery_.direction = 1;
    mystery_.timer = now;
    mystery_.play_sound = true;
    mystery_.visible = false;
    mystery_.channel = -1;

    pbullets_ = {};
    ebullets_ = {};
    explosions_ = {};

    shoot_timer_ = now;
    note_timer_ = now;
    note_index_ = 0;
    game_over_timer_ = 0;
}

/* タイトルでキーを離したときの開始処理（原典 main の mainScreen 分岐） */
void Game::start_game(Uint32 now)
{
    build_blockers();
    lives_spare_ = 3;
    enemy_position_ = ENEMY_DEFAULT_POSITION;
    reset_round(0, now);
    state_ = GameState::Play;
}

void Game::enter_game_over(Uint32 now)
{
    state_ = GameState::GameOver;
    game_over_timer_ = now;
    round_num_ = 0;
    enemy_position_ = ENEMY_DEFAULT_POSITION;
}

/* 原典 check_input の発射部分。武器の規則は logic の weapon_volley。 */
void Game::fire(Uint32 now)
{
    int sound = 0;
    int volley = weapon_volley(score_, round_num_, pbullet_count() == 0,
                               ship_alive_, sound);
    if (volley & VOLLEY_CENTER)
        spawn_bullet(false, ship_.r.x + 23, ship_.r.y + 5, -1, 15, now);
    if (volley & VOLLEY_LEFT)
        spawn_bullet(false, ship_.r.x + 8, ship_.r.y + 5, -1, 15, now);
    if (volley & VOLLEY_RIGHT)
        spawn_bullet(false, ship_.r.x + 38, ship_.r.y + 5, -1, 15, now);
    /* 原典は第1ブロックと第2ブロックで別々に音を鳴らす。ここでは
     * weapon_volley が返した最後の音だけを鳴らす（聞き分けできない差）。 */
    if (sound == 1)
        assets_.play(Snd::Shoot);
    else if (sound == 2)
        assets_.play(Snd::Shoot2);
}

/* 原典 EnemiesGroup.update */
void Game::fleet_update(Uint32 now)
{
    const FleetLedger &l = ledger_;
    if (static_cast<Sint32>(now - fleet_timer_) <= l.move_time)
        return;

    int max_move = (fleet_direction_ == 1)
                       ? right_moves_ + l.right_add_move
                       : left_moves_ + l.left_add_move;
    if (move_number_ >= max_move) {
        left_moves_ = 30 + l.right_add_move;
        right_moves_ = 30 + l.left_add_move;
        fleet_direction_ *= -1;
        move_number_ = 0;
        fleet_bottom_ = 0;
        for (Enemy &e : enemies_) {
            if (!e.alive) continue;
            e.r.y += ENEMY_MOVE_DOWN;
            e.image_index ^= 1;
            if (fleet_bottom_ < e.r.y + 35)
                fleet_bottom_ = e.r.y + 35;
        }
    } else {
        int velocity = (fleet_direction_ == 1) ? 10 : -10;
        if (l.count <= 4) {
            for (Enemy &e : enemies_) {
                if (!e.alive) continue;
                e.r.x += velocity;
                e.image_index ^= 1;
            }
            move_number_ += 1;
        } else {
            /* ROW_counter の行だけが動く（波状の動き） */
            for (Enemy &e : enemies_) {
                if (!e.alive) continue;
                if (e.row == row_counter_) {
                    e.r.x += velocity;
                    e.image_index ^= 1;
                }
            }
            bool wrapped = false;
            row_counter_ = fleet_counter_advance(row_counter_,
                                                 l.min_row, l.max_row,
                                                 wrapped);
            if (wrapped)
                move_number_ += 1;
        }
    }
    fleet_timer_ += static_cast<Uint32>(l.move_time);
}

/* 原典 Ship.update。加速と「差の1/3ずつ寄る」式をそのまま写した。
 * 左は Python の負数の床割り算のため切り上げ、右は切り捨てになる。 */
void Game::ship_update(const Input &in)
{
    Ship &s = ship_;
    if (in.held_a && s.r.x > 10) {
        s.vx += 1;
        s.r.x -= (s.vx + 2) / 3;
        if (s.r.x < 10) s.r.x = 10;
    }
    if (in.held_d && s.r.x < 740) {
        s.vx += 1;
        s.r.x += s.vx / 3;
        if (s.r.x > 740) s.r.x = 740;
    }
}

/* 原典 Bullet.update（自機弾・敵弾共通の式）。
 * 配列の長さが違うため実体はテンプレートに分ける。 */
template <std::size_t N>
void bullets_update_impl(std::array<Bullet, N> &arr, Uint32 now)
{
    for (Bullet &b : arr) {
        if (!b.active) continue;
        b.r.y += b.speed * b.direction;
        if (b.r.y < 35) {
            b.r.y = 35;
            b.flashing = true;
            if (now > b.born + 700)
                b.active = false;
        }
        if (b.r.y > 585) {
            b.r.y = 580;
            b.flashing = true;
            if (now > b.born + 160)
                b.active = false;
        }
    }
}

void Game::bullets_update(Uint32 now)
{
    bullets_update_impl(pbullets_, now);
    bullets_update_impl(ebullets_, now);
}

/* 原典 Mystery.update。描画は「動いたフレームだけ」のため visible で返す。 */
void Game::mystery_update(Uint32 now)
{
    Mystery &m = mystery_;
    m.visible = false;
    Uint32 passed = now - m.timer;
    bool reset_timer = false;
    if (passed > 25000) {
        if ((m.r.x < 0 || m.r.x > 800) && m.play_sound) {
            /* 専用チャンネル（予約済み）で鳴らす。以後のフェードアウトや
             * 停止はこの音にだけかかる */
            m.channel = assets_.play_on(Snd::MysteryEntered, MYSTERY_CHANNEL);
            m.play_sound = false;
        }
        if (m.r.x < 840 && m.direction == 1) {
            if (m.channel >= 0)
                Mix_FadeOutChannel(m.channel, 4000);
            m.r.x += 2;
            m.visible = true;
        } else if (m.r.x > -90 && m.direction == -1) {
            if (m.channel >= 0)
                Mix_FadeOutChannel(m.channel, 4000);
            m.r.x -= 2;
            m.visible = true;
        }
    }
    if (m.r.x > 830) {
        m.play_sound = true;
        m.direction = -1;
        reset_timer = true;
    }
    if (m.r.x < -90) {
        m.play_sound = true;
        m.direction = 1;
        reset_timer = true;
    }
    if (passed > 25000 && reset_timer)
        m.timer = now;
}

/* 原典 play_main_music */
void Game::play_music(Uint32 now)
{
    static constexpr std::array<Snd, 4> NOTES = {
        Snd::Note0, Snd::Note1, Snd::Note2, Snd::Note3};
    Uint32 interval = static_cast<Uint32>(ledger_.move_time * 3);
    if (now - note_timer_ > interval) {
        assets_.play(NOTES[note_index_]);
        note_index_ = (note_index_ < 3) ? note_index_ + 1 : 0;
        note_timer_ += interval;
    }
}

/* 原典 make_enemies_shoot */
void Game::make_enemies_shoot(Uint32 now)
{
    if (static_cast<Sint32>(now - shoot_timer_) > ledger_.level &&
        ledger_.count > 0) {
        int row, col;
        int pick = rand_below(ledger_.alive_column_count);
        if (ledger_.bottom_enemy(pick, row, col)) {
            Enemy &e = enemy_at(row, col);
            spawn_bullet(true, e.r.x + 14, e.r.y + 20, 1, 5, now);
            shoot_timer_ = now;
        }
    }
}

/* 原典 create_new_ship */
void Game::create_new_ship(Uint32 now)
{
    if (make_new_ship_ && static_cast<Sint32>(now - ship_timer_) > 1600) {
        ship_.r.x = 375;
        ship_.r.y = 540;
        ship_.r.w = 50;
        ship_.r.h = 50;
        ship_.vx = SHIP_VX;
        make_new_ship_ = false;
        ship_alive_ = true;
    }
}

/* ---- 原典 check_collisions（処理の順番も原典どおり） ---- */

void Game::kill_enemy_no_score(Enemy &e)
{
    e.alive = false;
    ledger_.kill(e.row, e.col);
    if (ledger_.count == 0)
        fleet_bottom_ = 0; /* 後段の判定が空の集団で誤作動しないように */
}

/* 弾とトーチカ／地面の衝突（原典の groupcollide 相当）。
 * 原典は重なったセルを全部消すので、自機弾・敵弾とも同じ規則で
 * 全セルを消す。敵弾のときは原典どおり bulletExplosion を置く。 */
void Game::bullets_vs_blockers(bool enemy, Uint32 now)
{
    if (enemy)
        bullets_vs_blockers_impl(ebullets_, enemy, now);
    else
        bullets_vs_blockers_impl(pbullets_, enemy, now);
}

template <std::size_t N>
void Game::bullets_vs_blockers_impl(std::array<Bullet, N> &arr, bool enemy,
                                    Uint32 now)
{
    for (Bullet &b : arr) {
        if (!b.active) continue;
        bool hit = false;
        for (int base = 0; base < 4; base++)
            for (int r = 0; r < BARRIER_ROWS; r++)
                for (int c = 0; c < BARRIER_COLS; c++) {
                    if (!blockers_[base][r][c]) continue;
                    SDL_Rect cell = blocker_cell_rect(base, r, c);
                    if (overlap(b.r, cell)) {
                        blockers_[base][r][c] = false;
                        hit = true;
                    }
                }
        for (int gi = 0; gi < 200; gi++) {
            if (!ground_[gi]) continue;
            SDL_Rect cell = {gi * 4, 595, 4, 4};
            if (overlap(b.r, cell)) {
                ground_[gi] = false;
                hit = true;
            }
        }
        if (hit) {
            b.active = false;
            if (enemy)
                spawn_explosion(ExplosionType::Bullet, b.r, 0, 0, now);
        }
    }
}

void Game::check_collisions(Uint32 now)
{
    /* (a) 自機弾 × 敵弾: 両方消える */
    for (Bullet &b : pbullets_) {
        if (!b.active) continue;
        for (Bullet &eb : ebullets_) {
            if (eb.active && overlap(b.r, eb.r)) {
                eb.active = false;
                b.active = false;
                break;
            }
        }
    }

    /* (b) 敵 × 自機弾: pygame の groupcollide は行優先の順に処理し、
     * 弾は最初に当たった敵で消える。その順番で写す。 */
    for (Enemy &e : enemies_) {
        if (!e.alive) continue;
        for (Bullet &b : pbullets_) {
            if (b.active && overlap(e.r, b.r)) {
                b.active = false;
                assets_.play(Snd::InvaderKilled);
                score_ += score_for_row(e.row);
                spawn_explosion(ExplosionType::Enemy, e.r, e.row, 0, now);
                game_timer_ = now;
                e.alive = false;
                ledger_.kill(e.row, e.col);
                break;
            }
        }
    }

    /* (c) ミステリーシップ × 自機弾 */
    for (Bullet &b : pbullets_) {
        if (b.active && overlap(mystery_.r, b.r)) {
            b.active = false;
            if (mystery_.channel >= 0)
                Mix_HaltChannel(mystery_.channel);
            assets_.play(Snd::MysteryKilled);
            int gained = mystery_score_for_pick(rand_below(4));
            score_ += gained;
            spawn_explosion(ExplosionType::MysteryScore, mystery_.r, 5,
                            gained, now);
            /* 原典は新しい Mystery を作り直す（次の出現は25秒後） */
            mystery_.r.x = -80;
            mystery_.r.y = 25;
            mystery_.direction = 1;
            mystery_.timer = now;
            mystery_.play_sound = true;
            mystery_.visible = false;
            mystery_.channel = -1;
            break;
        }
    }

    /* (d) 自機 × 敵弾 */
    if (ship_alive_) {
        for (Bullet &eb : ebullets_) {
            if (eb.active && overlap(ship_.r, eb.r)) {
                eb.active = false;
                assets_.play(Snd::ShipExplosion);
                spawn_explosion(ExplosionType::Ship, ship_.r, 0, 0, now);
                if (lives_spare_ > 0) {
                    lives_spare_--;
                } else {
                    enter_game_over(now);
                }
                make_new_ship_ = true;
                ship_timer_ = now;
                ship_alive_ = false;
                break;
            }
        }
    }

    /* (e) 敵集団が自機の高さまで来たとき */
    if (state_ == GameState::Play && fleet_bottom_ >= 540) {
        if (ship_alive_) {
            for (Enemy &e : enemies_) {
                if (e.alive && overlap(e.r, ship_.r)) {
                    kill_enemy_no_score(e);
                    ship_alive_ = false; /* groupcollide で自機も消える */
                }
            }
        }
        if (!ship_alive_ || fleet_bottom_ >= 600)
            enter_game_over(now);
    }

    /* (f) 自機弾 × トーチカ・地面、(g) 敵弾 × トーチカ・地面。
     * 原典の groupcollide はどちらも重なった全セルを消すので共通化した */
    bullets_vs_blockers(false, now);
    bullets_vs_blockers(true, now);

    /* (h) 敵集団がトーチカの高さまで来たら、重なったセルが消える */
    if (fleet_bottom_ >= BLOCKERS_POSITION) {
        for (Enemy &e : enemies_) {
            if (!e.alive) continue;
            for (int base = 0; base < 4; base++)
                for (int r = 0; r < BARRIER_ROWS; r++)
                    for (int c = 0; c < BARRIER_COLS; c++) {
                        if (!blockers_[base][r][c]) continue;
                        SDL_Rect cell = blocker_cell_rect(base, r, c);
                        if (overlap(e.r, cell))
                            blockers_[base][r][c] = false;
                    }
        }
    }
}

/* 爆発の寿命の管理（原典の各 Explosion.update の消滅条件）。
 * 描画側では状態を書き換えないよう、消滅判定はここ（更新側）に置く。
 * ラウンド遷移の条件（explosion_count == 0）も更新側だけで決まる。 */
void Game::explosions_update(Uint32 now)
{
    for (Explosion &ex : explosions_) {
        if (!ex.active) continue;
        Uint32 passed = now - ex.born;
        switch (ex.type) {
        case ExplosionType::Enemy:        if (passed > 400) ex.active = false; break;
        case ExplosionType::Ship:         if (passed > 800) ex.active = false; break;
        case ExplosionType::Bullet:       if (passed > 300) ex.active = false; break;
        case ExplosionType::MysteryScore: if (passed > 600) ex.active = false; break;
        }
    }
}

/* ---- 状態ごとの更新 ---- */

Game::Game(Assets &assets, unsigned seed) : assets_(assets), rng_(seed)
{
    /* C版 game_init の内容は、メンバの初期値（game.hpp）とこの2つ */
    ledger_.init();
    build_blockers();
}

bool Game::update(const Input &in, Uint32 now)
{
    if (in.quit)
        return false;

    switch (state_) {
    case GameState::Title:
        if (in.any_keyup)
            start_game(now);
        break;

    case GameState::GameOver:
        if (static_cast<Sint32>(now - game_over_timer_) > 4000)
            state_ = GameState::Title;
        break;

    case GameState::Play:
        if (ledger_.count == 0 && explosion_count() == 0) {
            /* ラウンド間の画面（原典 main の startGame 分岐の前半） */
            if (in.keyup_move)
                ship_.vx = SHIP_VX;
            if (in.fire_keydown)
                fire(now);
            if (static_cast<Sint32>(now - game_timer_) > 3000) {
                round_num_ += 1;
                build_blockers();
                enemy_position_ = ENEMY_DEFAULT_POSITION;
                reset_round(score_, now);
                game_timer_ += 3000;
            }
            break;
        }
        play_music(now);
        if (in.keyup_move)
            ship_.vx = SHIP_VX;
        if (in.fire_keydown)
            fire(now);
        fleet_update(now);
        if (ship_alive_)
            ship_update(in);
        bullets_update(now);
        mystery_update(now);
        explosions_update(now);
        check_collisions(now);
        if (state_ == GameState::Play) {
            create_new_ship(now);
            make_enemies_shoot(now);
        }
        break;
    }
    return true;
}

/* ---- 描画 ---- */

void Game::draw_tex(SDL_Renderer *ren, Tex tex, int x, int y, int w, int h)
{
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(ren, assets_.tex(tex), nullptr, &dst);
}

void Game::draw_blockers(SDL_Renderer *ren)
{
    SDL_SetRenderDrawColor(ren, COL_RED.r, COL_RED.g, COL_RED.b, 255);
    for (int base = 0; base < 4; base++)
        for (int r = 0; r < BARRIER_ROWS; r++)
            for (int c = 0; c < BARRIER_COLS; c++) {
                if (!blockers_[base][r][c]) continue;
                SDL_Rect cell = blocker_cell_rect(base, r, c);
                SDL_RenderFillRect(ren, &cell);
            }
    for (int gi = 0; gi < 200; gi++) {
        if (!ground_[gi]) continue;
        SDL_Rect cell = {gi * 4, 595, 4, 4};
        SDL_RenderFillRect(ren, &cell);
    }
}

void Game::draw_hud(SDL_Renderer *ren)
{
    assets_.draw_text(ren, 20, "Score", COL_WHITE, 5, 5);
    assets_.draw_text(ren, 20, std::to_string(score_), COL_GREEN, 85, 5);
    assets_.draw_text(ren, 20, "Lives", COL_WHITE, 640, 5);
    for (int i = 0; i < lives_spare_ && i < 3; i++)
        draw_tex(ren, Tex::Ship, 715 + 27 * i, 3, 23, 23);
}

void Game::draw_explosions(SDL_Renderer *ren, Uint32 now)
{
    static constexpr std::array<Tex, 8> ship_steps = {
        Tex::ShipExplosion1, Tex::ShipExplosion2, Tex::ShipExplosion3,
        Tex::ShipExplosion2, Tex::ShipExplosion1, Tex::ShipExplosion3,
        Tex::ShipExplosion2, Tex::ShipExplosion3,
    };
    for (const Explosion &ex : explosions_) {
        if (!ex.active) continue;
        Uint32 passed = now - ex.born;
        /* 描画は読むだけ。消滅の判定は explosions_update（更新側）にある */
        switch (ex.type) {
        case ExplosionType::Enemy:
            if (passed <= 100)
                draw_tex(ren, explosion_texture(ex.row), ex.r.x, ex.r.y, 40, 35);
            else if (passed <= 200)
                draw_tex(ren, explosion_texture(ex.row), ex.r.x - 6, ex.r.y - 6, 50, 45);
            break;
        case ExplosionType::Ship: {
            if (passed > 800)
                break;
            int step = static_cast<int>(passed / 100);
            if (step > 7) step = 7;
            draw_tex(ren, ship_steps[step], ex.r.x, ex.r.y, 50, 48);
            break;
        }
        case ExplosionType::Bullet:
            if (passed <= 300)
                draw_tex(ren, Tex::EnemyLaser1, ex.r.x, ex.r.y, 5, 15);
            break;
        case ExplosionType::MysteryScore:
            if (passed <= 200 || (passed > 400 && passed <= 600)) {
                assets_.draw_text(ren, 20, std::to_string(ex.score), COL_WHITE,
                                  ex.r.x + 20, ex.r.y + 6);
            }
            break;
        }
    }
}

void Game::draw_play_scene(SDL_Renderer *ren, Uint32 now)
{
    draw_tex(ren, Tex::Background, 0, 0, SCREEN_W, SCREEN_H);
    draw_blockers(ren);
    draw_hud(ren);

    if (ledger_.count == 0 && explosion_count() == 0 &&
        state_ == GameState::Play) {
        /* ラウンド間の画面 */
        assets_.draw_text(ren, 50, "Next Round", COL_WHITE, 240, 270);
        assets_.draw_text(ren, 50, std::to_string(round_num_ + 2), COL_WHITE,
                          390, 350);
        return;
    }

    assets_.draw_text(ren, 20, "Round " + std::to_string(round_num_ + 1),
                      COL_WHITE, 325, 5);

    if (ship_alive_)
        draw_tex(ren, Tex::Ship, ship_.r.x, ship_.r.y, 50, 50);

    for (const Enemy &e : enemies_) {
        if (!e.alive) continue;
        if (static_cast<Sint32>(now - e.draw_from) <= 0) continue;
        draw_tex(ren, enemy_texture(e.row, e.image_index),
                 e.r.x, e.r.y, 40, 35);
    }

    if (mystery_.visible)
        draw_tex(ren, Tex::Mystery, mystery_.r.x, mystery_.r.y, 80, 40);

    for (const Bullet &b : pbullets_) {
        if (b.active)
            draw_tex(ren, b.flashing ? Tex::Laser1 : Tex::Laser,
                     b.r.x, b.r.y, 5, 15);
    }
    for (const Bullet &b : ebullets_) {
        if (b.active)
            draw_tex(ren, b.flashing ? Tex::EnemyLaser1 : Tex::EnemyLaser,
                     b.r.x, b.r.y, 5, 15);
    }

    draw_explosions(ren, now);
}

void Game::render(SDL_Renderer *renderer)
{
    Uint32 now = SDL_GetTicks();
    SDL_RenderClear(renderer);
    switch (state_) {
    case GameState::Title:
        draw_tex(renderer, Tex::Background, 0, 0, SCREEN_W, SCREEN_H);
        assets_.draw_text(renderer, 50, "Space Invaders", COL_WHITE, 164, 155);
        assets_.draw_text(renderer, 25, "Press any key to continue", COL_WHITE, 201, 225);
        draw_tex(renderer, Tex::Enemy3_1, 318, 270, 40, 40);
        draw_tex(renderer, Tex::Enemy2_2, 318, 320, 40, 40);
        draw_tex(renderer, Tex::Enemy1_2, 318, 370, 40, 40);
        draw_tex(renderer, Tex::Mystery, 299, 420, 80, 40);
        assets_.draw_text(renderer, 25, "   =   10 pts", COL_GREEN, 368, 270);
        assets_.draw_text(renderer, 25, "   =   20 pts", COL_BLUE, 368, 320);
        assets_.draw_text(renderer, 25, "   =   30 pts", COL_PURPLE, 368, 370);
        assets_.draw_text(renderer, 25, "   =   ?????", COL_RED, 368, 420);
        break;
    case GameState::Play:
        draw_play_scene(renderer, now);
        break;
    case GameState::GameOver: {
        draw_play_scene(renderer, now);
        Uint32 passed = now - game_over_timer_;
        const char *msg = nullptr;
        if (passed > 900 && passed < 1000) msg = "G";
        else if (passed > 1000 && passed < 1100) msg = "Ga";
        else if (passed > 1100 && passed < 1200) msg = "Gam";
        else if (passed > 1200 && passed < 1300) msg = "Game";
        else if (passed > 1300 && passed < 1400) msg = "Game O";
        else if (passed > 1500 && passed < 1600) msg = "Game Ov";
        else if (passed > 1600 && passed < 1700) msg = "Game Ove";
        else if (passed > 1700 && passed < 1800) msg = "Game Over";
        if (msg)
            assets_.draw_text(renderer, 50, msg, COL_RED, 250, 270);
        break;
    }
    }
    SDL_RenderPresent(renderer);
}
