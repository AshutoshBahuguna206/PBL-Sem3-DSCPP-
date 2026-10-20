#pragma once
// Plain C++ (no Qt): the rules for "what can the player interact with, and where do they appear".
// Unit-tested, so the Qt screen only has to draw and forward key presses.
#include <algorithm>
#include <cmath>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "../Game.h"
#include "Player.h"
#include "TileMap.h"
#include "TiledMap.h"

inline bool isInteractive(const MapObject& o) {
    return o.type == "clue" || o.type == "door" || o.type == "npc" || o.type == "dial";
}

// Is this object currently usable? Hidden doors appear only after their clue; collected clues disappear.
inline bool objectActive(const MapObject& o, const Game& g) {
    if (!isInteractive(o)) return false;
    const std::string hidden = o.prop("hidden_until");
    if (!hidden.empty() && !g.has(hidden)) return false;
    if ((o.type == "clue" || o.type == "dial") && g.has(o.prop("clue_id"))) return false;
    return true;
}

inline float distanceToRect(float px, float py, const MapObject& o) {
    float dx = std::max(std::max(o.x - px, 0.f), px - (o.x + o.w));
    float dy = std::max(std::max(o.y - py, 0.f), py - (o.y + o.h));
    return std::sqrt(dx * dx + dy * dy);
}

inline const MapObject* nearestInteractable(const TiledMap& m, const Game& g, float px, float py, float radius = 36.f) {
    const MapObject* best = nullptr;
    float bestD = radius + 0.001f;
    for (const auto& o : m.objects()) {
        if (!objectActive(o, g)) continue;
        float d = distanceToRect(px, py, o);
        if (d < bestD) { bestD = d; best = &o; }
    }
    return best;
}

// Where does the player stand after walking through a door? Next to the door that leads back to `from`:
// we try the tile below it, then above, to the right, to the left (snapped to tile centres, so big doors such
// as a wardrobe or bookcase work too) and use the first spot that is free.
inline std::pair<float, float> arrivalPoint(const TiledMap& m, const TileMap& collision, const Id& from, Player probe) {
    const float ts = static_cast<float>(collision.tileSize());
    const MapObject* door = nullptr;
    if (!from.empty())
        for (const auto& o : m.objects()) if (o.type == "door" && o.prop("to") == from) { door = &o; break; }
    std::vector<std::pair<float, float>> spots;
    if (door) {
        const float cx = std::floor(door->cx() / ts) * ts + ts / 2, cy = std::floor(door->cy() / ts) * ts + ts / 2;
        spots = {{cx, door->y + door->h + ts / 2}, {cx, door->y - ts / 2}, {door->x + door->w + ts / 2, cy}, {door->x - ts / 2, cy}};
    } else if (const MapObject* s = m.findByType("spawn", "start")) {
        spots = {{s->cx(), s->cy()}, {s->cx(), s->cy() + ts}, {s->cx(), s->cy() - ts}, {s->cx() + ts, s->cy()}, {s->cx() - ts, s->cy()}};
    } else {
        spots = {{ts * 1.5f, ts * 1.5f}};
    }
    for (const auto& sp : spots)
        if (!probe.collides(sp.first, sp.second, collision)) return sp;
    return spots.front();
}

// Which tiles can the player reach by walking from (sx, sy)? (BFS over free tiles.) Used to prove no
// clue, door or person is ever sealed behind a wall.
inline std::vector<char> reachableTiles(const TileMap& m, const Player& probe, int sx, int sy) {
    const int w = m.width(), h = m.height(), ts = m.tileSize();
    std::vector<char> seen(static_cast<size_t>(w * h), 0);
    auto free = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < w && y < h && !probe.collides((x + 0.5f) * ts, (y + 0.5f) * ts, m);
    };
    if (!free(sx, sy)) return seen;
    std::queue<std::pair<int, int>> q;
    q.push({sx, sy}); seen[static_cast<size_t>(sy * w + sx)] = 1;
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    while (!q.empty()) {
        auto [x, y] = q.front(); q.pop();
        for (int k = 0; k < 4; ++k) {
            int nx = x + dx[k], ny = y + dy[k];
            if (free(nx, ny) && !seen[static_cast<size_t>(ny * w + nx)]) { seen[static_cast<size_t>(ny * w + nx)] = 1; q.push({nx, ny}); }
        }
    }
    return seen;
}

inline std::string resultMessage(Result r) {
    switch (r) {
        case Result::Ok: return "Done.";
        case Result::Unknown: return "There is nothing here.";
        case Result::NotAdjacent: return "You cannot get there from here.";
        case Result::Locked: return "It is locked. You will need to find a way in.";
        case Result::WrongPlace: return "That is not here.";
        case Result::MissingPrereq: return "You look closely, but you cannot make sense of this yet. Perhaps you are missing something.";
        case Result::AlreadyHave: return "You already have that.";
        case Result::NothingToUndo: return "Nothing to undo.";
    }
    return "";
}
