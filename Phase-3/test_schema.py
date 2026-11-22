#!/usr/bin/env python3
"""Tests the database's SQL against a real SQLite engine, using the very same statements the game uses.
Run:  python3 tools/test_schema.py        (needs only Python; sqlite3 is built in)
The statements are read from src/gui/DbSql.h, so what is tested is exactly what ships."""
import re, sqlite3, pathlib, sys

root = pathlib.Path(__file__).resolve().parent.parent
text = (root / "src/gui/DbSql.h").read_text()
SQL = {}
for m in re.finditer(r'inline constexpr const char\* (\w+) =\s*(?:R"SQL\((.*?)\)SQL"|"((?:[^"\\]|\\.)*)")\s*;', text, re.S):
    SQL[m.group(1)] = (m.group(2) if m.group(2) is not None else m.group(3)).strip()

failures = 0
def check(cond, label):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", label)

expected = ["kSqlPragmaForeignKeys", "kSqlCreateUsers", "kSqlCreateSaves", "kSqlCreateItems", "kSqlInsertUser", "kSqlSelectUser",
            "kSqlDeleteItems", "kSqlDeleteSave", "kSqlInsertSave", "kSqlInsertItem", "kSqlSelectSave", "kSqlSelectItems", "kSqlHasSave"]
check(all(k in SQL for k in expected), "all 13 statements were found in DbSql.h")

def fresh():
    db = sqlite3.connect(":memory:", isolation_level=None)   # we control transactions ourselves
    db.execute(SQL["kSqlPragmaForeignKeys"])
    for k in ("kSqlCreateUsers", "kSqlCreateSaves", "kSqlCreateItems"):
        db.execute(SQL[k])
    return db

def save(db, uid, case, loc, cred, x, y, clues, exposed, pressed):
    db.execute("BEGIN")
    try:
        db.execute(SQL["kSqlDeleteItems"], (uid, case)); db.execute(SQL["kSqlDeleteSave"], (uid, case))
        db.execute(SQL["kSqlInsertSave"], (uid, case, loc, cred, x, y))
        for i, c in enumerate(clues): db.execute(SQL["kSqlInsertItem"], (uid, case, "clue", c, i))
        for i, k in enumerate(sorted(exposed)): db.execute(SQL["kSqlInsertItem"], (uid, case, "exposed", k, i))
        for i, k in enumerate(sorted(pressed)): db.execute(SQL["kSqlInsertItem"], (uid, case, "pressed", k, i))
        db.execute("COMMIT"); return True
    except sqlite3.Error:
        db.execute("ROLLBACK"); return False

def load(db, uid, case):
    row = db.execute(SQL["kSqlSelectSave"], (uid, case)).fetchone()
    if not row: return None
    items = db.execute(SQL["kSqlSelectItems"], (uid, case)).fetchall()
    return {"loc": row[0], "cred": row[1], "x": row[2], "y": row[3],
            "clues": [v for k, v in items if k == "clue"],
            "exposed": {v for k, v in items if k == "exposed"}, "pressed": {v for k, v in items if k == "pressed"}}

