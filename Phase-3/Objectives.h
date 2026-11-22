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

// ---------------------------------------------------------------------------------------------------------------
// Objectives (Phase 3): the case broken into steps, shown one at a time at the top of the screen.
// ---------------------------------------------------------------------------------------------------------------
#include <functional>
#include <initializer_list>
#include <queue>
#include <utility>
#include <vector>

// One step of the case.
struct Objective {
    std::string text;                           // what to do, shown on screen
    std::string place;                          // the location id where it happens ("" if anywhere)
    std::function<bool(const Game&)> isDone;    // true once the step is done
};

// Stores the objectives in order. A vector keeps all of them (for the checklist) and a queue keeps the ones still to
// do, first in, first out: the front of the queue is the current objective. update() pops every finished step from
// the front and returns them in order, so the window can tick them off one by one. A step finished early (out of
// order) waits in the queue and is ticked off as soon as it reaches the front.
class ObjectiveTracker {
public:
    ObjectiveTracker() = default;
    explicit ObjectiveTracker(std::vector<Objective> list) : list_(std::move(list)) { reset(); }

    void reset() {
        done_.assign(list_.size(), false);
        pending_ = std::queue<size_t>();
        for (size_t i = 0; i < list_.size(); ++i) pending_.push(i);
    }
    std::vector<size_t> update(const Game& g) {
        std::vector<size_t> finished;
        while (!pending_.empty() && list_[pending_.front()].isDone && list_[pending_.front()].isDone(g)) {
            done_[pending_.front()] = true;
            finished.push_back(pending_.front());
            pending_.pop();
        }
        return finished;
    }

    bool hasCurrent() const { return !pending_.empty(); }
    const Objective& current() const { return list_[pending_.front()]; }       // only when hasCurrent()
    size_t currentIndex() const { return pending_.empty() ? list_.size() : pending_.front(); }
    size_t completed() const { return list_.size() - pending_.size(); }
    size_t total() const { return list_.size(); }
    bool allDone() const { return pending_.empty(); }
    bool isDone(size_t i) const { return i < done_.size() && done_[i]; }
    const Objective& at(size_t i) const { return list_[i]; }

private:
    std::vector<Objective> list_;
    std::vector<bool> done_;
    std::queue<size_t> pending_;
};

// "Done when the player holds all of these clues."
inline std::function<bool(const Game&)> holds(std::initializer_list<const char*> ids) {
    std::vector<std::string> need(ids.begin(), ids.end());
    return [need](const Game& g) {
        for (const auto& id : need) if (!g.has(id)) return false;
        return true;
    };
}

// The objectives of "The Old Way", in the order a player would naturally follow them.
inline std::vector<Objective> makeRastogiObjectives() {
    return {
        {"Talk to Dr. Reddy about the body", "library", holds({"c_body"})},
        {"Find out when he died: open the lab (L) and do the time-of-death test", "", holds({"c_tod"})},
        {"Search the library: the armchair, the side table and the desk", "library", holds({"c_scene", "c_mill_report", "c_cheque"})},
        {"Catch Rohit's lie: show him evidence that contradicts what he says", "library", holds({"c_rohit_testimony"})},
        {"Get the attic key: catch the cook's lie about a secret passage", "kitchen", holds({"c_shanta_testimony"})},
        {"Catch Meera's lie: the cook's confession may help", "hall", holds({"c_meera_testimony"})},
        {"Catch Kabir's lie: find his phone first", "family_wing", holds({"c_kabir_testimony"})},
        {"Search the Blue Room: the pen and the lawyer's phone", "blue_room", holds({"c_pen", "c_wife_text"})},
        {"Search the attic: find the blueprint", "attic", holds({"c_blueprint"})},
        {"Find the nameplate: it hides the dial's code", "attic", holds({"c_nameplate"})},
        {"Open the strongroom dial in the secret passage", "passage", holds({"c_strongroom_open"})},
        {"Search the passage and the strongroom", "passage", holds({"c_cufflink", "c_forged_fd", "c_diary", "c_new_will"})},
        {"Test the pen's ink: open the lab (L) and do the ink test", "", holds({"c_ink_match"})},
        {"Expose at least two of Advocate Deshpande's lies", "hall",
         [](const Game& g) { return g.exposedIncriminating("s_raghav") >= 2; }},
        {"Accuse the killer: press V", "", [](const Game&) { return false; }},   // finished by the ending itself
    };
}
