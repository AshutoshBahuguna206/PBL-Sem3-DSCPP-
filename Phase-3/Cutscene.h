#pragma once
// A small engine for scenes played inside the game's own rooms: characters walk between timed positions, subtitles
// appear, and sound cues fire at set moments. Plain C++ with no Qt, so it is tested with the engine tests; the game
// window only loads each shot's room and draws what this says.
#include <cmath>
#include <string>
#include <utility>
#include <vector>

struct Key {
    float t = 0.f;      // seconds into the shot
    float x = 0.f;      // where the character's feet are, in map pixels
    float y = 0.f;
    int face = -1;      // which way to face when standing here (0 down, 1 left, 2 right, 3 up); -1 = keep walking direction
};

struct Track {
    std::string sprite;          // which character (see Sprites.h)
    std::vector<Key> keys;       // in time order; the character walks in straight lines between them
};

struct Pose {
    float x = 0.f, y = 0.f;
    int facing = 0;
    bool moving = false;
};

struct Line {
    float from = 0.f, to = 0.f;  // when the subtitle is on screen
    std::string speaker, text;
    std::string voice;           // the recorded line (assets/sounds/voices/<voice>.wav) played when it appears, or ""
};

struct Cue {
    float at = 0.f;              // when it fires
    std::string name;            // a sound's name; "shake"; "voice:<id>" for a recorded reaction; "music:ambient"
};

struct Shot {
    std::string room;            // the location id whose map is shown
    float seconds = 5.f;
    std::string caption;         // a line in the top bar (a time and a place), or empty
    float camX = 0.f, camY = 0.f;// where the camera looks, unless it follows someone
    int follow = -1;             // the index of a track for the camera to follow, or -1
    std::vector<Track> tracks;
    std::vector<Line> lines;
    std::vector<Cue> cues;
};

// The direction a sprite faces when moving by (dx, dy): 0 down, 1 left, 2 right, 3 up.
inline int facingFor(float dx, float dy) {
    if (std::fabs(dx) > std::fabs(dy)) return dx < 0.f ? 1 : 2;
    return dy < 0.f ? 3 : 0;
}

// Where a character is at time t: before the first key it waits at the first key; between keys it walks in a straight
// line; after the last key it stands at the last one.
inline Pose poseAt(const Track& tr, float t) {
    Pose p;
    if (tr.keys.empty()) return p;
    const Key& first = tr.keys.front();
    if (t <= first.t) { p.x = first.x; p.y = first.y; p.facing = first.face >= 0 ? first.face : 0; return p; }
    int lastDir = 0;
    for (size_t i = 0; i + 1 < tr.keys.size(); ++i) {
        const Key& a = tr.keys[i];
        const Key& b = tr.keys[i + 1];
        const bool walks = (a.x != b.x || a.y != b.y);
        if (walks) lastDir = facingFor(b.x - a.x, b.y - a.y);
        if (t < b.t) {
            const float u = (b.t > a.t) ? (t - a.t) / (b.t - a.t) : 1.f;
            p.x = a.x + (b.x - a.x) * u;
            p.y = a.y + (b.y - a.y) * u;
            p.moving = walks;
            p.facing = walks ? lastDir : (a.face >= 0 ? a.face : lastDir);
            return p;
        }
    }
    const Key& last = tr.keys.back();
    p.x = last.x; p.y = last.y;
    p.facing = last.face >= 0 ? last.face : lastDir;
    return p;
}

// Plays a list of shots in order. update() advances the time and returns the cues that fired, so each fires once.
class Cutscene {
public:
    Cutscene() = default;
    explicit Cutscene(std::vector<Shot> shots) : shots_(std::move(shots)) {}

