#pragma once
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Case.h"

struct ValidationReport {
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

// Which locations can the player enter, given the clues they hold? (BFS over the world graph)
inline std::unordered_set<Id> reachableLocations(const Case& c, const std::unordered_set<Id>& held) {
    std::unordered_set<Id> seen{c.start()};
    std::queue<Id> q;
    q.push(c.start());
    while (!q.empty()) {
        Id cur = q.front(); q.pop();
        for (const Id& next : c.location(cur)->exits()) {
            if (seen.count(next)) continue;
            const Id& req = c.location(next)->requiredClue();
            if (!req.empty() && !held.count(req)) continue;  // locked door
            seen.insert(next);
            q.push(next);
        }
    }
    return seen;
}

inline bool prerequisitesMet(const Clue& clue, const std::unordered_set<Id>& held) {
    for (const Id& p : clue.prerequisites())
        if (!held.count(p)) return false;
    return true;
}

// Proves a case is well-formed AND solvable by simulating a perfect player.
inline ValidationReport validate(const Case& c) {
    ValidationReport r;
    auto err = [&](const std::string& m) { r.errors.push_back(m); };

    // 1. Referential integrity
    if (!c.location(c.start())) err("start location missing: " + c.start());
    if (!c.suspect(c.culprit())) err("culprit is not a suspect: " + c.culprit());
    for (auto& [id, l] : c.locations()) {
        for (auto& e : l.exits()) if (!c.location(e)) err(id + ": unknown exit " + e);
        if (!l.requiredClue().empty() && !c.clue(l.requiredClue())) err(id + ": unknown required clue");
    }
    for (auto& [id, k] : c.clues()) {
        if (!c.location(k.location())) err(id + ": unknown location " + k.location());
        for (auto& p : k.prerequisites()) if (!c.clue(p)) err(id + ": unknown prerequisite " + p);
        if (!k.contradicts().empty() && !c.suspect(k.contradicts())) err(id + ": contradicts unknown suspect");
    }
    for (const auto& kv : c.allStatements()) {
        if (!c.suspect(kv.first)) err("statements for unknown person: " + kv.first);
        for (const Statement& st : kv.second) {
            for (const Id& k : st.contradictedBy) if (!c.clue(k)) err(kv.first + ": statement contradicted by unknown clue " + k);
            if (!st.reveals.empty() && !c.clue(st.reveals)) err(kv.first + ": reveals unknown clue " + st.reveals);
            if (!st.pressReveals.empty() && !c.clue(st.pressReveals)) err(kv.first + ": press reveals unknown clue " + st.pressReveals);
        }
    }
    for (const LabTask& t : c.labs()) {
        for (const Id& n : t.needs) if (!c.clue(n)) err("lab " + t.id + ": unknown needed clue " + n);
        if (!c.clue(t.grants)) err("lab " + t.id + ": unknown result clue " + t.grants);
    }
    if (!r.ok()) return r;  // later checks assume valid references

    // 2. Clue prerequisites must form a DAG (Kahn's topological sort)
    std::unordered_map<Id, int> indeg;
    std::unordered_map<Id, std::vector<Id>> out;
    for (auto& [id, k] : c.clues()) {
        indeg[id] += 0;
        for (auto& p : k.prerequisites()) { out[p].push_back(id); indeg[id]++; }
    }
    std::queue<Id> q;
    for (auto& [id, d] : indeg) if (d == 0) q.push(id);
    size_t sorted = 0;
    while (!q.empty()) {
        Id cur = q.front(); q.pop(); sorted++;
        for (auto& nxt : out[cur]) if (--indeg[nxt] == 0) q.push(nxt);
    }
    if (sorted != c.clues().size()) { err("clue prerequisites contain a cycle"); return r; }

    const bool story = !c.statements(c.culprit()).empty();   // does this case use cross-examination?

    // 3. Simulate a perfect player: collect, question, test, repeat until nothing changes.
    std::unordered_set<Id> held;
    auto earn = [&](const Id& id) {
        if (id.empty() || held.count(id) || !prerequisitesMet(*c.clue(id), held)) return false;
        held.insert(id); return true;
    };
    for (bool changed = true; changed;) {
        changed = false;
        auto reach = reachableLocations(c, held);
        for (auto& [id, k] : c.clues())
            if (!k.granted() && !held.count(id) && reach.count(k.location()) && prerequisitesMet(k, held)) { held.insert(id); changed = true; }
        for (auto& [sid, vec] : c.allStatements())
            for (const Statement& st : vec) {
                if (earn(st.pressReveals)) changed = true;
                bool caught = false;
                for (const Id& k : st.contradictedBy) if (held.count(k)) caught = true;
                if (caught && earn(st.reveals)) changed = true;
            }
        for (const LabTask& t : c.labs()) {
            bool ready = true;
            for (const Id& n : t.needs) if (!held.count(n)) ready = false;
            if (ready && earn(t.grants)) changed = true;
        }
    }
    for (auto& [id, k] : c.clues())
        if (!held.count(id)) err("clue can never be obtained: " + id);

    if (!story) {
        // 4. (older cases) exactly one suspect can be proven guilty, and it is the culprit.
        std::unordered_set<Id> provable;
        for (const Id& id : held) {
            const Id& s = c.clue(id)->contradicts();
            if (!s.empty()) provable.insert(s);
        }
        if (!provable.count(c.culprit())) err("culprit cannot be proven with obtainable clues");
        if (provable.size() > 1) err("more than one suspect has a breakable alibi (ambiguous case)");
    } else {
        // 4. (cross-examination cases) every lie can be exposed, the killer has >= 2 exposable incriminating lies,
        //    and nobody else has any incriminating statement.
        int killerExposable = 0;
        for (auto& [sid, vec] : c.allStatements())
            for (size_t i = 0; i < vec.size(); ++i) {
                const Statement& st = vec[i];
                bool exposable = false;
                for (const Id& k : st.contradictedBy) if (held.count(k)) exposable = true;
                if (st.isLie() && !exposable) err(sid + ": lie #" + std::to_string(i) + " can never be exposed");
                if (st.incriminating && sid != c.culprit()) err(sid + ": innocent person has an incriminating statement");
                if (st.incriminating && sid == c.culprit() && exposable) ++killerExposable;
            }
        if (killerExposable < 2) err("the killer has fewer than two exposable incriminating lies");
        if (c.suspect(c.culprit()) && !c.suspect(c.culprit())->accusable()) err("the culprit cannot be accused");
    }
    return r;
}
