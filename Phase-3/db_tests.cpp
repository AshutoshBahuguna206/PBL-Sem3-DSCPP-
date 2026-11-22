// Tests for the database layer. Needs Qt (QtSql with its SQLite driver). Run it on your machine:
// build the db_tests target in Qt Creator and run it; the last line must say "Database tests passed."
#include <QCoreApplication>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include "CaseRastogi.h"
#include "Game.h"
#include "gui/Database.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::cerr << "FAIL line " << __LINE__ << ": " #cond "\n"; failures++; } } while (0)

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const std::string path = (std::filesystem::temp_directory_path() / "dossier_db_test.db").string();
    std::remove(path.c_str());
    const QString qpath = QString::fromStdString(path);
    const std::string caseId = "the_old_way";

    Case rc = makeRastogiCase();
    Game a(rc);                                   // play a little, so there is something to save
    CHECK(a.move("library") == Result::Ok);
    CHECK(a.collect("c_scene") == Result::Ok);
    CHECK(a.press("s_reddy", 0).revealed == "c_body");
    CHECK(a.present("s_rohit", 0, "c_scene").kind == Present::Wrong);
    CHECK(a.collect("c_mill_report") == Result::Ok);
    CHECK(a.present("s_rohit", 0, "c_mill_report").kind == Present::Exposed);
    const Snapshot snap = a.snapshot();

    int alice = 0, bob = 0;
    {
        Database db;
        CHECK(db.open(qpath));
        CHECK(db.isOpen());

        // Users: a name can be taken only once, and awkward text is stored as plain text.
        auto u = db.createUser("alice", "salt-a", "hash-a");
        CHECK(u.has_value() && *u >= 1);
        alice = u.value_or(0);
        CHECK(!db.createUser("alice", "other", "other").has_value());            // UNIQUE: name already taken
        auto found = db.findUser("alice");
        CHECK(found && found->id == alice && found->salt == "salt-a" && found->hash == "hash-a");
        CHECK(!db.findUser("nobody").has_value());
        const std::string nasty = "x'; DROP TABLE users;--";
        CHECK(db.createUser(nasty, "s", "h").has_value());                        // bound as text, never run as SQL
        CHECK(db.findUser(nasty).has_value() && db.findUser("alice").has_value());
        bob = db.createUser("bob", "salt-b", "hash-b").value_or(0);
        CHECK(bob > alice);

        // Save and load: the snapshot and the position come back exactly.
        CHECK(!db.hasSave(alice, caseId));
        CHECK(db.save(alice, caseId, snap, 123.5f, 456.25f));
        CHECK(db.hasSave(alice, caseId));
        auto loaded = db.load(alice, caseId);
        CHECK(loaded.has_value());
        if (loaded) {
            CHECK(loaded->snapshot == snap);
            CHECK(loaded->x == 123.5f && loaded->y == 456.25f);
            Game b(rc);
            CHECK(b.restore(loaded->snapshot));                                   // the saved state is playable
            CHECK(b.snapshot() == snap);
        }

        // Saving again replaces the old save (nothing is duplicated), and keeps the dossier order.
        CHECK(a.collect("c_cheque") == Result::Ok);
        const Snapshot snap2 = a.snapshot();
        CHECK(db.save(alice, caseId, snap2, 10.f, 20.f));
        auto loaded2 = db.load(alice, caseId);
        CHECK(loaded2 && loaded2->snapshot == snap2 && loaded2->snapshot.found.size() == snap.found.size() + 1);
        CHECK(loaded2 && loaded2->x == 10.f && loaded2->y == 20.f);

        // Saves belong to their user.
        CHECK(!db.hasSave(bob, caseId) && !db.load(bob, caseId).has_value());
        CHECK(db.save(bob, caseId, Game(rc).snapshot(), 1.f, 2.f));
        auto bobs = db.load(bob, caseId);
        CHECK(bobs && bobs->snapshot.found.empty());
        CHECK(db.load(alice, caseId)->snapshot == snap2);                         // alice's save is untouched

        // A save for a user who does not exist is refused, and leaves nothing behind.
        CHECK(!db.save(9999, caseId, snap, 0.f, 0.f));
        CHECK(!db.hasSave(9999, caseId));
    }
    {
        // The data survives closing and reopening the file.
        Database db;
        CHECK(db.open(qpath));
        auto again = db.load(alice, caseId);
        CHECK(again.has_value() && again->snapshot.location == "library");
        CHECK(db.findUser("alice").has_value());

        // Deleting a save removes it (and deleting a missing one is not an error).
        CHECK(db.deleteSave(alice, caseId));
        CHECK(!db.hasSave(alice, caseId) && !db.load(alice, caseId).has_value());
        CHECK(db.deleteSave(alice, caseId));
        CHECK(db.hasSave(bob, caseId));
    }
    {
        // The window's autosave rule: save whenever the engine's revision changes. Continue restores it; finishing deletes it.
        Database db;
        CHECK(db.open(qpath));
        const int carol = db.createUser("carol", "s", "h").value_or(0);
        CHECK(carol > 0);
        Game g(rc);
        unsigned long saved = g.revision();
        int saves = 0;
        auto autosave = [&] {
            if (g.revision() != saved) { CHECK(db.save(carol, caseId, g.snapshot(), 5.f, 6.f)); saved = g.revision(); ++saves; }
        };
        autosave();
        CHECK(saves == 0 && !db.hasSave(carol, caseId));                       // a fresh game has nothing to save
        CHECK(g.move("library") == Result::Ok);
        autosave();
        CHECK(saves == 1 && db.hasSave(carol, caseId));
        autosave();
        CHECK(saves == 1);                                                     // nothing changed: no extra save
        CHECK(g.collect("c_scene") == Result::Ok);
        CHECK(g.present("s_rohit", 0, "c_scene").kind == Present::Wrong);
        autosave();
        CHECK(saves == 2);
        Game again(rc);                                                        // "Continue" in a new session
        auto loaded = db.load(carol, caseId);
        CHECK(loaded.has_value() && again.restore(loaded->snapshot));
        CHECK(again.snapshot() == g.snapshot() && again.credibility() == 5);
        CHECK(db.deleteSave(carol, caseId) && !db.hasSave(carol, caseId));     // the case is finished
    }
    std::remove(path.c_str());

    if (failures == 0) std::cout << "Database tests passed.\n";
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
