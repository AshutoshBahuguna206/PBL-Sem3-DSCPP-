#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "Entities.h"

// Immutable-after-load description of one mystery.
// Hash maps give O(1) lookup by ID (the registry the proposal promised).
class Case {
public:
    Case(std::string title, Id startLocation, Id culprit)
        : title_(std::move(title)), start_(std::move(startLocation)), culprit_(std::move(culprit)) {}

    void add(Location l) { locations_.emplace(l.id(), std::move(l)); }
    void add(Clue c)     { clues_.emplace(c.id(), std::move(c)); }
    void add(Suspect s)  { suspects_.emplace(s.id(), std::move(s)); }
    void addStatement(const Id& suspect, Statement s) { statements_[suspect].push_back(std::move(s)); }
    void addLab(LabTask t) { labs_.push_back(std::move(t)); }

    const Location* location(const Id& id) const { return find(locations_, id); }
    const Clue*     clue(const Id& id) const     { return find(clues_, id); }
    const Suspect*  suspect(const Id& id) const  { return find(suspects_, id); }
    const LabTask*  lab(const Id& id) const { for (const auto& t : labs_) if (t.id == id) return &t; return nullptr; }

    const std::vector<Statement>& statements(const Id& suspect) const {
        static const std::vector<Statement> none;
        auto it = statements_.find(suspect);
        return it == statements_.end() ? none : it->second;
    }
    const std::unordered_map<Id, std::vector<Statement>>& allStatements() const { return statements_; }
    const std::vector<LabTask>& labs() const { return labs_; }

    const std::unordered_map<Id, Location>& locations() const { return locations_; }
    const std::unordered_map<Id, Clue>& clues() const { return clues_; }
    const std::unordered_map<Id, Suspect>& suspects() const { return suspects_; }

    const std::string& title() const { return title_; }
    const Id& start() const { return start_; }
    const Id& culprit() const { return culprit_; }

private:
    template <class M>
    static const typename M::mapped_type* find(const M& m, const Id& id) {
        auto it = m.find(id);
        return it == m.end() ? nullptr : &it->second;
    }
    std::string title_;
    Id start_, culprit_;
    std::unordered_map<Id, Location> locations_;
    std::unordered_map<Id, Clue> clues_;
    std::unordered_map<Id, Suspect> suspects_;
    std::unordered_map<Id, std::vector<Statement>> statements_;
    std::vector<LabTask> labs_;
};
