/*
 * assets.cpp - C版 assets.c の C++ 化。読み込み手順と失敗時の表示は C版と同じ。
 */
#include "assets.hpp"

#include <cstdio>

#include <SDL_image.h>

namespace {

const std::array<const char *, TEX_COUNT> TEX_FILES = {{
    "images/ship.png",
    "images/shipexplosion1.png",
    "images/shipexplosion2.png",
    "images/shipexplosion3.png",
    "images/mystery.png",
    "images/enemy1_1.png",
    "images/enemy1_2.png",
    "images/enemy2_1.png",
    "images/enemy2_2.png",
    "images/enemy3_1.png",
    "images/enemy3_2.png",
    "images/laser.png",
    "images/laser1.png",
    "images/laser2.png",
    "images/enemylaser.png",
    "images/enemylaser1.png",
    "images/explosionblue.png",
    "images/explosiongreen.png",
    "images/explosionpurple.png",
    "images/background.jpg",
}};

const std::array<const char *, SND_COUNT> SND_FILES = {{
    "sounds/shoot.wav",
    "sounds/shoot2.wav",
    "sounds/invaderkilled.wav",
    "sounds/mysterykilled.wav",
    "sounds/shipexplosion.wav",
    "sounds/mysteryentered.wav",
    "sounds/0.wav",
    "sounds/1.wav",
    "sounds/2.wav",
    "sounds/3.wav",
}};

} // namespace

Assets::Assets(SDL_Renderer *renderer, const std::string &base_path)
{
    for (std::size_t i = 0; i < TEX_COUNT; i++) {
        std::string path = base_path + TEX_FILES[i];
        SDL_Surface *surf = IMG_Load(path.c_str());
        if (!surf) {
            std::fprintf(stderr, "image load failed: %s: %s\n",
                         path.c_str(), IMG_GetError());
            return;
        }
        tex_[i] = TexturePtr(SDL_CreateTextureFromSurface(renderer, surf));
        SDL_FreeSurface(surf);
        if (!tex_[i]) {
            std::fprintf(stderr, "texture create failed: %s\n", path.c_str());
            return;
        }
    }
    for (std::size_t i = 0; i < SND_COUNT; i++) {
        std::string path = base_path + SND_FILES[i];
        snd_[i] = ChunkPtr(Mix_LoadWAV(path.c_str()));
        if (!snd_[i]) {
            std::fprintf(stderr, "sound load failed: %s: %s\n",
                         path.c_str(), Mix_GetError());
            return;
        }
        /* 原典は全ての音を set_volume(0.1) している */
        Mix_VolumeChunk(snd_[i].get(), static_cast<int>(MIX_MAX_VOLUME * 0.1));
    }

    std::string path = base_path + "fonts/space_invaders.ttf";
    font20_ = FontPtr(TTF_OpenFont(path.c_str(), 20));
    font25_ = FontPtr(TTF_OpenFont(path.c_str(), 25));
    font50_ = FontPtr(TTF_OpenFont(path.c_str(), 50));
    if (!font20_ || !font25_ || !font50_) {
        std::fprintf(stderr, "font load failed: %s: %s\n",
                     path.c_str(), TTF_GetError());
        return;
    }
    ok_ = true;
}

void Assets::draw_text(SDL_Renderer *renderer, int size, const std::string &msg,
                       SDL_Color color, int x, int y) const
{
    TTF_Font *font = (size == 50) ? font50_.get()
                   : (size == 25) ? font25_.get() : font20_.get();
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, msg.c_str(), color);
    if (!surf)
        return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = {x, y, surf->w, surf->h};
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

int Assets::play(Snd s) const
{
    std::size_t i = index(s);
    if (i >= SND_COUNT || !snd_[i])
        return -1;
    return Mix_PlayChannel(-1, snd_[i].get(), 0);
}

int Assets::play_on(Snd s, int channel) const
{
    std::size_t i = index(s);
    if (i >= SND_COUNT || !snd_[i])
        return -1;
    return Mix_PlayChannel(channel, snd_[i].get(), 0);
}
