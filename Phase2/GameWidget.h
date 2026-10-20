#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QPixmap>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <memory>
#include <string>
#include <vector>
#include "CaseRastogi.h"
#include "Forensics.h"
#include "Game.h"
#include "Interact.h"
#include "Objectives.h"
#include "Player.h"
#include "TileMap.h"
#include "TiledMap.h"

class QPainter;
class QKeyEvent;
class QFocusEvent;
class QPaintEvent;

// The whole game screen. Game/Case hold the rules; this class draws the world, forwards key presses and shows
// the conversations (cross-examination), the evidence picker, the forensic lab, the dossier and the ending.
// Throws std::exception from the constructor if the assets cannot be loaded.
class GameWidget : public QWidget {
public:
    explicit GameWidget(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void keyReleaseEvent(QKeyEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    bool focusNextPrevChild(bool) override { return false; }  // lets the Tab key reach keyPressEvent

private:
    enum class Mode { Explore, Dossier, Accuse, Dial, Ending, Talk, Present, LabMenu, LabTod, LabInk, GameOver };

    // world
    void loadLocation(const Id& id, const Id& from);
    void refreshGates();
    void restart();
    void tick();
    // actions
    void interact();
    void startTalk(const MapObject& o);
    void showClue(const Id& clueId);
    void say(const QString& title, const QString& body);
    void checkFailure();
    // keys
    void handleDialKey(QKeyEvent* e);
    void handleAccuseKey(int key);
    void handleDossierKey(int key);
    void handleTalkKey(int key);
    void handlePresentKey(int key);
    void handleLabMenuKey(int key);
    void handleLabTodKey(int key);
    void handleLabInkKey(int key);
    // drawing
    void drawTile(QPainter& p, unsigned gid, const QRect& dst);
    void drawWorld(QPainter& p);
    void drawCharacter(QPainter& p, int sprite, int dir, int frame, float cx, float feetY);
    void drawHud(QPainter& p, const MapObject* target);
    void drawHearts(QPainter& p);
    void drawMessage(QPainter& p);
    void drawDossier(QPainter& p);
    void drawAccuse(QPainter& p);
    void drawDial(QPainter& p);
    void drawEnding(QPainter& p);
    void drawTalk(QPainter& p);
    void drawPresent(QPainter& p);
    void drawLabMenu(QPainter& p);
    void drawLabTod(QPainter& p);
    void drawLabInk(QPainter& p);
    void drawGameOver(QPainter& p);
    void panel(QPainter& p, const QRect& r);

    bool down(int k1, int k2) const { return keys_.contains(k1) || keys_.contains(k2); }
    bool hasMessage() const { return !msgTitle_.isEmpty() || !msgBody_.isEmpty(); }

    // NOTE: members are initialised in this order.
    Case case_;
    std::unique_ptr<Game> game_;
    TiledMap tiled_;
    TileMap collision_;
    QPixmap tileset_;
    QPixmap characters_;
    Player player_;
    QTimer timer_;
    QElapsedTimer clock_;
    QSet<int> keys_;
    Mode mode_ = Mode::Explore;
    bool debug_ = false;
    float elapsed_ = 0.f;
    QString msgTitle_, msgBody_;
    int dossierPage_ = 0, scroll_ = 0, contentH_ = 0, bodyH_ = 0;
    int accuseIdx_ = 0;
    QString dialInput_;
    bool dialWrong_ = false;
    MapObject dialObj_;
    std::vector<Id> accusable_;
    // conversation
    Id talkSuspect_;
    int talkSprite_ = 1, talkIdx_ = 0, presentIdx_ = 0;
    QString talkLine_, talkTag_, talkNote_;
    // forensic lab
    int labIdx_ = 0, todHour_ = 22, todMinute_ = 0, todField_ = 0;
    QString labNote_;
    TodPuzzle tod_;
    InkPuzzle ink_;
};
