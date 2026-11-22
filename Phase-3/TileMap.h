#pragma once
// Plain C++ (no Qt) so it can be unit-tested. Step 3 will fill this from a Tiled JSON file instead.
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class TileMap {
public:
    // rows: one string per map row. '#' = wall, '.' = floor, 'D' = door, 'o' = object spot.
    explicit TileMap(std::vector<std::string> rows, int tileSize = 32)
        : rows_(std::move(rows)), tileSize_(tileSize) {
        if (rows_.empty()) throw std::invalid_argument("empty map");
        for (const auto& r : rows_)
            if (r.size() != rows_[0].size()) throw std::invalid_argument("ragged map rows");
    }

    int width() const { return static_cast<int>(rows_[0].size()); }
    int height() const { return static_cast<int>(rows_.size()); }
    int tileSize() const { return tileSize_; }

    // Outside the map counts as wall, so the player can never escape.
    char at(int tx, int ty) const {
        if (tx < 0 || ty < 0 || tx >= width() || ty >= height()) return '#';
        return rows_[ty][tx];
    }
    bool blockedTile(int tx, int ty) const { return at(tx, ty) == '#'; }
    // Used for gates that open when the player earns a clue.
    void set(int tx, int ty, char c) {
        if (tx >= 0 && ty >= 0 && tx < width() && ty < height()) rows_[ty][tx] = c;
    }
    bool blockedPx(float px, float py) const {
        return blockedTile(static_cast<int>(std::floor(px / tileSize_)),
                           static_cast<int>(std::floor(py / tileSize_)));
    }

private:
    std::vector<std::string> rows_;
    int tileSize_;
};
