/*
 * logic.cpp - logic.hpp の実装。規則の出典は原典 spaceinvaders.py の各関数。
 * C版 logic.c と1対1に対応する。
 */
#include "logic.hpp"

void FleetLedger::init()
{
    for (int r = 0; r < FLEET_ROWS; r++)
        for (int c = 0; c < FLEET_COLS; c++)
            alive[r][c] = true;
    count = FLEET_ROWS * FLEET_COLS;
    for (int c = 0; c < FLEET_COLS; c++)
        column_alive[c] = true;
    alive_column_count = FLEET_COLS;
    left_alive_column = 0;
    right_alive_column = FLEET_COLS - 1;
    left_add_move = 0;
    right_add_move = 0;
    min_row = 0;
    max_row = 4;
    move_time = 200;
    level = 700;
}

bool FleetLedger::column_dead(int col) const
{
    if (col < 0 || col >= FLEET_COLS)
        return true; /* 原典では到達しない経路の安全側 */
    return !column_alive[col];
}

void FleetLedger::update_speed()
{
    if (count == 0)
        return;

    int minimum = 5, maximum = 0;
    for (int r = 0; r < FLEET_ROWS; r++)
        for (int c = 0; c < FLEET_COLS; c++)
            if (alive[r][c]) {
                if (r < minimum) minimum = r;
                if (r > maximum) maximum = r;
            }
    min_row = minimum;
    max_row = maximum;

    if (count == 1) {
        move_time = 2;
        level = 350;
    } else if (count == 2) {
        move_time = 40;
    } else if (count == 3) {
        move_time = 60;
    } else if (count == 4) {
        move_time = 60;
    } else if (count <= 10) {
        move_time = 40;
    } else if (count <= 15) {
        move_time = 80;
    } else if (count <= 20) {
        move_time = 90;
    } else if (count <= 30) {
        move_time = 100;
    } else if (count <= 40) {
        move_time = 110;
        level = 400;
    }
    /* 41体以上では原典も何も変えない（初期値 200 / 700 が残る） */
}

void FleetLedger::kill(int row, int col)
{
    if (row < 0 || row >= FLEET_ROWS || col < 0 || col >= FLEET_COLS)
        return;
    if (!alive[row][col])
        return;
    alive[row][col] = false;
    count--;

    /* 列が全滅したか */
    bool col_dead = true;
    for (int r = 0; r < FLEET_ROWS; r++)
        if (alive[r][col]) { col_dead = false; break; }
    if (col_dead && column_alive[col]) {
        column_alive[col] = false;
        alive_column_count--;
    }

    /* 原典 kill(): 端の列が全滅したら、生きている端までずらしながら
     * 反対側の移動回数に 5 ずつ加える。右端の判定が先（elif でない方）。 */
    if (col == right_alive_column) {
        bool dead = col_dead;
        while (right_alive_column > 0 && dead) {
            right_alive_column--;
            right_add_move += 5;
            dead = column_dead(right_alive_column);
        }
    } else if (col == left_alive_column) {
        bool dead = col_dead;
        while (left_alive_column < FLEET_COLS && dead) {
            left_alive_column++;
            left_add_move += 5;
            dead = column_dead(left_alive_column);
        }
    }

    update_speed();
}

bool FleetLedger::bottom_enemy(int pick, int &out_row, int &out_col) const
{
    if (alive_column_count == 0 || pick < 0 || pick >= alive_column_count)
        return false;
    int seen = -1;
    for (int c = 0; c < FLEET_COLS; c++) {
        if (!column_alive[c])
            continue;
        seen++;
        if (seen == pick) {
            for (int r = FLEET_ROWS - 1; r >= 0; r--) {
                if (alive[r][c]) {
                    out_row = r;
                    out_col = c;
                    return true;
                }
            }
        }
    }
    return false;
}

int fleet_counter_advance(int counter, int min_row, int max_row, bool &wrapped)
{
    counter -= 1;
    if (counter <= min_row - 1) {
        wrapped = true;
        return max_row;
    }
    wrapped = false;
    return counter;
}

int score_for_row(int row)
{
    static constexpr std::array<int, FLEET_ROWS> scores = {30, 20, 20, 10, 10};
    if (row < 0 || row >= FLEET_ROWS)
        return 0;
    return scores[row];
}

int mystery_score_for_pick(int pick)
{
    static constexpr std::array<int, 4> scores = {50, 100, 150, 300};
    if (pick < 0 || pick >= 4)
        return 0;
    return scores[pick];
}

int weapon_volley(int score, int round_num, bool bullets_empty, bool ship_alive,
                  int &out_sound)
{
    int volley = 0;
    int sound = 0;
    if (bullets_empty && ship_alive) {
        if (score <= 1990) {
            volley = VOLLEY_CENTER;
            sound = 1;
        } else if (score < 3500) {
            volley = VOLLEY_LEFT | VOLLEY_RIGHT;
            sound = 2;
        } else {
            volley = VOLLEY_CENTER | VOLLEY_LEFT | VOLLEY_RIGHT;
            sound = 2;
        }
    }
    if (round_num >= 6 && ship_alive) {
        volley |= VOLLEY_CENTER | VOLLEY_LEFT | VOLLEY_RIGHT;
        sound = 2;
    }
    out_sound = sound;
    return volley;
}

int barrier_cell_count()
{
    int n = 0;
    for (int r = 0; r < BARRIER_ROWS; r++)
        for (int c = 0; c < BARRIER_COLS; c++)
            if (BARRIER_DESIGN[r][c])
                n++;
    return n;
}
