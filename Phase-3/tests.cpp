#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <queue>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
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
#include "Rating.h"
#include "gui/Interact.h"
#include "gui/MapCheck.h"
#include "gui/Player.h"
#include "gui/AuthRules.h"
#include "gui/Menu.h"
#include "gui/Sfx.h"
#include "gui/Cinematic.h"
#include "gui/Cutscene.h"
#include "gui/Sprites.h"

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

    // Undo rule (Phase 3): moves and room pick-ups are undone; earned clues and exposed lies are not.
    {
        Case rc = makeRastogiCase();
        Game u(rc);
        CHECK(u.undo() == Result::NothingToUndo);                       // fresh game: nothing to undo
        CHECK(u.move("library") == Result::Ok);
        CHECK(u.collect("c_scene") == Result::Ok);
        CHECK(u.undo() == Result::Ok && !u.has("c_scene"));             // a room pick-up is undone first (LIFO)
        CHECK(u.undo() == Result::Ok && u.location() == "hall");        // then the move
        CHECK(u.undo() == Result::NothingToUndo);
        // A clue earned from a conversation is never taken back by undo.
        CHECK(u.move("library") == Result::Ok);
        CHECK(u.press("s_reddy", 0).revealed == "c_body" && u.has("c_body"));
        CHECK(u.undo() == Result::Ok && u.location() == "hall");        // only the move was recorded
        CHECK(u.has("c_body"));                                         // the earned clue stays
        CHECK(u.undo() == Result::NothingToUndo);
        // Undo never un-exposes a lie.
        CHECK(u.move("library") == Result::Ok);
        CHECK(u.collect("c_mill_report") == Result::Ok);
        CHECK(u.present("s_rohit", 0, "c_mill_report").kind == Present::Exposed);
        CHECK(u.undo() == Result::Ok && !u.has("c_mill_report"));       // the pick-up is undone
        CHECK(u.exposed("s_rohit", 0) && u.has("c_rohit_testimony"));   // the exposed lie and the confession stay
    }

    // Snapshot and restore (Phase 3): the game state as plain data round-trips exactly.
    {
        Case rc = makeRastogiCase();
        Game a(rc);
        CHECK(a.move("library") == Result::Ok);
        CHECK(a.collect("c_scene") == Result::Ok);
        CHECK(a.press("s_reddy", 0).revealed == "c_body");
        CHECK(a.present("s_rohit", 0, "c_scene").kind == Present::Wrong);          // costs a heart
        CHECK(a.collect("c_mill_report") == Result::Ok);
        CHECK(a.present("s_rohit", 0, "c_mill_report").kind == Present::Exposed);
        const Snapshot snap = a.snapshot();
        CHECK(snap.location == "library" && snap.credibility == 5);
        CHECK(snap.found.size() == a.dossier().size());

        Game b(rc);
        CHECK(b.restore(snap));
        CHECK(b.snapshot() == snap);                                               // identical state
        CHECK(b.location() == "library" && b.credibility() == 5);
        CHECK(b.dossier() == a.dossier());                                         // same clues, same order
        CHECK(b.has("c_scene") && b.has("c_body") && b.has("c_rohit_testimony"));
        CHECK(b.exposed("s_rohit", 0) && b.pressed("s_reddy", 0));
        CHECK(b.undo() == Result::NothingToUndo);                                  // undo history is not saved
        CHECK(b.collect("c_cheque") == Result::Ok);                                // and play continues normally
        CHECK(a.snapshot() == snap);                                               // the original is unchanged
    }
    // A snapshot that does not fit the case is refused and changes nothing.
    {
        Case rc = makeRastogiCase();
        Game g(rc);
        CHECK(g.move("library") == Result::Ok && g.collect("c_scene") == Result::Ok);
        const Snapshot before = g.snapshot();
        Snapshot bad = before;
        bad.location = "dungeon";                         CHECK(!g.restore(bad));      // unknown room
        bad = before; bad.found.push_back("c_nope");      CHECK(!g.restore(bad));      // unknown clue
        bad = before; bad.found.push_back("c_scene");     CHECK(!g.restore(bad));      // clue listed twice
        bad = before; bad.credibility = -1;               CHECK(!g.restore(bad));
        bad = before; bad.credibility = 7;                CHECK(!g.restore(bad));
        bad = before; bad.exposed.insert("nokey");        CHECK(!g.restore(bad));      // not a statement key
        bad = before; bad.exposed.insert("s_nobody#0");   CHECK(!g.restore(bad));      // unknown suspect
        bad = before; bad.pressed.insert("s_rohit#99");   CHECK(!g.restore(bad));      // statement does not exist
        bad = before; bad.pressed.insert("s_rohit#x");    CHECK(!g.restore(bad));      // number is not a number
        CHECK(g.snapshot() == before);                    // every refused restore left the game unchanged
        const Snapshot fresh = Game(rc).snapshot();
        CHECK(fresh.location == rc.start() && fresh.found.empty() && fresh.credibility == 6);
    }

    // Log-in rules (Phase 3): usernames and passwords.
    {
        using namespace AuthRules;
        CHECK(normalizeUsername("  Alice ") == "alice");                                  // trimmed, lower case
        CHECK(checkUsername("alice") == Problem::None && checkUsername("a_1") == Problem::None);
        CHECK(checkUsername("ab") == Problem::UsernameTooShort);
        CHECK(checkUsername(std::string(20, 'a')) == Problem::None);
        CHECK(checkUsername(std::string(21, 'a')) == Problem::UsernameTooLong);
        CHECK(checkUsername("a b") == Problem::UsernameBadCharacter);                     // no spaces inside
        CHECK(checkUsername("a-b") == Problem::UsernameBadCharacter);
        CHECK(checkUsername("a'b") == Problem::UsernameBadCharacter);
        CHECK(checkUsername("caf\xC3\xA9") == Problem::UsernameBadCharacter);             // letters outside A-Z
        CHECK(checkUsername("x'; DROP TABLE users;--") != Problem::None);
        CHECK(checkPassword("12345") == Problem::PasswordTooShort);
        CHECK(checkPassword("123456") == Problem::None);
        CHECK(checkPassword(std::string(64, 'x')) == Problem::None);
        CHECK(checkPassword(std::string(65, 'x')) == Problem::PasswordTooLong);
        CHECK(std::string(describe(Problem::None)).empty());
        for (Problem p : {Problem::UsernameTooShort, Problem::UsernameTooLong, Problem::UsernameBadCharacter,
                          Problem::PasswordTooShort, Problem::PasswordTooLong})
            CHECK(std::string(describe(p)).size() > 10);                                  // every problem has a message
    }

    // Menu logic (Phase 3): the cursor wraps around and skips items that are switched off.
    {
        Menu m(std::vector<MenuItem>{{"New Game"}, {"Continue", false}, {"Help"}, {"Settings"}, {"Quit"}});
        CHECK(m.index() == 0 && m.selected().label == "New Game");
        m.next(); CHECK(m.index() == 2);                       // skips the switched-off "Continue"
        m.next(); m.next(); CHECK(m.index() == 4);             // Quit
        m.next(); CHECK(m.index() == 0);                       // wraps to the top
        m.prev(); CHECK(m.index() == 4);                       // and back to the bottom
        m.prev(); m.prev(); CHECK(m.index() == 2);             // going up also skips "Continue"
        m.setEnabled(1, true);                                 // a save appears: Continue can be chosen
        m.prev(); CHECK(m.index() == 1);
        m.setEnabled(1, false);                                // ...and the cursor leaves it when it is switched off
        CHECK(m.index() == 0);
        m.setLabel(3, "Settings (changed)");
        CHECK(m.items()[3].label == "Settings (changed)");
        Menu first_off(std::vector<MenuItem>{{"A", false}, {"B"}, {"C"}});
        CHECK(first_off.index() == 1);                         // starts on the first item that is switched on
        Menu all_off(std::vector<MenuItem>{{"A", false}, {"B", false}});
        all_off.next(); all_off.prev();                        // must not loop forever
        CHECK(all_off.index() == 0);
        Menu none;
        none.next(); none.prev();                              // an empty menu is harmless
        CHECK(none.empty());
        Menu one(std::vector<MenuItem>{{"Only"}});
        one.next(); CHECK(one.index() == 0);
        one.prev(); CHECK(one.index() == 0);
    }

    // Revision counter (Phase 3): it goes up when the state really changes, and only then. The window autosaves on it.
    {
        Case rc = makeRastogiCase();
        Game g(rc);
        CHECK(g.revision() == 0);
        CHECK(g.move("attic") == Result::Locked && g.revision() == 0);              // a refused move changes nothing
        CHECK(g.move("library") == Result::Ok);
        unsigned long r = g.revision();
        CHECK(r > 0);
        CHECK(g.collect("c_body") == Result::WrongPlace && g.revision() == r);      // refused: no change
        CHECK(g.collect("c_scene") == Result::Ok && g.revision() > r);
        r = g.revision();
        CHECK(g.collect("c_scene") == Result::AlreadyHave && g.revision() == r);
        CHECK(g.present("s_rohit", 0, "c_mill_report").kind == Present::NotHeld && g.revision() == r);   // not held: no change
        CHECK(g.present("s_rohit", 0, "c_scene").kind == Present::Wrong && g.revision() > r);            // a lost heart
        r = g.revision();
        CHECK(g.press("s_reddy", 0).valid && g.revision() > r);                      // pressed, and a clue earned
        r = g.revision();
        CHECK(g.press("s_reddy", 0).valid && g.revision() == r);                     // pressing again: nothing new
        g.penalize(0);
        CHECK(g.revision() == r);                                                    // no penalty, no change
        CHECK(g.undo() == Result::Ok && g.revision() > r);
        r = g.revision();
        CHECK(g.collect("c_mill_report") == Result::Ok);
        CHECK(g.present("s_rohit", 0, "c_mill_report").kind == Present::Exposed);
        CHECK(g.revision() > r);
        Game h(rc);
        CHECK(h.revision() == 0);
        CHECK(h.restore(g.snapshot()) && h.revision() > 0);                          // a restore counts as a change
        Snapshot bad = g.snapshot(); bad.location = "dungeon";
        const unsigned long before = h.revision();
        CHECK(!h.restore(bad) && h.revision() == before);                            // a refused restore does not
    }

    // Sound files (Phase 3): each sound exists, is a valid 16-bit mono WAV, is audible but not clipped,
    // and the ambient loop has no seam (its last sample leads smoothly into its first).
    {
        std::set<std::string> names;
        for (size_t i = 0; i < std::size(kAllSfx); ++i) {
            const Sfx sfx = kAllSfx[i];
            CHECK(static_cast<size_t>(sfx) == i);                                       // the order matches the enum
            CHECK(names.insert(sfxFile(sfx)).second);                                   // every sound has its own file
            CHECK(sfxGain(sfx) > 0.0 && sfxGain(sfx) <= 1.0);
            std::ifstream f(std::string(ASSET_DIR) + "/sounds/" + sfxFile(sfx), std::ios::binary);
            if (sfxOptional(sfx) && !f) continue;                                       // an optional file that is not there
            CHECK(f.good());
            if (!f) continue;
            const std::vector<char> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            auto u16 = [&](size_t o) { return static_cast<unsigned>(static_cast<unsigned char>(b[o]) | (static_cast<unsigned char>(b[o + 1]) << 8)); };
            auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };
            CHECK(b.size() > 44 && std::string(b.data(), 4) == "RIFF" && std::string(b.data() + 8, 4) == "WAVE");
            CHECK(std::string(b.data() + 12, 4) == "fmt " && std::string(b.data() + 36, 4) == "data");
            CHECK(u16(20) == 1 && u16(22) == 1 && u32(24) == 22050 && u16(34) == 16);     // PCM, mono, 22050 Hz, 16 bit
            const size_t n = u32(40) / 2;
            CHECK(n > 0 && b.size() >= 44 + n * 2);
            if (n == 0 || b.size() < 44 + n * 2) continue;
            std::vector<int> x(n);
            double peak = 0, sum = 0;
            for (size_t k = 0; k < n; ++k) {
                x[k] = static_cast<std::int16_t>(u16(44 + 2 * k));
                peak = std::max(peak, std::fabs(static_cast<double>(x[k])));
                sum += static_cast<double>(x[k]) * x[k];
            }
            const double seconds = static_cast<double>(n) / 22050.0, rms = std::sqrt(sum / static_cast<double>(n));
            CHECK(peak > 0.05 * 32768 && peak < 0.99 * 32768);                          // audible, not clipped
            CHECK(rms > 0.01 * 32768);                                                  // not nearly silent
            CHECK(seconds >= 0.05 && seconds <= 40.0);
            if (sfx == Sfx::Ambient) {
                CHECK(seconds >= 10.0);                                                 // a long loop, so it does not feel repetitive
                CHECK(std::abs(x[n - 1] - x[0]) < 0.05 * peak);                         // no seam where the loop restarts
                int maxStep = 0;
                for (size_t k = 1; k < n; ++k) maxStep = std::max(maxStep, std::abs(x[k] - x[k - 1]));
                CHECK(maxStep < 0.1 * 32768);                                           // no clicks inside the loop
            } else {
                CHECK(std::abs(x[n - 1]) < 0.02 * 32768);                               // every sound ends in silence
            }
        }
    }

    // Music and effects are told apart, and the theme is long enough to carry the opening.
    CHECK(sfxIsMusic(Sfx::Theme) && sfxIsMusic(Sfx::Ambient) && !sfxIsMusic(Sfx::Exposed) && !sfxIsMusic(Sfx::Step));
    CHECK(sfxOptional(Sfx::Voice) && !sfxOptional(Sfx::Theme));
    CHECK(kVoiceLeadMs >= 500 && kVoiceLeadMs <= 4000);                     // the sting waits for the spoken line

    // Cinematic timing (Phase 3): cards fade in and out, the text types out, Enter completes it and then moves on.
    {
        Sequence s(std::vector<Card>{{"K", "One", "Hello there", 3.f}, {"", "Two", "", 2.f}});
        CHECK(!s.done() && s.index() == 0 && s.count() == 2 && s.alpha() == 0.f);
        s.update(0.3f);
        CHECK(s.alpha() > 0.45f && s.alpha() < 0.55f);                      // fading in
        CHECK(s.visibleChars() == 0);                                       // the typing starts after a short pause
        s.update(0.2f);
        CHECK(s.visibleChars() > 0 && s.visibleChars() < 11);              // part of "Hello there"
        s.next();
        CHECK(s.index() == 0 && s.visibleChars() == 11);                    // Enter first shows the whole text
        s.next();
        CHECK(s.index() == 1 && s.time() == 0.f);                           // then moves to the next card
        s.update(1.0f);
        CHECK(s.alpha() == 1.f);
        s.update(0.7f);
        CHECK(s.alpha() < 0.6f);                                            // fading out near the end
        s.update(0.5f);
        CHECK(s.done() && s.alpha() == 0.f && s.visibleChars() == 0);
        Sequence k(std::vector<Card>{{"", "A", "", 1.f}, {"", "B", "", 1.f}});
        k.update(2.5f);
        CHECK(k.done());                                                    // a long frame can pass several cards
        Sequence sk(std::vector<Card>{{"", "A", "", 5.f}});
        sk.skipAll();
        CHECK(sk.done() && sk.card().title.empty());                        // Esc skips everything
        Sequence none;
        none.update(1.f); none.next();
        CHECK(none.done());                                                 // an empty sequence is harmless

        Timer t;
        CHECK(!t.active() && t.remaining() == 0.f && t.envelope(0.1f, 0.1f) == 0.f);
        t.start(2.f); t.update(0.5f);
        CHECK(t.active() && std::fabs(t.progress() - 0.25f) < 1e-5f && std::fabs(t.remaining() - 0.75f) < 1e-5f);
        CHECK(std::fabs(t.envelope(1.f, 1.f) - 0.5f) < 1e-5f);             // half way through the fade-in
        t.update(1.0f);
        CHECK(std::fabs(t.envelope(1.f, 1.f) - 0.5f) < 1e-5f);             // half way through the fade-out
        t.update(1.0f);
        CHECK(!t.active() && t.envelope(0.2f, 0.2f) == 0.f);

        Shake sh;
        CHECK(sh.x() == 0.f && sh.y() == 0.f && !sh.active());
        sh.start(10.f, 0.5f); sh.update(0.01f);
        CHECK(sh.active() && std::fabs(sh.x()) <= 10.f && std::fabs(sh.y()) <= 10.f);
        sh.update(0.4f);
        CHECK(std::fabs(sh.x()) <= 2.0f + 1e-4f && std::fabs(sh.y()) <= 2.0f + 1e-4f);   // it dies away
        sh.update(0.2f);
        CHECK(!sh.active() && sh.x() == 0.f && sh.y() == 0.f);
    }

    // The ending's stamp lands once, and the story types out (Phase 3).
    {
        CHECK(std::fabs(stampScale(0.f) - 2.4f) < 1e-5f);                    // starts large
        CHECK(stampScale(0.2f) < stampScale(0.1f) && stampScale(0.2f) > 1.f); // shrinks smoothly
        CHECK(stampScale(0.45f) == 1.f && stampScale(5.f) == 1.f);            // and lands at its size
        const std::string text = "Case closed.";
        CHECK(typedChars(text, 0.5f, 1.0f) == 0);                             // nothing before the delay
        CHECK(typedChars(text, 1.1f, 1.0f) == 4);                             // 0.1 s at 45 letters a second
        CHECK(typedChars(text, 1000.f, 1.0f) == text.size());                 // Enter shows it all
    }

    // Objectives (Phase 3): a queue of steps; the front is the current objective; finished steps are ticked off in order.
    {
        Case rc = makeRastogiCase();
        Game g(rc);
        ObjectiveTracker t(makeRastogiObjectives());
        using V = std::vector<size_t>;
        CHECK(t.total() == 15 && t.completed() == 0 && t.currentIndex() == 0 && t.hasCurrent());
        CHECK(t.current().text.find("Dr. Reddy") != std::string::npos);
        for (size_t i = 0; i < t.total(); ++i) {
            CHECK(!t.at(i).text.empty() && t.at(i).isDone);
            CHECK(t.at(i).place.empty() || rc.location(t.at(i).place) != nullptr);   // every place is a real room
        }
        CHECK(t.update(g).empty());                                           // nothing done yet
        CHECK(g.move("library") == Result::Ok && t.update(g).empty());
        CHECK(g.press("s_reddy", 0).revealed == "c_body" && t.update(g) == V{0});
        CHECK(g.completeLab("lab_tod") == Result::Ok && t.update(g) == V{1});
        CHECK(g.collect("c_scene") == Result::Ok && g.collect("c_mill_report") == Result::Ok);
        CHECK(t.update(g).empty());                                           // the library is not fully searched yet
        CHECK(g.collect("c_cheque") == Result::Ok && t.update(g) == V{2});
        CHECK(g.present("s_rohit", 0, "c_mill_report").kind == Present::Exposed && t.update(g) == V{3});
        CHECK(g.move("hall") == Result::Ok && g.move("kitchen") == Result::Ok);
        CHECK(g.present("s_shanta", 0, "c_scene").kind == Present::Exposed && t.update(g) == V{4});
        CHECK(g.move("hall") == Result::Ok && g.present("s_meera", 0, "c_shanta_testimony").kind == Present::Exposed);
        CHECK(t.update(g) == V{5});
        CHECK(g.move("family_wing") == Result::Ok && g.collect("c_betting") == Result::Ok);
        CHECK(g.present("s_kabir", 0, "c_betting").kind == Present::Exposed && t.update(g) == V{6});
        CHECK(g.move("hall") == Result::Ok && g.move("blue_room") == Result::Ok);
        CHECK(g.collect("c_pen") == Result::Ok && g.collect("c_wife_text") == Result::Ok && t.update(g) == V{7});
        CHECK(g.move("hall") == Result::Ok && g.move("attic") == Result::Ok);
        CHECK(g.collect("c_blueprint") == Result::Ok && t.update(g) == V{8});
        CHECK(g.collect("c_nameplate") == Result::Ok && t.update(g) == V{9});
        CHECK(g.move("hall") == Result::Ok && g.move("library") == Result::Ok && g.move("passage") == Result::Ok);
        CHECK(g.collect("c_cufflink") == Result::Ok && t.update(g).empty());
        CHECK(g.collect("c_strongroom_open") == Result::Ok && t.update(g) == V{10});
        CHECK(g.collect("c_forged_fd") == Result::Ok && g.collect("c_diary") == Result::Ok && g.collect("c_new_will") == Result::Ok);
        CHECK(t.update(g) == V{11});
        CHECK(g.completeLab("lab_ink") == Result::Ok && t.update(g) == V{12});
        CHECK(g.present("s_raghav", 0, "c_wife_text").kind == Present::Exposed && t.update(g).empty());   // one lie is not enough
        CHECK(g.present("s_raghav", 1, "c_cufflink").kind == Present::Exposed && t.update(g) == V{13});
        CHECK(t.currentIndex() == 14 && t.current().text.find("Accuse") != std::string::npos);
        CHECK(t.completed() == 14 && !t.allDone() && t.isDone(0) && t.isDone(13) && !t.isDone(14));

        // Out of order: steps finished early wait in the queue, then are ticked off together, in order.
        Game o(rc);
        ObjectiveTracker u(makeRastogiObjectives());
        CHECK(o.move("library") == Result::Ok && o.collect("c_scene") == Result::Ok);
        CHECK(o.collect("c_mill_report") == Result::Ok && o.collect("c_cheque") == Result::Ok);
        CHECK(u.update(o).empty() && u.completed() == 0 && !u.isDone(2));    // done, but not yet at the front
        CHECK(o.press("s_reddy", 0).valid && u.update(o) == V{0});
        CHECK(o.completeLab("lab_tod") == Result::Ok && u.update(o) == (V{1, 2}));
        CHECK(u.currentIndex() == 3);

        // After a Continue or an undo the tracker is rebuilt from the game state, without ticking anything twice.
        u.reset();
        CHECK(u.completed() == 0 && u.currentIndex() == 0);
        CHECK(u.update(o) == (V{0, 1, 2}) && u.update(o).empty());
        ObjectiveTracker none;
        CHECK(none.total() == 0 && !none.hasCurrent() && none.allDone() && none.update(o).empty());
    }

    // The detective's rating, the lantern flicker and the dust (Phase 3 polish).
    {
        Case rc = makeRastogiCase();
        Game fresh(rc);
        const Rating r0 = rateDetective(fresh, rc);
        CHECK(r0.score == 40 && r0.grade == 'C');                           // full credibility, nothing found yet
        Game sloppy(rc);
        CHECK(sloppy.move("library") == Result::Ok && sloppy.collect("c_scene") == Result::Ok);
        for (int i = 0; i < 5; ++i) CHECK(sloppy.present("s_rohit", 1, "c_scene").kind == Present::Wrong);   // wrong evidence, again and again
        CHECK(rateDetective(sloppy, rc).score < r0.score && rateDetective(sloppy, rc).grade == 'D');
        Game good(rc);                                                      // find everything, catch every lie, no mistakes
        CHECK(good.move("library") == Result::Ok && good.press("s_reddy", 0).valid && good.press("s_reddy", 1).valid);
        CHECK(good.completeLab("lab_tod") == Result::Ok);
        for (const char* id : {"c_scene", "c_mill_report", "c_cheque"}) CHECK(good.collect(id) == Result::Ok);
        CHECK(good.present("s_rohit", 0, "c_mill_report").kind == Present::Exposed);
        CHECK(good.move("hall") == Result::Ok && good.collect("c_dinner") == Result::Ok && good.move("kitchen") == Result::Ok);
        CHECK(good.collect("c_milk_jug") == Result::Ok && good.present("s_shanta", 0, "c_scene").kind == Present::Exposed);
        CHECK(good.move("hall") == Result::Ok && good.present("s_meera", 0, "c_shanta_testimony").kind == Present::Exposed);
        CHECK(good.move("family_wing") == Result::Ok && good.collect("c_betting") == Result::Ok);
        CHECK(good.present("s_kabir", 0, "c_betting").kind == Present::Exposed);
        CHECK(good.move("hall") == Result::Ok && good.move("blue_room") == Result::Ok);
        CHECK(good.collect("c_pen") == Result::Ok && good.collect("c_wife_text") == Result::Ok);
        CHECK(good.move("hall") == Result::Ok && good.move("attic") == Result::Ok);
        CHECK(good.collect("c_blueprint") == Result::Ok && good.collect("c_nameplate") == Result::Ok);
        CHECK(good.move("hall") == Result::Ok && good.move("library") == Result::Ok && good.move("passage") == Result::Ok);
        for (const char* id : {"c_cufflink", "c_strongroom_open", "c_forged_fd", "c_diary", "c_new_will"}) CHECK(good.collect(id) == Result::Ok);
        CHECK(good.completeLab("lab_ink") == Result::Ok);
        CHECK(good.present("s_raghav", 0, "c_wife_text").kind == Present::Exposed);
        CHECK(good.present("s_raghav", 1, "c_cufflink").kind == Present::Exposed);
        CHECK(good.present("s_raghav", 2, "c_ink_match").kind == Present::Exposed);
        const Rating rb = rateDetective(good, rc);
        CHECK(rb.score > 80 && (rb.grade == 'S' || rb.grade == 'A') && !rb.title.empty());

        for (float t = 0.f; t < 20.f; t += 0.37f) CHECK(lanternFlicker(t) > 0.95f && lanternFlicker(t) < 1.05f);
        DustField d(50, 7u), e(50, 7u);
        CHECK(d.motes().size() == 50);
        for (int i = 0; i < 200; ++i) { d.update(0.5f); e.update(0.5f); }
        bool inside = true, same = true, rising = false;
        for (size_t i = 0; i < d.motes().size(); ++i) {
            const Mote& m = d.motes()[i];
            inside = inside && m.x >= 0.f && m.x < 1.f && m.y >= 0.f && m.y < 1.f;
            same = same && m.x == e.motes()[i].x && m.y == e.motes()[i].y;
            rising = rising || m.vy < 0.f;
            CHECK(DustField::twinkle(m, 3.f) >= 0.f && DustField::twinkle(m, 3.f) <= 1.f);
        }
        CHECK(inside && same && rising);                                    // stays on screen, repeatable, drifts up
        CHECK(DustField(0).motes().empty());
    }

    // The opening scene's engine (Phase 3): characters walk between keys, cues fire once, Enter and Esc work.
    {
        Track t{"rohit", {{1.f, 0, 0}, {3.f, 100, 0}, {4.f, 100, 0, 3}, {5.f, 100, 50}}};
        Pose p0 = poseAt(t, 0.f);
        CHECK(p0.x == 0.f && !p0.moving);                                  // waits at the first key
        Pose p1 = poseAt(t, 2.f);
        CHECK(std::fabs(p1.x - 50.f) < 1e-4f && p1.moving && p1.facing == 2);   // half way, walking right
        Pose p2 = poseAt(t, 3.5f);
        CHECK(p2.x == 100.f && !p2.moving && p2.facing == 2);              // standing, still facing the way he walked
        Pose p3 = poseAt(t, 4.5f);
        CHECK(p3.moving && p3.facing == 0 && std::fabs(p3.y - 25.f) < 1e-4f);   // walking down
        Pose p4 = poseAt(t, 9.f);
        CHECK(p4.x == 100.f && p4.y == 50.f && !p4.moving);
        CHECK(facingFor(-5, 1) == 1 && facingFor(5, 1) == 2 && facingFor(1, -5) == 3 && facingFor(1, 5) == 0);
        Track still{"dayanand", {{0.f, 10, 20, 0}}};
        CHECK(poseAt(still, 7.f).x == 10.f && poseAt(still, 7.f).facing == 0 && poseAt(Track{}, 1.f).x == 0.f);

        Shot a; a.seconds = 2.f; a.cues = {{0.f, "start"}, {1.f, "mid"}, {2.f, "end"}}; a.lines = {{0.5f, 1.5f, "A", "hello", ""}};
        Shot b; b.seconds = 1.f; b.cues = {{0.5f, "b"}};
        Cutscene cs(std::vector<Shot>{a, b});
        CHECK(!cs.done() && cs.count() == 2 && cs.line() == nullptr);
        CHECK(cs.update(0.1f) == std::vector<std::string>{"start"});        // a cue at 0 fires too
        CHECK(cs.update(0.5f).empty() && cs.line() && cs.line()->text == "hello");
        CHECK(cs.update(0.5f) == std::vector<std::string>{"mid"});
        CHECK(cs.update(5.0f) == std::vector<std::string>{"end"} && cs.index() == 1);   // a long frame: the last cue still fires once
        cs.nextShot();
        CHECK(cs.done() && cs.update(1.f).empty() && cs.line() == nullptr);
        Shot v; v.seconds = 2.f; v.lines = {{0.5f, 1.0f, "A", "x", "v_a"}, {1.2f, 1.8f, "B", "y", ""}};
        Cutscene vc(std::vector<Shot>{v});
        CHECK(vc.update(0.4f).empty());
        CHECK(vc.update(0.2f) == std::vector<std::string>{"voice:v_a"});     // the voice starts with its subtitle
        CHECK(vc.update(1.0f).empty());                                       // once only; a line with no voice is silent
        Cutscene sk(std::vector<Shot>{a, b});
        sk.skipAll();
        CHECK(sk.done() && sk.shot().room.empty());

        // The real opening scene: every room, sprite and sound exists, subtitles fit their shot, cameras are sensible.
        Case rc = makeRastogiCase();
        const std::vector<Shot> scene = makeOpeningCutscene();
        CHECK(scene.size() == 4);
        float total = 0.f;
        for (const Shot& sh : scene) {
            total += sh.seconds;
            CHECK(rc.location(sh.room) != nullptr && sh.seconds > 0.f);
            CHECK(sh.follow < static_cast<int>(sh.tracks.size()));
            for (const Track& tr : sh.tracks) {
                CHECK(spriteExists(tr.sprite) && !tr.keys.empty());
                for (size_t k = 1; k < tr.keys.size(); ++k) CHECK(tr.keys[k].t >= tr.keys[k - 1].t);   // keys in time order
                for (const Key& k : tr.keys) CHECK(k.t <= sh.seconds && k.x >= 0.f && k.y >= 0.f && k.x <= 800.f && k.y <= 600.f);
            }
            for (const Line& l : sh.lines) CHECK(l.from < l.to && l.to <= sh.seconds && !l.text.empty() && !l.speaker.empty());
            for (const Cue& c : sh.cues) {
                Sfx any;
                CHECK(c.name == "shake" || c.name == "music:ambient" || c.name.rfind("voice:", 0) == 0 || sfxFromName(c.name, any));
            }
            for (const Line& l : sh.lines) CHECK(!l.voice.empty() && l.voice.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos);
        }
        CHECK(total > 30.f && total < 75.f);                                   // long enough to matter, short enough to enjoy
        for (const Shot& sh : scene) {
            for (const Line& l : sh.lines) {
                std::ifstream vf(std::string(ASSET_DIR) + "/sounds/voices/" + l.voice + ".wav", std::ios::binary);
                CHECK(vf.good());                                               // the recording is there
                if (!vf) continue;
                vf.seekg(0, std::ios::end);
                const double secs = (static_cast<double>(vf.tellg()) - 44.0) / (2.0 * 22050.0);
                CHECK(secs <= l.to - l.from + 0.6);                             // the subtitle stays up while it is spoken
                for (const Line& other : sh.lines)                              // the next spoken line waits for this one
                    if (other.from > l.from) CHECK(other.from >= l.from + secs - 0.35);
            }
        }
        CHECK(spriteIndex("dayanand") == 7 && spriteIndex("detective") == 0 && spriteIndex("nobody") == 1);
    }

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
