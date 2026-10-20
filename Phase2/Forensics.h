#pragma once
// The science puzzles. Pure maths (no Qt), so they are unit-tested and the screen only has to show them.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

// ---- Biology: how long ago did he die? (body cooling)
// A living body is about 37 C. After death it cools about 1 C per hour (a classroom rule of thumb).
struct TodPuzzle {
    double bodyTempC = 29.0;      // measured by the doctor
    double normalTempC = 37.0;
    double coolingPerHour = 1.0;
    int examMinute = 7 * 60 + 15; // minutes after midnight when it was measured (7:15 am)
    int toleranceMin = 45;        // answers within 45 minutes count
};

inline int todAnswerMinute(const TodPuzzle& p) {
    const double hours = (p.normalTempC - p.bodyTempC) / p.coolingPerHour;
    int m = (p.examMinute - static_cast<int>(std::lround(hours * 60))) % 1440;
    return m < 0 ? m + 1440 : m;
}
inline bool todCorrect(const TodPuzzle& p, int guessMinute) {
    int d = std::abs(guessMinute - todAnswerMinute(p));
    d = std::min(d, 1440 - d);
    return d <= p.toleranceMin;
}

// ---- Chemistry: whose pen wrote the forged signature? (paper chromatography)
// Rf = distance moved by the ink spot / distance moved by the solvent. The same ink always gives the same Rf.
struct InkSample {
    std::string label;
    double spotCm, frontCm;
    double rf() const { return spotCm / frontCm; }
};
struct InkPuzzle {
    InkSample forged;
    std::vector<InkSample> pens;
};

// Index of the single pen whose Rf matches the forged ink (within tolerance), or -1 if none / ambiguous.
inline int inkMatch(const InkPuzzle& p, double tol = 0.005) {
    int found = -1;
    for (size_t i = 0; i < p.pens.size(); ++i)
        if (std::fabs(p.pens[i].rf() - p.forged.rf()) <= tol) { if (found >= 0) return -1; found = static_cast<int>(i); }
    return found;
}

inline InkPuzzle rastogiInkPuzzle() {
    InkPuzzle p;
    p.forged = {"Forged signature on the certificates", 4.26, 6.0};      // Rf 0.71
    p.pens = {{"Dayanand's own pen (cheque book)", 3.10, 5.0},            // Rf 0.62
              {"Rohit's pen (mill report)", 3.90, 6.0},                   // Rf 0.65
              {"Raghav's pen (writing set)", 5.68, 8.0}};                 // Rf 0.71
    return p;
}
