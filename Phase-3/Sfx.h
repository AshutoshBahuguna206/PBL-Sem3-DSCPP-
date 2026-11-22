#pragma once
// The game's sounds and music: one enum and the file of each (in assets/sounds). Plain C++ with no Qt, so the files
// can be tested with the engine tests. The files are made by tools/make_sounds.py (all original, synthesised in code).
enum class Sfx { Pickup, Door, Locked, Exposed, Wrong, LabSuccess, DialClick, Ending, GameOver, Ambient, Theme, Step, Voice, Objective, Knock, Crash, Discovery, Running, Pound, Chaos };

// Every sound, in the order of the enum (the audio class indexes by it).
inline constexpr Sfx kAllSfx[] = {Sfx::Pickup, Sfx::Door, Sfx::Locked, Sfx::Exposed, Sfx::Wrong, Sfx::LabSuccess,
                                  Sfx::DialClick, Sfx::Ending, Sfx::GameOver, Sfx::Ambient, Sfx::Theme, Sfx::Step, Sfx::Voice,
                                  Sfx::Objective, Sfx::Knock, Sfx::Crash, Sfx::Discovery,
                                  Sfx::Running, Sfx::Pound, Sfx::Chaos};

inline const char* sfxFile(Sfx s) {
    switch (s) {
        case Sfx::Pickup: return "pickup.wav";           // a two-note "clue found" cue
        case Sfx::Door: return "door.wav";               // a latch, a soft swing, a wooden knock
        case Sfx::Locked: return "locked.wav";           // a rattled handle
        case Sfx::Exposed: return "exposed.wav";         // "dun dun DUNNN": a lie is caught
        case Sfx::Wrong: return "wrong.wav";             // two falling brass notes: a mistake
        case Sfx::LabSuccess: return "lab_success.wav";  // a rising arpeggio and a brass chord: a breakthrough
        case Sfx::DialClick: return "dial_click.wav";    // a small metallic tick
        case Sfx::Ending: return "ending.wav";           // a fanfare: case closed
        case Sfx::GameOver: return "game_over.wav";      // a dark descent
        case Sfx::Ambient: return "ambient.wav";         // the investigation loop (music, loops with no seam)
        case Sfx::Theme: return "theme.wav";             // the opening theme (music)
        case Sfx::Step: return "step.wav";               // a soft footstep
        case Sfx::Voice: return "voice_gadbad.wav";      // OPTIONAL: your own recorded line, played when a lie is caught
        case Sfx::Objective: return "objective.wav";     // an objective is complete
        case Sfx::Knock: return "knock.wav";             // knocking on a wooden door
        case Sfx::Crash: return "crash.wav";             // a locked door broken open
        case Sfx::Discovery: return "discovery.wav";     // the moment the body is found
        case Sfx::Running: return "running.wav";         // several people running down the stairs and across the hall
        case Sfx::Pound: return "pound.wav";             // fists pounding on a locked door
        case Sfx::Chaos: return "chaos.wav";             // the tension score that builds until the door gives way
    }
    return "";
}

// When your voice line is present, it plays first and the "dun dun DUNNN" follows after this many milliseconds
// (the length of the spoken line). If you record a new line, set this to its length.
constexpr int kVoiceLeadMs = 1700;

// Music loops; effects play once.
inline bool sfxIsMusic(Sfx s) { return s == Sfx::Ambient || s == Sfx::Theme; }

// Optional files are not shipped; the game simply skips them when they are missing.
inline bool sfxOptional(Sfx s) { return s == Sfx::Voice; }

// How loud each sound is, relative to the master volume (0 to 1).
inline double sfxGain(Sfx s) {
    switch (s) {
        case Sfx::Pickup: return 0.80;
        case Sfx::Door: return 0.70;
        case Sfx::Locked: return 0.75;
        case Sfx::Exposed: return 1.00;
        case Sfx::Wrong: return 0.85;
        case Sfx::LabSuccess: return 0.90;
        case Sfx::DialClick: return 0.70;
        case Sfx::Ending: return 1.00;
        case Sfx::GameOver: return 1.00;
        case Sfx::Ambient: return 0.85;                  // the file itself is already quiet
        case Sfx::Theme: return 0.90;
        case Sfx::Step: return 0.30;                     // footsteps are felt more than heard
        case Sfx::Voice: return 1.00;
        case Sfx::Objective: return 0.70;
        case Sfx::Knock: return 0.85;
        case Sfx::Crash: return 1.00;
        case Sfx::Discovery: return 1.00;
        case Sfx::Running: return 0.80;
        case Sfx::Pound: return 0.95;
        case Sfx::Chaos: return 0.85;
    }
    return 1.0;
}

// A sound by the name used in a scene's cues ("knock", "crash", ...): its file name without ".wav".
#include <string>
inline bool sfxFromName(const std::string& name, Sfx& out) {
    for (Sfx s : kAllSfx) {
        const std::string f = sfxFile(s);
        if (f.size() > 4 && f.compare(0, f.size() - 4, name) == 0 && f.size() - 4 == name.size()) { out = s; return true; }
    }
    return false;
}
