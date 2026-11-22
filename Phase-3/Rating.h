#pragma once
// The detective's rating at the end of the case: how carefully the player investigated, from S (legendary) to D.
// Plain C++, worked out from the game state, so it is tested with the engine tests.
#include <cmath>
#include <string>
#include "Game.h"

struct Rating {
    char grade = 'D';
    std::string title;
    int score = 0;          // 0 to 100
};

// 40% how much credibility is left, 30% how much of the evidence was found, 30% how many of the lies were exposed.
inline Rating rateDetective(const Game& g, const Case& c) {
    int lies = 0, exposed = 0;
    for (const auto& [who, statements] : c.allStatements()) {
        for (const auto& s : statements) if (s.isLie()) ++lies;
        exposed += g.exposedLies(who);
    }
    const double cred = g.credibility() / static_cast<double>(kStartCredibility);
    const double found = c.clues().empty() ? 0.0 : static_cast<double>(g.dossier().size()) / static_cast<double>(c.clues().size());
    const double caught = lies == 0 ? 0.0 : static_cast<double>(exposed) / static_cast<double>(lies);
    Rating r;
    r.score = static_cast<int>(std::lround(100.0 * (0.4 * cred + 0.3 * found + 0.3 * caught)));
    if (r.score >= 90) { r.grade = 'S'; r.title = "Legendary detective"; }
    else if (r.score >= 75) { r.grade = 'A'; r.title = "Sharp-eyed inspector"; }
    else if (r.score >= 60) { r.grade = 'B'; r.title = "Seasoned investigator"; }
    else if (r.score >= 40) { r.grade = 'C'; r.title = "Promising constable"; }
    else { r.grade = 'D'; r.title = "A lucky guess"; }
    return r;
}
