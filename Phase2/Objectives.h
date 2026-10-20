#pragma once
// A gentle nudge so the player never feels lost: the next sensible goal, based on what they hold.
#include <string>
#include "Game.h"

inline std::string currentObjective(const Game& g) {
    if (!g.has("c_body")) return "Talk to Dr. Reddy in the library.";
    if (!g.has("c_tod")) return "Press L to open the forensic lab and work out when he died.";
    if (!g.has("c_rohit_testimony") || !g.has("c_kabir_testimony") || !g.has("c_meera_testimony"))
        return "Cross-examine the family. Find a lie, then show evidence that contradicts it (Rohit, Kabir, Meera).";
    if (!g.has("c_shanta_testimony")) return "Those scrape marks in the library. Ask the cook what she knows.";
    if (!g.has("c_blueprint")) return "The cook's key opens the attic. Search it.";
    if (!g.has("c_forged_fd")) return "A hidden passage runs from the library. Find what it protects.";
    if (!g.has("c_ink_match")) return "Press L: whose pen wrote the forged signature?";
    return "You know enough. Cross-examine Advocate Deshpande, then press V to accuse.";
}
