#pragma once
#include <cmath>
#include "TileMap.h"

// Square collision box centred on (x, y), in pixels. Also remembers which way the player faces
// and whether they are walking, so the screen can pick the right animation frame.
struct Player {
    float x = 0, y = 0;
    float half = 9.f;     // half the box size (box is 18x18 px, smaller than a 32px tile)
    float speed = 130.f;  // pixels per second
    int facing = 0;       // 0 = down, 1 = left, 2 = right, 3 = up (matches the sprite sheet)
    bool moving = false;
    float anim = 0.f;     // seconds spent walking; drives the walk cycle

    // dir is the input direction (e.g. -1,0 = left). dt is the frame time in seconds.
    // X and Y are resolved separately so the player slides along walls instead of sticking.
    void move(float dirX, float dirY, float dt, const TileMap& map) {
        float len = std::sqrt(dirX * dirX + dirY * dirY);
        if (len > 0) { dirX /= len; dirY /= len; }  // diagonal is not faster
        const float ox = x, oy = y;
        float nx = x + dirX * speed * dt;
        if (!collides(nx, y, map)) x = nx;
        float ny = y + dirY * speed * dt;
        if (!collides(x, ny, map)) y = ny;
        moving = (std::fabs(x - ox) + std::fabs(y - oy)) > 0.001f;
        if (len > 0) {
            if (std::fabs(dirX) > std::fabs(dirY)) facing = dirX < 0 ? 1 : 2;
            else facing = dirY < 0 ? 3 : 0;
        }
        anim = moving ? anim + dt : 0.f;
    }

    bool collides(float cx, float cy, const TileMap& map) const {
        const float e = 0.01f;
        return map.blockedPx(cx - half, cy - half) || map.blockedPx(cx + half - e, cy - half) ||
               map.blockedPx(cx - half, cy + half - e) || map.blockedPx(cx + half - e, cy + half - e);
    }
};
