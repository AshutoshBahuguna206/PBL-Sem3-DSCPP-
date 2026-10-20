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

struct PressOutcome { bool valid = false; std::string text; Id revealed; };
struct PresentOutcome { Present kind = Present::Invalid; std::string text; Id revealed; };

class Game {
public:
    explicit Game(const Case& c) : case_(c), loc_(c.start()) {}

    const Id& location() const { return loc_; }
    const std::vector<Id>& dossier() const { return found_; }  // clues in the order found
    const std::unordered_set<Id>& heldSet() const { return held_; }
    bool has(const Id& clue) const { return held_.count(clue) > 0; }

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
        return Result::Ok;
    }

    // Pick up something lying in the current room.
    Result collect(const Id& clueId) {
        const Clue* k = case_.clue(clueId);
        if (!k) return Result::Unknown;
        if (has(clueId)) return Result::AlreadyHave;
        if (k->granted() || k->location() != loc_) return Result::WrongPlace;
        if (!prerequisitesMet(*k, held_)) return Result::MissingPrereq;
        return add(clueId);
    }

    // Earn a clue from a conversation or a lab result (no room needed, but its prerequisites must be held).
    Result grant(const Id& clueId) {
        const Clue* k = case_.clue(clueId);
        if (!k) return Result::Unknown;
        if (has(clueId)) return Result::AlreadyHave;
        if (!prerequisitesMet(*k, held_)) return Result::MissingPrereq;
        return add(clueId);
    }

    // Stack-based undo: pops the most recent action and reverses it.
    Result undo() {
        if (history_.empty()) return Result::NothingToUndo;
        Action a = history_.top(); history_.pop();
        if (a.type == Action::Move) {
            loc_ = a.arg;
        } else {
            held_.erase(a.arg);
            found_.erase(std::remove(found_.begin(), found_.end(), a.arg), found_.end());
        }
        return Result::Ok;
    }

    // ---------------------------------------------------------------- cross-examination
    int credibility() const { return credibility_; }
    bool failed() const { return credibility_ <= 0; }
    void penalize(int n) { credibility_ = std::max(0, credibility_ - n); }

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
        pressed_.insert(key(suspect, idx));
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
            exposed_.insert(key(suspect, idx));
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
    Result add(const Id& clueId) {
        held_.insert(clueId);
        found_.push_back(clueId);
        history_.push({Action::Collect, clueId});
        return Result::Ok;
    }
    const Case& case_;
    Id loc_;
    std::unordered_set<Id> held_;
    std::vector<Id> found_;
    std::stack<Action> history_;
    std::set<std::string> exposed_, pressed_;
    int credibility_ = 6;
};