    std::vector<std::string> update(float dt) {
        std::vector<std::string> fired;
        if (done()) return fired;
        t_ += dt > 0.f ? dt : 0.f;
        const float end = std::fmin(t_, shots_[i_].seconds);
        for (const Cue& c : shots_[i_].cues) if (c.at > firedTo_ && c.at <= end) fired.push_back(c.name);
        for (const Line& l : shots_[i_].lines)                 // a subtitle's recorded voice starts with it
            if (!l.voice.empty() && l.from > firedTo_ && l.from <= end) fired.push_back("voice:" + l.voice);
        firedTo_ = end;
        if (t_ >= shots_[i_].seconds) startShot(i_ + 1);
        return fired;
    }
    void nextShot() { if (!done()) startShot(i_ + 1); }     // Enter: on to the next shot
    void skipAll() { i_ = shots_.size(); }                  // Esc: skip the whole scene
    bool done() const { return i_ >= shots_.size(); }
    size_t index() const { return i_; }
    size_t count() const { return shots_.size(); }
    float time() const { return t_; }
    const Shot& shot() const { static const Shot none; return done() ? none : shots_[i_]; }
    // The subtitle on screen now, or nullptr.
    const Line* line() const {
        if (done()) return nullptr;
        for (const Line& l : shots_[i_].lines) if (t_ >= l.from && t_ < l.to) return &l;
        return nullptr;
    }

private:
    void startShot(size_t i) { i_ = i; t_ = 0.f; firedTo_ = -1.f; }
    std::vector<Shot> shots_;
    size_t i_ = 0;
    float t_ = 0.f;
    float firedTo_ = -1.f;      // cues up to this time have already fired (-1 so a cue at 0 fires too)
};

