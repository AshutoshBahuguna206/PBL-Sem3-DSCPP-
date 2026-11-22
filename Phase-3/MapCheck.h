#pragma once
// Cross-checks a Tiled map against the case data, so mistakes show up as readable messages
// instead of "why can't I collect this clue?" at 2am.
#include <set>
#include <string>
#include <vector>
#include "Case.h"
#include "TiledMap.h"

inline std::vector<std::string> checkMap(const TiledMap& m, const Case& c, const Id& locId) {
    std::vector<std::string> w;
    const Location* loc = c.location(locId);
    if (!loc) { w.push_back("unknown location id: " + locId); return w; }

    std::set<Id> clueObjs, doorObjs;
    for (const auto& o : m.objects()) {
        const Id hidden = o.prop("hidden_until");
        if (!hidden.empty() && !c.clue(hidden)) w.push_back("object '" + o.name + "': unknown hidden_until clue '" + hidden + "'");
        if (o.type == "clue" || o.type == "dial" || o.type == "npc") {
            const Id id = o.prop("clue_id");
            if (o.type != "npc" && id.empty()) w.push_back("object '" + o.name + "': missing clue_id");
            if (!id.empty()) {
                const Clue* k = c.clue(id);
                if (!k) w.push_back("object '" + o.name + "': unknown clue_id '" + id + "'");
                else {
                    if (k->location() != locId) w.push_back("clue " + id + " belongs to '" + k->location() + "', not '" + locId + "'");
                    clueObjs.insert(id);
                }
            }
            if (o.type == "npc" && !c.suspect(o.prop("suspect_id")))
                w.push_back("npc '" + o.name + "': unknown suspect_id '" + o.prop("suspect_id") + "'");
        } else if (o.type == "door") {
            const Id to = o.prop("to");
            bool isExit = false;
            for (const auto& e : loc->exits()) if (e == to) isExit = true;
            if (!c.location(to)) w.push_back("door '" + o.name + "': unknown location '" + to + "'");
            else if (!isExit) w.push_back("door '" + o.name + "': '" + locId + "' has no exit to '" + to + "'");
            else doorObjs.insert(to);
        }
    }
    for (const auto& kv : c.clues())
        if (!kv.second.granted() && kv.second.location() == locId && !clueObjs.count(kv.first))
            w.push_back("clue " + kv.first + " has no object on this map (player could never collect it)");
    for (const auto& e : loc->exits())
        if (!doorObjs.count(e)) w.push_back("exit '" + e + "' has no door object on this map");
    if (locId == c.start() && !m.findByType("spawn", "start")) w.push_back("start map has no spawn point named 'start'");
    return w;
}
