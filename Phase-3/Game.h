#pragma once
#include <algorithm>
#include <set>
#include <stack>
#include <string>
#include <unordered_set>
#include <vector>
#include "Case.h"
#include "Validator.h"

enum class Result { Ok, Unknown, NotAdjacent, Locked, WrongPlace, MissingPrereq, AlreadyHave, NothingToUndo };
enum class Verdict { Solved, WrongSuspect, InsufficientEvidence, UnknownSuspect };
enum class Present { Invalid, NotHeld, Exposed, AlreadyExposed, Wrong };

constexpr int kStartCredibility = 6;

// The game state as plain data: exactly what a save file needs to store. It holds no pointers and no Qt types, so it
// can be written to a database and read back. The undo history is deliberately not part of it.
struct Snapshot {
    Id location;
    std::vector<Id> found;                   // clues in the order found (the dossier)
    std::set<std::string> exposed, pressed;  // statement keys such as "s_raghav#1"
    int credibility = kStartCredibility;
    bool operator==(const Snapshot& o) const {
        return location == o.location && found == o.found && exposed == o.exposed && pressed == o.pressed &&
               credibility == o.credibility;
    }
    bool operator!=(const Snapshot& o) const { return !(*this == o); }
};

struct PressOutcome { bool valid = false; std::string text; Id revealed; };
struct PresentOutcome { Present kind = Present::Invalid; std::string text; Id revealed; };

class Game {
public:
    explicit Game(const Case& c) : case_(c), loc_(c.start()) {}

    const Id& location() const { return loc_; }
    const std::vector<Id>& dossier() const { return found_; }  // clues in the order found
    const std::unordered_set<Id>& heldSet() const { return held_; }
    bool has(const Id& clue) const { return held_.count(clue) > 0; }

    // Goes up by one every time the game state really changes: a move, a clue, a caught lie, a lost heart, an undo or a
    // restore. A refused action changes nothing. The window compares it with the last saved value to know when to autosave.
    unsigned long revision() const { return revision_; }

    // Copy the current state out as plain data (for saving).
    Snapshot snapshot() const { return Snapshot{loc_, found_, exposed_, pressed_, credibility_}; }

    // Load a saved state. Returns false, and changes nothing, if the snapshot does not fit this case: an unknown
    // room or clue, a clue listed twice, a bad statement key, or credibility out of range. The undo history is cleared.
    bool restore(const Snapshot& s) {
        if (!case_.location(s.location)) return false;
        if (s.credibility < 0 || s.credibility > kStartCredibility) return false;
        std::unordered_set<Id> seen;
        for (const Id& id : s.found) if (!case_.clue(id) || !seen.insert(id).second) return false;
        for (const auto& k : s.exposed) if (!validKey(k)) return false;
        for (const auto& k : s.pressed) if (!validKey(k)) return false;
        loc_ = s.location;
        found_ = s.found;
        held_ = std::move(seen);
        exposed_ = s.exposed;
        pressed_ = s.pressed;
        credibility_ = s.credibility;
        history_ = std::stack<Action>();
        ++revision_;
        return true;
    }

    Result move(const Id& to) {
        const Location* here = case_.location(loc_);
        const Location* dest = case_.location(to);
        if (!dest) return Result::Unknown;
        bool adjacent = false;
        for (auto& e : here->exits()) if (e == to) adjacent = true;
        if (!adjacent) return Result::NotAdjacent;
        if (!dest->requiredClue().empty() && !has(dest->requiredClue())) return Result::Locked;
        history_.push({Action::Move, loc_});
        loc_ = to;
        ++revision_;
        return Result::Ok;
    }

    // Pick up something lying in the current room.
    Result collect(const Id& clueId) {
        const Clue* k = case_.clue(clueId);
        if (!k) return Result::Unknown;
        if (has(clueId)) return Result::AlreadyHave;
        if (k->granted() || k->location() != loc_) return Result::WrongPlace;
        if (!prerequisitesMet(*k, held_)) return Result::MissingPrereq;
        return add(clueId, true);
    }

    // Earn a clue from a conversation or a lab result (no room needed, but its prerequisites must be held).
    Result grant(const Id& clueId) {
        const Clue* k = case_.clue(clueId);
        if (!k) return Result::Unknown;
        if (has(clueId)) return Result::AlreadyHave;
        if (!prerequisitesMet(*k, held_)) return Result::MissingPrereq;
        return add(clueId, false);
    }

    // Stack-based undo: pops the most recent action and reverses it.
    // Rule (Phase 3): undo covers moves and clues picked up in rooms only. Clues earned in a conversation or a
    // lab (grant) are never recorded, so undo can never take back evidence you earned, and it never un-exposes a lie.
    Result undo() {
        if (history_.empty()) return Result::NothingToUndo;
        Action a = history_.top(); history_.pop();
        if (a.type == Action::Move) {
            loc_ = a.arg;
        } else {
            held_.erase(a.arg);
            found_.erase(std::remove(found_.begin(), found_.end(), a.arg), found_.end());
        }
        ++revision_;
        return Result::Ok;
    }

    // ---------------------------------------------------------------- cross-examination
    int credibility() const { return credibility_; }
    bool failed() const { return credibility_ <= 0; }
    void penalize(int n) {
        const int before = credibility_;
        credibility_ = std::max(0, credibility_ - n);
        if (credibility_ != before) ++revision_;
    }

