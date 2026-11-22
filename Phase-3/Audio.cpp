#include "Audio.h"
#include <QDir>
#include <QFileInfo>
#include <QSoundEffect>
#include <QUrl>

Audio::Audio(const QString& soundsDir) {
    for (Sfx s : kAllSfx) {
        const QString path = soundsDir + "/" + QString::fromUtf8(sfxFile(s));
        if (!QFileInfo::exists(path)) { fx_.push_back(nullptr); continue; }   // an optional file that is not there
        auto e = std::make_unique<QSoundEffect>();
        e->setSource(QUrl::fromLocalFile(path));
        if (sfxIsMusic(s)) e->setLoopCount(QSoundEffect::Infinite);
        fx_.push_back(std::move(e));
    }
    // Load every recorded voice up front, so a line plays the moment its subtitle appears.
    const QDir voices(soundsDir + "/voices");
    for (const QString& file : voices.entryList(QStringList{"*.wav"}, QDir::Files)) {
        auto e = std::make_unique<QSoundEffect>();
        e->setSource(QUrl::fromLocalFile(voices.filePath(file)));
        voices_[QFileInfo(file).completeBaseName().toStdString()] = std::move(e);
    }
    applyVolumes();
}

Audio::~Audio() = default;

void Audio::applyVolumes() {
    for (Sfx s : kAllSfx) {
        if (QSoundEffect* e = fx(s)) {
            e->setMuted(muted_);
            e->setVolume(static_cast<float>(sfxGain(s) * volume_ / 100.0));
        }
    }
    for (auto& entry : voices_) {
        entry.second->setMuted(muted_);
        entry.second->setVolume(static_cast<float>(volume_ / 100.0));
    }
}

void Audio::playVoice(const std::string& id) {
    const auto it = voices_.find(id);
    if (it != voices_.end()) it->second->play();
}

void Audio::stopVoices() {
    for (auto& entry : voices_) entry.second->stop();
}

void Audio::play(Sfx s) {
    if (sfxIsMusic(s)) { playMusic(s); return; }
    if (QSoundEffect* e = fx(s)) e->play();
}

void Audio::stop(Sfx s) {
    if (QSoundEffect* e = fx(s)) e->stop();
}

void Audio::playMusic(Sfx s) {
    if (musicOn_ && music_ == s) return;
    stopMusic();
    music_ = s;
    if (QSoundEffect* e = fx(s)) { e->play(); musicOn_ = true; }
}

void Audio::stopMusic() {
    if (QSoundEffect* e = fx(Sfx::Theme)) e->stop();
    if (QSoundEffect* e = fx(Sfx::Ambient)) e->stop();
    musicOn_ = false;
}

void Audio::setMuted(bool muted) { muted_ = muted; applyVolumes(); }
void Audio::setVolume(int percent) { volume_ = percent < 0 ? 0 : percent > 100 ? 100 : percent; applyVolumes(); }
