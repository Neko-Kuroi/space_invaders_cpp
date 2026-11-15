/*
 * assets.hpp - 画像・音・フォントの読み込みと解放（C版 assets.h の C++ 化）
 *
 * C版との対応:
 *   - assets_load / assets_free の対は、コンストラクタとデストラクタに（RAII）
 *   - SDL の資源は unique_ptr と専用 deleter で持ち、解放漏れをなくす
 *   - 番号の enum は enum class に
 *   - 読み込みの失敗は例外ではなく ok() で伝える（C版の戻り値に一番近い形）。
 *     途中まで読み込んだ資源は、失敗時もデストラクタが全部解放する
 * 素材は原典リポジトリのものをそのまま使う（プロジェクト直下の
 * images/ sounds/ fonts/ を、実行ファイルの場所から相対で読む）。
 */
#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

/* 画像の番号。原典の IMAGES 辞書のキーに対応する。 */
enum class Tex {
    Ship,
    ShipExplosion1, ShipExplosion2, ShipExplosion3,
    Mystery,
    Enemy1_1, Enemy1_2, Enemy2_1, Enemy2_2,
    Enemy3_1, Enemy3_2,
    Laser, Laser1, Laser2,
    EnemyLaser, EnemyLaser1,
    ExplosionBlue, ExplosionGreen, ExplosionPurple,
    Background,
    Count
};

/* 音の番号。原典の sounds 辞書と musicNotes、Mystery の音に対応する。 */
enum class Snd {
    Shoot, Shoot2, InvaderKilled, MysteryKilled,
    ShipExplosion, MysteryEntered,
    Note0, Note1, Note2, Note3,
    Count
};

constexpr std::size_t TEX_COUNT = static_cast<std::size_t>(Tex::Count);
constexpr std::size_t SND_COUNT = static_cast<std::size_t>(Snd::Count);

/* SDL の資源用の deleter。関数ポインタを deleter にすると
 * unique_ptr のデフォルトコンストラクタが使えず std::array に
 * 入れられないため、状態を持たない小さな構造体にする。 */
struct TextureDeleter {
    void operator()(SDL_Texture *p) const { SDL_DestroyTexture(p); }
};
struct ChunkDeleter {
    void operator()(Mix_Chunk *p) const { Mix_FreeChunk(p); }
};
struct FontDeleter {
    void operator()(TTF_Font *p) const { TTF_CloseFont(p); }
};

class Assets {
public:
    /* base_path は素材フォルダのあるディレクトリ（末尾に '/' を含む想定）。
     * 失敗したら stderr に理由を出し、ok() が false になる。 */
    Assets(SDL_Renderer *renderer, const std::string &base_path);
    ~Assets() = default;

    Assets(const Assets &) = delete;
    Assets &operator=(const Assets &) = delete;

    bool ok() const { return ok_; }

    SDL_Texture *tex(Tex t) const { return tex_[index(t)].get(); }

    /* 文字をその場で描く（原典の Text.draw 相当）。size は 20/25/50 のみ。 */
    void draw_text(SDL_Renderer *renderer, int size, const std::string &msg,
                   SDL_Color color, int x, int y) const;

    /* 音量は原典どおり全て 0.1。channel を返す（-1 は失敗）。 */
    int play(Snd s) const;
    int play_on(Snd s, int channel) const;

private:
    using TexturePtr = std::unique_ptr<SDL_Texture, TextureDeleter>;
    using ChunkPtr = std::unique_ptr<Mix_Chunk, ChunkDeleter>;
    using FontPtr = std::unique_ptr<TTF_Font, FontDeleter>;

    static constexpr std::size_t index(Tex t) { return static_cast<std::size_t>(t); }
    static constexpr std::size_t index(Snd s) { return static_cast<std::size_t>(s); }

    bool ok_ = false;
    std::array<TexturePtr, TEX_COUNT> tex_;
    std::array<ChunkPtr, SND_COUNT> snd_;
    FontPtr font20_;
    FontPtr font25_;
    FontPtr font50_;
};
