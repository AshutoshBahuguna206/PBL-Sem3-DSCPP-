#include <iostream>
#include <sstream>
#include "Game.h"
#include "SampleCase.h"
#include "CaseMumbai.h"
#include "CaseRastogi.h"

static const char* msg(Result r) {
    switch (r) {
        case Result::Ok: return "Done.";
        case Result::Unknown: return "No such thing.";
        case Result::NotAdjacent: return "You can't get there from here.";
        case Result::Locked: return "It's locked. You need something first.";
        case Result::WrongPlace: return "That isn't here.";
        case Result::MissingPrereq: return "You don't yet know enough to make sense of that.";
        case Result::AlreadyHave: return "You already have that.";
        case Result::NothingToUndo: return "Nothing to undo.";
    }
    return "";
}

int main(int argc, char** argv) {
    std::string which = argc > 1 ? argv[1] : "rastogi";   // rastogi (default), mumbai, tutorial
    Case c = which == "tutorial" ? makeSampleCase() : which == "mumbai" ? makeMumbaiCase() : makeRastogiCase();
    auto report = validate(c);
    if (!report.ok()) { for (auto& e : report.errors) std::cerr << "CASE ERROR: " << e << "\n"; return 1; }

    Game g(c);
    std::cout << "== " << c.title() << " ==\nCommands: look, go <id>, take <id>, undo, dossier, suspects, accuse <id>, quit\n";
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd, arg;
        in >> cmd >> arg;
        if (cmd == "quit") break;
        else if (cmd == "look") {
            auto* l = c.location(g.location());
            std::cout << l->name() << ": " << l->description() << "\nExits:";
            for (auto& e : l->exits()) std::cout << " " << e;
            std::cout << "\nClues here:";
            for (auto& [id, k] : c.clues()) if (!k.granted() && k.location() == g.location() && !g.has(id) && prerequisitesMet(k, g.heldSet())) std::cout << " " << id;
            std::cout << "\n";
        }
        else if (cmd == "go") std::cout << msg(g.move(arg)) << "\n";
        else if (cmd == "take") std::cout << msg(g.collect(arg)) << "\n";
        else if (cmd == "undo") std::cout << msg(g.undo()) << "\n";
        else if (cmd == "dossier") for (auto& id : g.dossier()) std::cout << "- " << c.clue(id)->name() << ": " << c.clue(id)->description() << "\n";
        else if (cmd == "suspects") for (auto& [id, s] : c.suspects()) std::cout << id << " (" << s.name() << "): \"" << s.alibi() << "\"\n";
        else if (cmd == "accuse") {
            switch (g.accuse(arg)) {
                case Verdict::Solved: std::cout << "CASE SOLVED! The proof holds up.\n"; return 0;
                case Verdict::WrongSuspect: std::cout << "Wrong suspect. Keep investigating.\n"; break;
                case Verdict::InsufficientEvidence: std::cout << "Right instinct, but you can't prove it yet.\n"; break;
                case Verdict::UnknownSuspect: std::cout << "No such suspect.\n"; break;
            }
        }
        else std::cout << "Unknown command.\n";
    }
}
