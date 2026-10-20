#include <cmath>
#include <cstdlib>
#include <queue>
#include <algorithm>
#include <fstream>
#ifndef ASSET_DIR
#define ASSET_DIR "assets"
#endif
#include <iostream>
#include "Game.h"
#include "SampleCase.h"
#include "CaseMumbai.h"
#include "CaseRastogi.h"
#include "Forensics.h"
#include "Objectives.h"
#include "gui/Interact.h"
#include "gui/MapCheck.h"
#include "gui/Player.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; failures++; } } while (0)

int main() {
    Case c = makeSampleCase();
    CHECK(validate(c).ok());

    // Cyclic prerequisites must be rejected.
    Case bad("bad", "a", "s");
    bad.add(Location("a", "A", "", {}));
    bad.add(Suspect("s", "S", "", "x"));
    bad.add(Clue("k1", "K1", "", "a", {"k2"}));
    bad.add(Clue("k2", "K2", "", "a", {"k1"}, "s"));
    CHECK(!validate(bad).ok());

    // Unobtainable culprit proof must be rejected (locked behind a clue that doesn't exist physically).
    Case unsolv("unsolvable", "a", "s");
    unsolv.add(Location("a", "A", "", {"b"}));
    unsolv.add(Location("b", "B", "", {"a"}, "k_key"));
    unsolv.add(Suspect("s", "S", "", "x"));
    unsolv.add(Clue("k_key", "Key", "", "b"));          // key is inside the locked room
    unsolv.add(Clue("k_proof", "Proof", "", "b", {}, "s"));
    CHECK(!validate(unsolv).ok());

    // Gameplay rules.
    Game g(c);
    CHECK(g.move("study") == Result::NotAdjacent);
    CHECK(g.move("library") == Result::Ok);
    CHECK(g.move("study") == Result::Locked);
    CHECK(g.collect("c_ledger") == Result::Ok);
    CHECK(g.accuse("s_iyer") == Verdict::InsufficientEvidence);  // right name, no proof
    CHECK(g.accuse("s_rao") == Verdict::WrongSuspect);
    CHECK(g.move("lobby") == Result::Ok);
    CHECK(g.collect("c_keycard") == Result::Ok);
    CHECK(g.move("library") == Result::Ok);
    CHECK(g.move("study") == Result::Ok);
    CHECK(g.collect("c_cctv") == Result::Ok);
    CHECK(g.accuse("s_iyer") == Verdict::Solved);

    // Undo reverses actions in LIFO order.
    CHECK(g.undo() == Result::Ok);  // un-collect cctv
    CHECK(!g.has("c_cctv"));
    CHECK(g.accuse("s_iyer") == Verdict::InsufficientEvidence);
    CHECK(g.undo() == Result::Ok);  // back to library
    CHECK(g.location() == "library");

    // Full case: validator proof + intended deduction path.
    Case r = makeMumbaiCase();
    auto rep = validate(r);
    for (auto& e : rep.errors) std::cerr << "CASE: " << e << "\n";
    CHECK(rep.ok());
    CHECK(r.suspects().size() == 7);
    Game h(r);
    CHECK(h.move("terrace") == Result::Locked);
    CHECK(h.move("builder_office") == Result::Locked);
    CHECK(h.collect("c_fall_spot") == Result::Ok);
    CHECK(h.move("cabin") == Result::Ok);
    CHECK(h.collect("c_terrace_key") == Result::Ok);
    CHECK(h.move("compound") == Result::Ok);
    CHECK(h.move("terrace") == Result::Ok);
    CHECK(h.collect("c_phone") == Result::MissingPrereq);    // must learn who Ramesh meant to confront
    CHECK(h.move("compound") == Result::Ok);
    CHECK(h.move("society_office") == Result::Ok);
    CHECK(h.collect("c_rti") == Result::Ok);
    CHECK(h.move("compound") == Result::Ok);
    CHECK(h.move("flat") == Result::Ok);
    CHECK(h.collect("c_diary") == Result::Ok);
    CHECK(h.move("compound") == Result::Ok);
    CHECK(h.accuse("s_gill") == Verdict::InsufficientEvidence);   // suspicion is not proof
    CHECK(h.move("terrace") == Result::Ok);
    CHECK(h.collect("c_phone") == Result::Ok);
    CHECK(h.accuse("s_mehta") == Verdict::WrongSuspect);          // the bribe-payer is not the killer
    CHECK(h.accuse("s_gill") == Verdict::Solved);

    // ---- Tile map and player movement ----
    TileMap tm({"#####", "#...#", "#.#.#", "#...#", "#####"});
    bool threw = false;
    try { TileMap bad({"###", "##"}); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);                                              // ragged maps are rejected
    CHECK(tm.blockedTile(-1, 0) && tm.blockedTile(99, 99));    // outside = wall
    Player pl; pl.x = 1.5f * 32; pl.y = 1.5f * 32;
    for (int i = 0; i < 200; ++i) pl.move(-1, -1, 0.016f, tm); // push into the corner for ~3s
    CHECK(pl.x - pl.half >= 32.f - 0.01f && pl.y - pl.half >= 32.f - 0.01f);   // never inside a wall
    pl.x = 1.5f * 32; pl.y = 1.5f * 32;
    for (int i = 0; i < 200; ++i) pl.move(1, 0, 0.016f, tm);   // walk right
    CHECK(pl.x + pl.half <= 4 * 32.f + 0.01f);                 // stopped by the right wall
    pl.x = 1.5f * 32; pl.y = 1.5f * 32;
    for (int i = 0; i < 20; ++i) pl.move(1, 1, 0.016f, tm);    // diagonal into the pillar at (2,2)
    CHECK(!pl.collides(pl.x, pl.y, tm));                       // slid along it, never overlapped

    // ---- Facing and walk animation state ----
    {
        TileMap open({"#####", "#...#", "#...#", "#...#", "#####"});
        Player w; w.x = 2.5f * 32; w.y = 2.5f * 32;
        w.move(0, 1, 0.016f, open);  CHECK(w.facing == 0 && w.moving);        // down
        w.move(-1, 0, 0.016f, open); CHECK(w.facing == 1 && w.moving);        // left
        w.move(1, 0, 0.016f, open);  CHECK(w.facing == 2);                    // right
        w.move(0, -1, 0.016f, open); CHECK(w.facing == 3);                    // up
        const float a = w.anim; w.move(0, -1, 0.016f, open); CHECK(w.anim > a);   // animation clock advances while walking
        w.move(0, 0, 0.016f, open);  CHECK(!w.moving && w.anim == 0.f && w.facing == 3);   // idle keeps facing, resets walk cycle
        for (int i = 0; i < 100; ++i) w.move(0, -1, 0.016f, open);
        CHECK(!w.moving);                                                     // pushing into a wall is not "walking"
    }
    {
        std::ifstream cs(std::string(ASSET_DIR) + "/sprites/characters.png", std::ios::binary);
        CHECK(cs.good());
    }

    // ---- JSON parser ----
    Json j = Json::parse(R"({"a":[1,2.5,-3],"s":"hi\n\u0041","t":true,"n":null,"o":{"k":"v"}})");
    CHECK(j["a"].size() == 3 && j["a"][static_cast<size_t>(1)].number() == 2.5);
    CHECK(j["s"].string() == "hi\nA" && j["t"].boolean() && j["n"].isNull() && j["o"]["k"].string() == "v");
    CHECK(j["missing"]["deeper"].isNull());                    // safe on missing keys
    threw = false;
    try { Json::parse("{\"a\": [1,}"); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);

    // ---- Tiled map loading + cross-check against the case ----
    TiledMap tmap = TiledMap::fromFile(std::string(ASSET_DIR) + "/maps/compound.json");
    CHECK(tmap.width() == 20 && tmap.height() == 15 && tmap.tileW() == 32);
    CHECK(tmap.tileset().columns == 4 && tmap.tileset().image == "../tilesets/tileset.png");
    TileMap cm = tmap.collisionMap();
    CHECK(cm.blockedTile(0, 0) && !cm.blockedTile(2, 1));      // border wall vs open floor
    const MapObject* sp = tmap.findByType("spawn", "start");
    CHECK(sp != nullptr && !cm.blockedPx(sp->cx(), sp->cy())); // spawn is not inside a wall
    auto warns = checkMap(tmap, makeMumbaiCase(), "compound");
    for (auto& w : warns) std::cerr << "MAP: " << w << "\n";
    CHECK(warns.empty());
    // A map with a deliberately wrong clue id must be reported.
    std::string badMap = "{\"width\":1,\"height\":1,\"tilewidth\":32,\"tileheight\":32,\"layers\":["
        "{\"type\":\"tilelayer\",\"name\":\"Collision\",\"data\":[0]},"
        "{\"type\":\"objectgroup\",\"objects\":[{\"id\":1,\"name\":\"x\",\"class\":\"clue\",\"x\":0,\"y\":0,\"width\":32,\"height\":32,"
        "\"properties\":[{\"name\":\"clue_id\",\"type\":\"string\",\"value\":\"c_nope\"}]}]}],\"tilesets\":[]}";
    CHECK(!checkMap(TiledMap::fromString(badMap), makeMumbaiCase(), "compound").empty());

    // ---- Case 3: The Old Way: cross-examination + forensics ----
    Case ro = makeRastogiCase();
    auto rrep = validate(ro);
    for (auto& e : rrep.errors) std::cerr << "RASTOGI: " << e << "\n";
    CHECK(rrep.ok());
    CHECK(ro.suspects().size() == 6 && ro.locations().size() == 7);
    {
        Game rg(ro);
        CHECK(rg.credibility() == 6);
        // Forensic puzzles (pure maths)
        TodPuzzle tod;
        CHECK(todAnswerMinute(tod) == 23 * 60 + 15);                 // 37-29 = 8 hours before 7:15am = 11:15pm
        CHECK(todCorrect(tod, 23 * 60) && todCorrect(tod, 23 * 60 + 59) && todCorrect(tod, 0 * 60 + 0));
        CHECK(!todCorrect(tod, 20 * 60) && !todCorrect(tod, 7 * 60));
        InkPuzzle ink = rastogiInkPuzzle();
        CHECK(inkMatch(ink) == 2);                                    // Raghav's pen: Rf 0.71
        CHECK(std::fabs(ink.pens[0].rf() - 0.62) < 1e-9 && std::fabs(ink.pens[1].rf() - 0.65) < 1e-9);

        // Round 1: the library. Talk to the doctor, who teaches and gives forensic facts.
        CHECK(rg.accuse("s_raghav") == Verdict::InsufficientEvidence);
        CHECK(rg.accuse("s_rohit") == Verdict::WrongSuspect);
        CHECK(!ro.suspect("s_reddy")->accusable());
        CHECK(rg.move("library") == Result::Ok);
        CHECK(rg.collect("c_body") == Result::WrongPlace);            // earned, not lying around
        CHECK(rg.press("s_reddy", 0).revealed == "c_body");
        CHECK(rg.press("s_reddy", 1).revealed == "c_milk_glass");
        CHECK(rg.labReady("lab_tod") && !rg.labReady("lab_ink"));
        CHECK(rg.completeLab("lab_tod") == Result::Ok && rg.has("c_tod"));
        CHECK(rg.completeLab("lab_tod") == Result::AlreadyHave);
        CHECK(rg.collect("c_scene") == Result::Ok);
        CHECK(rg.collect("c_mill_report") == Result::Ok);
        CHECK(rg.collect("c_cheque") == Result::Ok);

        // Round 2: cross-examine. A wrong piece of evidence costs credibility.
        CHECK(rg.present("s_rohit", 0, "c_scene").kind == Present::Wrong);
        CHECK(rg.credibility() == 5);
        CHECK(rg.present("s_rohit", 0, "c_betting").kind == Present::NotHeld);   // you can't present what you don't have
        CHECK(rg.credibility() == 5);
        auto rohit = rg.present("s_rohit", 0, "c_mill_report");
        CHECK(rohit.kind == Present::Exposed && rohit.revealed == "c_rohit_testimony");
        CHECK(rg.present("s_rohit", 0, "c_mill_report").kind == Present::AlreadyExposed);
        CHECK(rg.exposedLies("s_rohit") == 1 && rg.exposedIncriminating("s_rohit") == 0);
        CHECK(rg.present("s_rohit", 1, "c_scene").kind == Present::Wrong);       // a TRUE statement cannot be contradicted
        CHECK(rg.credibility() == 4);
        CHECK(rg.move("hall") == Result::Ok);
        CHECK(rg.move("attic") == Result::Locked);
        CHECK(rg.move("kitchen") == Result::Ok);
        CHECK(rg.collect("c_milk_jug") == Result::Ok);                           // needs the doctor's finding first
        auto shanta = rg.present("s_shanta", 0, "c_scene");
        CHECK(shanta.kind == Present::Exposed && shanta.revealed == "c_shanta_testimony");
        CHECK(rg.move("hall") == Result::Ok);
        auto meera = rg.present("s_meera", 0, "c_shanta_testimony");             // Shanta's confession exposes Meera
        CHECK(meera.kind == Present::Exposed && rg.has("c_meera_testimony"));
        CHECK(rg.move("family_wing") == Result::Ok);
        CHECK(rg.collect("c_betting") == Result::Ok);
        CHECK(rg.present("s_kabir", 0, "c_betting").revealed == "c_kabir_testimony");

        // Round 3: the hidden passage and the dial puzzle.
        CHECK(rg.move("hall") == Result::Ok);
        CHECK(rg.move("blue_room") == Result::Ok);
        CHECK(rg.collect("c_pen") == Result::Ok);
        CHECK(rg.collect("c_wife_text") == Result::Ok);                          // unlocked by Kabir's testimony
        CHECK(rg.move("passage") == Result::Locked);
        CHECK(rg.move("hall") == Result::Ok && rg.move("attic") == Result::Ok);
        CHECK(rg.collect("c_blueprint") == Result::Ok && rg.collect("c_nameplate") == Result::Ok);
        CHECK(rg.move("hall") == Result::Ok && rg.move("library") == Result::Ok && rg.move("passage") == Result::Ok);
        CHECK(rg.collect("c_cufflink") == Result::Ok);
        CHECK(rg.collect("c_forged_fd") == Result::MissingPrereq);               // the dial comes first
        CHECK(rg.collect("c_strongroom_open") == Result::Ok);
        CHECK(rg.collect("c_forged_fd") == Result::Ok && rg.collect("c_diary") == Result::Ok && rg.collect("c_new_will") == Result::Ok);

        // Round 4: ink chromatography, then the final cross-examination.
        CHECK(rg.labReady("lab_ink"));
        CHECK(rg.completeLab("lab_ink") == Result::Ok && rg.has("c_ink_match"));
        CHECK(rg.accuse("s_raghav") == Verdict::InsufficientEvidence);           // suspicion is not proof: lies must be exposed
        CHECK(rg.present("s_raghav", 0, "c_wife_text").kind == Present::Exposed);
        CHECK(rg.accuse("s_raghav") == Verdict::InsufficientEvidence);           // one lie is not enough
        CHECK(rg.present("s_raghav", 1, "c_cufflink").kind == Present::Exposed);
        CHECK(rg.accuse("s_raghav") == Verdict::Solved);
        CHECK(rg.present("s_raghav", 2, "c_ink_match").kind == Present::Exposed);
        CHECK(rg.exposedIncriminating("s_raghav") == 3 && rg.credibility() == 4);
        CHECK(rg.accuse("s_meera") == Verdict::WrongSuspect);

        // Credibility runs out: the case is taken away.
        Game bad(ro);
        CHECK(bad.move("library") == Result::Ok);
        bad.press("s_reddy", 0);
        for (int i = 0; i < 6; ++i) bad.present("s_rohit", 1, "c_body");
        CHECK(bad.failed() && bad.credibility() == 0);
        // Objectives guide the player, step by step.
        Game obj(ro);
        CHECK(currentObjective(obj).find("Dr. Reddy") != std::string::npos);
        CHECK(currentObjective(rg).find("accuse") != std::string::npos);
    }

    // ---- Mansion maps: every map matches the case and nothing is sealed behind a wall ----
    {
        std::ifstream tsf(std::string(ASSET_DIR) + "/tilesets/mansion.png", std::ios::binary);
        CHECK(tsf.good());
        for (const auto& kv : ro.locations()) {
            const Id id = kv.first;
            TiledMap mm = TiledMap::fromFile(std::string(ASSET_DIR) + "/maps/" + id + ".json");
            auto warns2 = checkMap(mm, ro, id);
            for (auto& w : warns2) std::cerr << "MAP " << id << ": " << w << "\n";
            CHECK(warns2.empty());
            TileMap col = mm.collisionMap();
            for (const auto& o : mm.objects())                       // pretend gates are open
                if (o.type == "dial")
                    for (int ty = static_cast<int>(o.y) / 32; ty < static_cast<int>(o.y + o.h) / 32; ++ty)
                        for (int tx = static_cast<int>(o.x) / 32; tx < static_cast<int>(o.x + o.w) / 32; ++tx) col.set(tx, ty, '.');
            Id from;
            if (id != ro.start()) for (const auto& o : mm.objects()) if (o.type == "door") { from = o.prop("to"); break; }
            Player probe;
            auto pt = arrivalPoint(mm, col, from, probe);
            CHECK(!probe.collides(pt.first, pt.second, col));        // arrival spot is not inside a wall
            auto seen = reachableTiles(col, probe, static_cast<int>(pt.first) / 32, static_cast<int>(pt.second) / 32);
            for (const auto& o : mm.objects()) {
                if (!isInteractive(o)) continue;
                bool ok = false;
                for (int ty = 0; ty < col.height() && !ok; ++ty)
                    for (int tx = 0; tx < col.width() && !ok; ++tx)
                        if (seen[static_cast<size_t>(ty * col.width() + tx)] &&
                            distanceToRect((tx + 0.5f) * 32, (ty + 0.5f) * 32, o) <= 36.f) ok = true;
                if (!ok) std::cerr << "UNREACHABLE in " << id << ": " << o.name << "\n";
                CHECK(ok);
            }
            // Actually WALK to every interactive object (not just a tile-grid check): proves nobody gets stuck.
            {
                const int W = col.width(), Hh = col.height();
                const int sx = static_cast<int>(pt.first) / 32, sy = static_cast<int>(pt.second) / 32;
                std::vector<int> parent(static_cast<size_t>(W * Hh), -2);
                std::queue<int> bq; parent[static_cast<size_t>(sy * W + sx)] = -1; bq.push(sy * W + sx);
                const int ddx[4] = {1, -1, 0, 0}, ddy[4] = {0, 0, 1, -1};
                while (!bq.empty()) {
                    int cur = bq.front(); bq.pop(); int cx = cur % W, cy = cur / W;
                    for (int k = 0; k < 4; ++k) {
                        int nx = cx + ddx[k], ny = cy + ddy[k];
                        if (nx < 0 || ny < 0 || nx >= W || ny >= Hh || parent[static_cast<size_t>(ny * W + nx)] != -2) continue;
                        if (probe.collides((nx + 0.5f) * 32, (ny + 0.5f) * 32, col)) continue;
                        parent[static_cast<size_t>(ny * W + nx)] = cur; bq.push(ny * W + nx);
                    }
                }
                for (const auto& o : mm.objects()) {
                    if (!isInteractive(o)) continue;
                    int goal = -1;
                    for (int ty = 0; ty < Hh && goal < 0; ++ty)
                        for (int tx = 0; tx < W && goal < 0; ++tx)
                            if (parent[static_cast<size_t>(ty * W + tx)] != -2 && distanceToRect((tx + 0.5f) * 32, (ty + 0.5f) * 32, o) <= 36.f) goal = ty * W + tx;
                    CHECK(goal >= 0);
                    if (goal < 0) continue;
                    std::vector<int> path;
                    for (int c2 = goal; c2 != -1; c2 = parent[static_cast<size_t>(c2)]) path.push_back(c2);
                    std::reverse(path.begin(), path.end());
                    Player walker = probe; walker.x = pt.first; walker.y = pt.second;
                    bool ok2 = true;
                    for (int cell : path) {
                        const float tcx = (cell % W + 0.5f) * 32, tcy = (cell / W + 0.5f) * 32;
                        int it = 0;
                        while ((std::fabs(tcx - walker.x) > 2 || std::fabs(tcy - walker.y) > 2) && it < 600) { walker.move(tcx - walker.x, tcy - walker.y, 0.016f, col); ++it; }
                        if (it >= 600) { ok2 = false; break; }
                    }
                    if (!ok2) std::cerr << "STUCK walking to " << o.name << " in " << id << "\n";
                    CHECK(ok2);
                }
            }
            for (const auto& o : mm.objects()) {                     // walking through any door lands somewhere valid
                if (o.type != "door") continue;
                TiledMap dest = TiledMap::fromFile(std::string(ASSET_DIR) + "/maps/" + o.prop("to") + ".json");
                TileMap dcol = dest.collisionMap();
                auto ap = arrivalPoint(dest, dcol, id, probe);
                CHECK(!probe.collides(ap.first, ap.second, dcol));
            }
        }
        // Every person in the story is somewhere on a map, exactly once (nobody missing, nobody duplicated).
        for (const auto& kv : ro.suspects()) {
            int count = 0;
            for (const auto& loc : ro.locations()) {
                TiledMap m2 = TiledMap::fromFile(std::string(ASSET_DIR) + "/maps/" + loc.first + ".json");
                for (const auto& o : m2.objects()) if (o.type == "npc" && o.prop("suspect_id") == kv.first) ++count;
            }
            if (count != 1) std::cerr << "PERSON " << kv.first << " appears " << count << " times\n";
            CHECK(count == 1);
        }
        // Interaction rules
        Game ig(ro);
        TiledMap hallMap = TiledMap::fromFile(std::string(ASSET_DIR) + "/maps/hall.json");
        const MapObject* near = nearestInteractable(hallMap, ig, 4.5f * 32, 7.5f * 32);   // just below the dining table
        CHECK(near != nullptr && near->name == "Birthday table");
        CHECK(nearestInteractable(hallMap, ig, 10.5f * 32, 12.5f * 32) == nullptr);       // nothing near the start spot
        TiledMap libMap = TiledMap::fromFile(std::string(ASSET_DIR) + "/maps/library.json");
        const MapObject* bookcase = nullptr;
        for (const auto& o : libMap.objects()) if (o.name == "Third bookcase") bookcase = &o;
        CHECK(bookcase != nullptr && !objectActive(*bookcase, ig));                         // the secret door is invisible...
        CHECK(ig.move("library") == Result::Ok && ig.collect("c_scene") == Result::Ok);
        CHECK(ig.move("hall") == Result::Ok && ig.present("s_shanta", 0, "c_scene").kind == Present::Exposed);   // the cook's confession opens the attic
        CHECK(ig.move("attic") == Result::Ok && ig.collect("c_blueprint") == Result::Ok);
        CHECK(bookcase != nullptr && objectActive(*bookcase, ig));                          // ...until the blueprint is found
        const MapObject* scene = nullptr;
        for (const auto& o : libMap.objects()) if (o.name == "The armchair") scene = &o;
        CHECK(scene != nullptr && !objectActive(*scene, ig));                               // collected clues disappear
    }

    if (failures == 0) std::cout << "All tests passed.\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
