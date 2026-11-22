#pragma once
// Plays the game's sounds and music with Qt Multimedia's QSoundEffect. Effects are played from event handlers only
// (a door, a clue, a caught lie), never from paintEvent, which runs about 60 times a second. Needs Qt Multimedia.
#include <QString>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Sfx.h"

class QSoundEffect;

class Audio {
public:
    explicit Audio(const QString& soundsDir);
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    void play(Sfx s);                // play an effect once (music: see playMusic)
    void stop(Sfx s);
    void playMusic(Sfx s);           // switch the looping music (Theme or Ambient); nothing happens if it is already playing
    void stopMusic();
    void setMuted(bool muted);
    void setVolume(int percent);     // the master volume, 0 to 100
    bool has(Sfx s) const { return fx(s) != nullptr; }   // false if an optional file is missing
    // Recorded voices for scenes: every assets/sounds/voices/<id>.wav is loaded at start; a missing id is skipped.
    void playVoice(const std::string& id);
    void stopVoices();

private:
    void applyVolumes();
    QSoundEffect* fx(Sfx s) const { return fx_[static_cast<size_t>(s)].get(); }
    std::vector<std::unique_ptr<QSoundEffect>> fx_;   // one per sound, indexed by the Sfx value; empty if the file is missing
    std::unordered_map<std::string, std::unique_ptr<QSoundEffect>> voices_;   // recorded voices, by file name
    bool muted_ = false;
    int volume_ = 70;
    bool musicOn_ = false;
    Sfx music_ = Sfx::Theme;
};