    const std::vector<Statement>& statements(const Id& suspect) const { return case_.statements(suspect); }
    bool exposed(const Id& suspect, size_t idx) const { return exposed_.count(key(suspect, idx)) > 0; }
    bool pressed(const Id& suspect, size_t idx) const { return pressed_.count(key(suspect, idx)) > 0; }

    int exposedLies(const Id& suspect) const {
        int n = 0; const auto& st = case_.statements(suspect);
        for (size_t i = 0; i < st.size(); ++i) if (st[i].isLie() && exposed(suspect, i)) ++n;
        return n;
    }
    int exposedIncriminating(const Id& suspect) const {
        int n = 0; const auto& st = case_.statements(suspect);
        for (size_t i = 0; i < st.size(); ++i) if (st[i].incriminating && exposed(suspect, i)) ++n;
        return n;
    }

    // "Press" a statement: ask for more detail. May earn a clue.
    PressOutcome press(const Id& suspect, size_t idx) {
        const auto& st = case_.statements(suspect);
        PressOutcome o;
        if (idx >= st.size()) return o;
        o.valid = true;
        o.text = st[idx].press.empty() ? "They have nothing more to add." : st[idx].press;
        if (pressed_.insert(key(suspect, idx)).second) ++revision_;
        if (!st[idx].pressReveals.empty() && grant(st[idx].pressReveals) == Result::Ok) o.revealed = st[idx].pressReveals;
        return o;
    }

    // "Present" a clue against a statement. The right clue exposes a lie; the wrong one costs credibility.
    PresentOutcome present(const Id& suspect, size_t idx, const Id& clue) {
        const auto& st = case_.statements(suspect);
        PresentOutcome o;
        if (idx >= st.size()) return o;
        if (!has(clue)) { o.kind = Present::NotHeld; return o; }
        if (exposed(suspect, idx)) { o.kind = Present::AlreadyExposed; o.text = "You have already caught them on that."; return o; }
        const Statement& s = st[idx];
        if (std::find(s.contradictedBy.begin(), s.contradictedBy.end(), clue) != s.contradictedBy.end()) {
            if (exposed_.insert(key(suspect, idx)).second) ++revision_;
            o.kind = Present::Exposed;
            o.text = s.confession;
            if (!s.reveals.empty() && grant(s.reveals) == Result::Ok) o.revealed = s.reveals;
            return o;
        }
        penalize(1);
        o.kind = Present::Wrong;
        o.text = "That does not contradict what they said.";
        return o;
    }

    // ---------------------------------------------------------------- forensic lab
    bool labDone(const Id& lab) const { const LabTask* t = case_.lab(lab); return t && has(t->grants); }
    bool labReady(const Id& lab) const {
        const LabTask* t = case_.lab(lab);
        if (!t || has(t->grants)) return false;
        for (const Id& n : t->needs) if (!has(n)) return false;
        return true;
    }
    Result completeLab(const Id& lab) {
        const LabTask* t = case_.lab(lab);
        if (!t) return Result::Unknown;
        if (has(t->grants)) return Result::AlreadyHave;
        if (!labReady(lab)) return Result::MissingPrereq;
        return grant(t->grants);
    }

    // Clues the player holds that break this suspect's alibi (older cases).
    std::vector<Id> proofAgainst(const Id& suspect) const {
        std::vector<Id> out;
        for (auto& id : found_) if (case_.clue(id)->contradicts() == suspect) out.push_back(id);
        return out;
    }

    // Accusing requires PROOF. In a case with cross-examination, the killer's lies must be exposed.
    Verdict accuse(const Id& suspect) const {
        if (!case_.suspect(suspect)) return Verdict::UnknownSuspect;
        if (suspect != case_.culprit()) return Verdict::WrongSuspect;
        if (!case_.statements(suspect).empty()) return exposedIncriminating(suspect) >= 2 ? Verdict::Solved : Verdict::InsufficientEvidence;
        return proofAgainst(suspect).empty() ? Verdict::InsufficientEvidence : Verdict::Solved;
    }

private:
    struct Action { enum Type { Move, Collect } type; Id arg; };
    static std::string key(const Id& s, size_t i) { return s + "#" + std::to_string(i); }
    // A statement key looks like "s_raghav#1": a real suspect, then the number of a statement that exists.
    bool validKey(const std::string& k) const {
        const size_t hash = k.find('#');
        if (hash == std::string::npos || hash == 0 || hash + 1 >= k.size()) return false;
        const Id who = k.substr(0, hash);
        if (!case_.suspect(who)) return false;
        const std::string num = k.substr(hash + 1);
        if (num.find_first_not_of("0123456789") != std::string::npos) return false;
        size_t idx = 0;
        try { idx = std::stoul(num); } catch (...) { return false; }
        return idx < case_.statements(who).size();
    }
    // The only way a clue enters the dossier. Only room pick-ups are recorded for undo (recordForUndo).
    Result add(const Id& clueId, bool recordForUndo) {
        held_.insert(clueId);
        found_.push_back(clueId);
        ++revision_;
        if (recordForUndo) history_.push({Action::Collect, clueId});
        return Result::Ok;
    }
    const Case& case_;
    Id loc_;
    std::unordered_set<Id> held_;
    std::vector<Id> found_;
    std::stack<Action> history_;
    std::set<std::string> exposed_, pressed_;
    int credibility_ = kStartCredibility;
    unsigned long revision_ = 0;
};