db = fresh()
# tables created, and creating them again is harmless
for k in ("kSqlCreateUsers", "kSqlCreateSaves", "kSqlCreateItems"): db.execute(SQL[k])
names = {r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
check({"users", "saves", "save_items"} <= names, "three tables exist")

# users: unique names, bound values, injection-looking text stays text
db.execute(SQL["kSqlInsertUser"], ("alice", "salt-a", "hash-a"))
uid = db.execute(SQL["kSqlSelectUser"], ("alice",)).fetchone()
check(uid and uid[1:] == ("salt-a", "hash-a"), "user can be found with its salt and hash")
try: db.execute(SQL["kSqlInsertUser"], ("alice", "x", "y")); dup = False
except sqlite3.IntegrityError: dup = True
check(dup, "a duplicate username is refused (UNIQUE)")
nasty = "x'; DROP TABLE users;--"
db.execute(SQL["kSqlInsertUser"], (nasty, "s", "h"))
check(db.execute(SQL["kSqlSelectUser"], (nasty,)).fetchone() is not None, "injection-looking username stored as plain text")
check("users" in {r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}, "users table still exists")
check(db.execute(SQL["kSqlSelectUser"], ("nobody",)).fetchone() is None, "unknown user is not found")
alice = uid[0]
bob = db.execute(SQL["kSqlInsertUser"], ("bob", "sb", "hb")).lastrowid

# saves: round trip, order of clues, replace, separate users
state = dict(loc="library", cred=5, x=123.5, y=456.25, clues=["c_scene", "c_body", "c_mill_report", "c_rohit_testimony"],
             exposed={"s_rohit#0"}, pressed={"s_reddy#0", "s_rohit#0"})
check(save(db, alice, "case1", state["loc"], state["cred"], state["x"], state["y"], state["clues"], state["exposed"], state["pressed"]), "save succeeds")
check(db.execute(SQL["kSqlHasSave"], (alice, "case1")).fetchone() is not None, "hasSave is true after saving")
check(load(db, alice, "case1") == state, "load returns exactly what was saved, with the clues in their original order")
state2 = dict(state, clues=state["clues"] + ["c_cheque"], x=10.0, y=20.0)
check(save(db, alice, "case1", state2["loc"], state2["cred"], state2["x"], state2["y"], state2["clues"], state2["exposed"], state2["pressed"]), "second save succeeds")
check(load(db, alice, "case1") == state2, "saving again replaces the old save")
check(db.execute("SELECT COUNT(*) FROM save_items WHERE user_id=? AND kind='clue'", (alice,)).fetchone()[0] == 5, "no duplicate items after a re-save")
check(load(db, bob, "case1") is None, "another user has no save")
check(save(db, bob, "case1", "hall", 6, 1.0, 2.0, [], set(), set()), "another user can save")
check(load(db, alice, "case1") == state2, "the first user's save is untouched")
check(load(db, alice, "other_case") is None, "saves are per case")

# foreign key: a save for a missing user is refused and leaves nothing
check(not save(db, 9999, "case1", "hall", 6, 0.0, 0.0, ["c_x"], set(), set()), "save for a missing user is refused")
check(db.execute(SQL["kSqlHasSave"], (9999, "case1")).fetchone() is None and
      db.execute("SELECT COUNT(*) FROM save_items WHERE user_id=9999").fetchone()[0] == 0, "nothing was left behind")

# atomic: a failure in the middle rolls everything back and keeps the previous save
db.execute("BEGIN")
try:
    db.execute(SQL["kSqlDeleteItems"], (alice, "case1")); db.execute(SQL["kSqlDeleteSave"], (alice, "case1"))
    db.execute(SQL["kSqlInsertSave"], (alice, "case1", "attic", 1, 0.0, 0.0))
    db.execute(SQL["kSqlInsertItem"], (alice, "case1", "not_a_kind", "v", 0))      # violates the CHECK
    db.execute("COMMIT")
except sqlite3.Error:
    db.execute("ROLLBACK")
check(load(db, alice, "case1") == state2, "a failed save rolls back and the old save is intact")

# delete: removes the save and its items
db.execute("BEGIN"); db.execute(SQL["kSqlDeleteItems"], (alice, "case1")); db.execute(SQL["kSqlDeleteSave"], (alice, "case1")); db.execute("COMMIT")
check(load(db, alice, "case1") is None and db.execute("SELECT COUNT(*) FROM save_items WHERE user_id=?", (alice,)).fetchone()[0] == 0, "delete removes the save and its items")
check(load(db, bob, "case1") is not None, "deleting one user's save keeps the other's")

# deleting a user cascades to their saves
db.execute("DELETE FROM users WHERE id=?", (bob,))
check(db.execute("SELECT COUNT(*) FROM saves WHERE user_id=?", (bob,)).fetchone()[0] == 0, "deleting a user removes their saves (cascade)")

print("Schema tests passed." if failures == 0 else f"{failures} schema test(s) FAILED")
sys.exit(1 if failures else 0)
