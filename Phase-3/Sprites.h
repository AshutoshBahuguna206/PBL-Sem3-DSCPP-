#pragma once
// The character sprites, in the order of their rows in assets/sprites/characters.png (made by
// tools/make_mansion_assets.py, whose CHARS list must keep the same order). Plain C++, so it is tested.
#include <iterator>
#include <string>

inline constexpr const char* kSpriteNames[] = {"detective", "raghav", "reddy", "shanta", "rohit", "meera", "kabir", "dayanand"};

// The row of a sprite by name (Raghav's row if the name is unknown).
inline int spriteIndex(const std::string& name) {
    for (int i = 0; i < static_cast<int>(std::size(kSpriteNames)); ++i) if (name == kSpriteNames[i]) return i;
    return 1;
}
inline bool spriteExists(const std::string& name) {
    for (const char* n : kSpriteNames) if (name == n) return true;
    return false;
}
