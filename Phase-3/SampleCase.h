#pragma once
#include "Case.h"

// "The Midnight Library": culprit is Ms. Iyer. Solution path:
// lobby: keycard -> library: ledger -> study (needs keycard): camera log (needs ledger) breaks her alibi.
inline Case makeSampleCase() {
    Case c("The Midnight Library", "lobby", "s_iyer");
    c.add(Location("lobby",   "Lobby",   "Marble floor, a reception desk.", {"library", "garden"}));
    c.add(Location("library", "Library", "Tall shelves; the victim was found here.", {"lobby", "study"}));
    c.add(Location("study",   "Study",   "Locked private study.", {"library"}, "c_keycard"));
    c.add(Location("garden",  "Garden",  "Damp soil, a broken gate.", {"lobby"}));

    c.add(Clue("c_keycard", "Keycard",     "A staff keycard under the desk.", "lobby"));
    c.add(Clue("c_ledger",  "Entry ledger","Shows a visitor signed in at 9:30pm.", "library"));
    c.add(Clue("c_cctv",    "Study camera log", "Ms. Iyer enters the study at 9:40pm.", "study",
               {"c_ledger"}, "s_iyer"));
    c.add(Clue("c_boot",    "Muddy boot print", "Fresh print near the gate (a red herring).", "garden"));

    c.add(Suspect("s_rao",  "Mr. Rao",  "The butler.",     "I was in the kitchen all night."));
    c.add(Suspect("s_khan", "Dr. Khan", "Family doctor.",  "I was in the garden at 9:30pm."));
    c.add(Suspect("s_iyer", "Ms. Iyer", "The librarian.",  "I was in the library the whole evening."));
    return c;
}
