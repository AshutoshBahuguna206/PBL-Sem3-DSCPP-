#pragma once
// The timing behind the game's cinematic touches: title cards and story cards with fades and a typewriter, a screen
// shake, and simple timers for flashes, captions and fades. Plain C++ with no Qt, so it is tested with the engine tests;
// the game window only draws what these say.
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

struct Card {
    std::string kicker;   // a small line above the title, for example a place and a time
    std::string title;
    std::string body;     // typed out letter by letter
    float seconds = 4.f;  // how long the card stays (the player can also move on with Enter)
};

// A sequence of cards that fade in, hold and fade out, one after another.
class Sequence {
public:
    static constexpr float kFade = 0.6f;             // seconds to fade in, and to fade out
    static constexpr float kTypeDelay = 0.35f;       // the body starts typing after this
    static constexpr float kCharsPerSecond = 42.f;

    Sequence() = default;
    explicit Sequence(std::vector<Card> cards) : cards_(std::move(cards)) {}

    void update(float dt) {
        if (done()) return;
        t_ += std::max(0.f, dt);
        while (!done() && t_ >= cards_[i_].seconds) { t_ -= cards_[i_].seconds; ++i_; }
    }
    // Enter: if the card is still typing, show it all at once; otherwise go to the next card.
    void next() {
        if (done()) return;
        if (visibleChars() < cards_[i_].body.size()) { t_ = std::max(t_, typingEnd()); return; }
        ++i_; t_ = 0.f;
    }
    void skipAll() { i_ = cards_.size(); }
    bool done() const { return i_ >= cards_.size(); }
    size_t index() const { return i_; }
    size_t count() const { return cards_.size(); }
    float time() const { return t_; }
    const Card& card() const { static const Card none; return done() ? none : cards_[i_]; }

    // 0 to 1: fades in at the start of a card and out at its end.
    float alpha() const {
        if (done()) return 0.f;
        const float s = cards_[i_].seconds;
        return std::clamp(std::min(t_ / kFade, (s - t_) / kFade), 0.f, 1.f);
    }
    // How many letters of the body are visible so far (the typewriter).
    size_t visibleChars() const {
        if (done()) return 0;
        const float n = (t_ - kTypeDelay) * kCharsPerSecond;
        if (n <= 0.f) return 0;
        return std::min(cards_[i_].body.size(), static_cast<size_t>(n));
    }

private:
    float typingEnd() const { return kTypeDelay + static_cast<float>(cards_[i_].body.size()) / kCharsPerSecond + 0.01f; }
    std::vector<Card> cards_;
    size_t i_ = 0;
    float t_ = 0.f;
};

// A timer that runs from 0 to 1 over a duration: used for flashes, captions, banners and fades.
class Timer {
public:
    void start(float seconds) { dur_ = std::max(0.001f, seconds); t_ = 0.f; on_ = true; }
    void update(float dt) { if (!on_) return; t_ += std::max(0.f, dt); if (t_ >= dur_) { t_ = dur_; on_ = false; } }
    bool active() const { return on_; }
    float progress() const { return on_ ? std::clamp(t_ / dur_, 0.f, 1.f) : 1.f; }
    float remaining() const { return on_ ? 1.f - progress() : 0.f; }        // 1 at the start, 0 at the end
    // 0 to 1: rises over fadeIn seconds, holds, and falls over the last fadeOut seconds.
    float envelope(float fadeIn, float fadeOut) const {
        if (!on_) return 0.f;
        const float a = fadeIn > 0.f ? t_ / fadeIn : 1.f, b = fadeOut > 0.f ? (dur_ - t_) / fadeOut : 1.f;
        return std::clamp(std::min(a, b), 0.f, 1.f);
    }
private:
    float dur_ = 1.f, t_ = 0.f;
    bool on_ = false;
};

// A screen shake that dies away: two sine waves at different speeds, scaled by the time left.
class Shake {
public:
    void start(float amplitude, float seconds) { amp_ = amplitude; timer_.start(seconds); }
    void update(float dt) { timer_.update(dt); time_ += dt; }
    bool active() const { return timer_.active(); }
    float x() const { return active() ? amp_ * timer_.remaining() * std::sin(time_ * 61.f) : 0.f; }
    float y() const { return active() ? amp_ * timer_.remaining() * std::sin(time_ * 47.f + 1.3f) : 0.f; }
private:
    Timer timer_;
    float amp_ = 0.f, time_ = 0.f;
};

// The ending's rubber stamp: it starts large and lands at its normal size after `land` seconds.
inline float stampScale(float t, float land = 0.45f) {
    if (t >= land) return 1.f;
    const float u = 1.f - std::max(0.f, t) / land;
    return 1.f + 1.4f * u * u * u;
}

// How many letters of a text a typewriter has shown after t seconds (it starts after `delay`).
inline size_t typedChars(const std::string& text, float t, float delay, float charsPerSecond = 45.f) {
    const float n = (t - delay) * charsPerSecond;
    if (n <= 0.f) return 0;
    return std::min(text.size(), static_cast<size_t>(n));
}

// The lantern's light breathes a little, like a flame: a size factor between about 0.96 and 1.04.
inline float lanternFlicker(float t) {
    return 1.f + 0.025f * std::sin(t * 7.3f) + 0.015f * std::sin(t * 13.1f + 1.f);
}

// Dust drifting slowly upwards through the lantern light. Positions are fractions of the screen (0 to 1) and wrap
// round at the edges. A small fixed-seed random generator makes the field the same every time (and testable).
struct Mote {
    float x = 0.f, y = 0.f, vx = 0.f, vy = 0.f, size = 1.f, phase = 0.f;
};

class DustField {
public:
    explicit DustField(int count = 70, unsigned seed = 1921u) : state_(seed) {
        motes_.reserve(static_cast<size_t>(std::max(0, count)));
        for (int i = 0; i < count; ++i) {
            Mote m;
            m.x = rand01(); m.y = rand01();
            m.vx = (rand01() - 0.5f) * 0.012f;
            m.vy = -(0.004f + rand01() * 0.010f);            // drifting up
            m.size = 1.f + rand01() * 1.6f;
            m.phase = rand01() * 6.2832f;
            motes_.push_back(m);
        }
    }
    void update(float dt) {
        for (Mote& m : motes_) { m.x = wrap01(m.x + m.vx * dt); m.y = wrap01(m.y + m.vy * dt); }
    }
    const std::vector<Mote>& motes() const { return motes_; }
    // 0 to 1: how bright a mote is at time t (they glint slowly as they turn).
    static float twinkle(const Mote& m, float t) { return 0.5f + 0.5f * std::sin(t * 1.7f + m.phase); }

private:
    float rand01() { state_ = state_ * 1664525u + 1013904223u; return static_cast<float>(state_ >> 8) / 16777216.f; }
    static float wrap01(float v) { v -= std::floor(v); return v >= 1.f ? 0.f : v; }
    unsigned state_;
    std::vector<Mote> motes_;
};