// ---------------------------------------------------------------------------------------------------------------
// The opening scene: the morning after the birthday. Positions are feet positions in each room's map (32-pixel tiles).
// ---------------------------------------------------------------------------------------------------------------
inline std::vector<Shot> makeOpeningCutscene() {
    std::vector<Shot> s;

    // 1. The hall, 6:50 am. Shanta Bai carries the morning tea from the kitchen to the library door and knocks.
    Shot a;
    a.room = "hall"; a.seconds = 12.3f; a.caption = "6:50 AM   |   THE MORNING AFTER THE BIRTHDAY"; a.follow = 0;
    a.tracks = {{"shanta", {{0.f, 700, 368}, {5.6f, 60, 368, 1}}}};
    a.cues = {{6.0f, "knock"}};
    a.lines = {{6.9f, 11.7f, "Shanta Bai", "Sahab? Your tea... Sahab, are you awake?", "v01_shanta_tea"}};
    s.push_back(a);

    // 2. The hall. The family comes running; the door is locked from inside; Rohit and Kabir break it down.
    Shot b;
    b.room = "hall"; b.seconds = 16.0f; b.camX = 230; b.camY = 290;
    b.tracks = {
        {"shanta", {{0.f, 60, 368, 3}, {4.6f, 60, 368, 3}, {5.4f, 140, 448, 1}}},
        {"rohit", {{0.8f, 176, 72}, {1.6f, 176, 150}, {2.6f, 290, 150}, {3.8f, 290, 360}, {5.0f, 92, 360, 1},
                   {14.05f, 92, 360, 1}, {14.4f, 62, 352, 1}}},
        {"kabir", {{1.6f, 176, 72}, {2.4f, 176, 150}, {3.4f, 290, 150}, {4.6f, 290, 392}, {5.8f, 100, 392, 1},
                   {14.05f, 100, 392, 1}, {14.4f, 70, 376, 1}}},
        {"meera", {{2.6f, 592, 72}, {3.4f, 592, 200}, {5.0f, 300, 200}, {6.0f, 300, 412}, {7.0f, 160, 420, 1}}},
        {"raghav", {{3.2f, 592, 72}, {4.0f, 592, 200}, {5.4f, 330, 200}, {6.4f, 330, 440}, {7.6f, 200, 448, 1}}},
    };
    b.lines = {
        {0.2f, 8.1f, "Shanta Bai", "Rohit baba! Kabir baba! The door is locked, and Sahab won't answer!", "v02_shanta_help"},
        {8.4f, 11.3f, "Rohit", "Papa? Papa, open the door!", "v03_rohit_papa"},
        {11.6f, 14.1f, "Rohit", "Kabir, help me. Now!", "v04_rohit_help"},
    };
    b.cues = {{0.8f, "running"}, {2.2f, "voice:r01_kabir_what"}, {4.4f, "voice:r02_meera_upstairs"},
              {5.4f, "chaos"},                                         // nine seconds that end exactly on the crash
              {8.4f, "pound"}, {10.0f, "voice:r03_raghav_calm"}, {14.4f, "crash"}, {14.4f, "shake"}};
    s.push_back(b);

    // 3. The library. Dayanand in his armchair. Dr. Reddy comes through.
    Shot c;
    c.room = "library"; c.seconds = 16.2f; c.caption = "THE LIBRARY"; c.camX = 420; c.camY = 280;
    c.tracks = {
        {"dayanand", {{0.f, 320, 244, 0}}},
        // in through the door, along the clear row inside it, then up to the armchair (avoiding the green chair)
        {"rohit", {{0.f, 630, 304}, {0.3f, 585, 336}, {1.3f, 392, 336}, {1.6f, 392, 282, 3}}},
        {"kabir", {{0.4f, 630, 304}, {0.7f, 585, 336}, {2.0f, 300, 336}, {2.1f, 300, 316, 3}}},
        {"meera", {{0.8f, 630, 304}, {1.1f, 585, 336}, {2.6f, 268, 336}, {2.9f, 268, 282, 2}}},
        {"raghav", {{1.4f, 630, 304}, {1.7f, 585, 336}, {2.6f, 400, 336}, {2.8f, 400, 300, 1}}},
        {"shanta", {{2.0f, 630, 304}, {2.3f, 585, 336}, {2.6f, 560, 370, 1}}},
        {"reddy", {{3.6f, 630, 304}, {3.9f, 585, 336}, {5.0f, 340, 336}, {5.3f, 340, 282, 3}}},
    };
    c.cues = {{1.0f, "discovery"}, {1.3f, "voice:r04_all_gasp"}, {3.2f, "voice:r05_shanta_hairam"},
              {4.3f, "voice:r06_meera_sobbing"}, {9.7f, "voice:r06_meera_sobbing"}};
    c.lines = {
        {2.4f, 3.9f, "Meera", "Papa...!", "v05_meera_papa"},
        {5.6f, 9.1f, "Dr. Reddy", "Let me through. I'm a doctor.", "v06_reddy_through"},
        {9.4f, 12.3f, "Dr. Reddy", "He's gone. He has been dead for hours.", "v07_reddy_gone"},
        {12.6f, 15.6f, "Raghav", "I'll call the police. Nobody touch anything.", "v08_raghav_police"},
    };
    s.push_back(c);

    // 4. The hall, 7:40 am. The road is gone; the inspector (you) arrives on foot. Raghav greets you.
    Shot d;
    d.room = "hall"; d.seconds = 19.4f; d.caption = "7:40 AM   |   THE ROAD IS GONE. ONE INSPECTOR COMES ON FOOT.";
    d.camX = 400; d.camY = 410;
    d.tracks = {
        {"detective", {{0.6f, 400, 536}, {2.8f, 400, 470, 3}}},
        {"raghav", {{0.f, 420, 330, 0}, {3.4f, 420, 330, 0}, {4.4f, 404, 420, 0}}},
        {"meera", {{0.f, 350, 340, 0}}},
        {"rohit", {{0.f, 470, 350, 0}}},
        {"kabir", {{0.f, 616, 400, 1}}},
        {"shanta", {{0.f, 318, 404, 0}}},
        {"reddy", {{0.f, 486, 418, 0}}},
    };
    d.lines = {
        {4.6f, 10.5f, "Raghav", "Inspector, thank God you made it. I am Raghav Deshpande, the family's lawyer.", "v09_raghav_welcome"},
        {10.8f, 18.6f, "Raghav", "Dayanand is dead, and the road is gone. If someone in this house did it, I trust you will find out who.", "v10_raghav_trust"},
    };
    d.cues = {{0.0f, "music:ambient"}};
    s.push_back(d);
    return s;
}
