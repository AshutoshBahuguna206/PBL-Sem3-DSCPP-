#pragma once
#include <QColor>
#include <QElapsedTimer>
#include <QPixmap>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "CaseRastogi.h"
#include "Cinematic.h"
#include "Cutscene.h"
#include "Sprites.h"
#include "Forensics.h"
#include "Game.h"
#include "Interact.h"
#include "Menu.h"
#include "Objectives.h"
#include "Player.h"
#include "Sfx.h"
#include "TileMap.h"
#include "TiledMap.h"

#ifdef HAVE_QT_SQL
class Database;
#endif
#ifdef HAVE_QT_MULTIMEDIA
class Audio;
#endif
class QCloseEvent;
class QPainter;
class QPointF;
class QKeyEvent;
class QFocusEvent;
class QPaintEvent;

// The whole game screen. Game/Case hold the rules; this class draws the world, forwards key presses and shows
// the conversations (cross-examination), the evidence picker, the forensic lab, the dossier and the ending.
// Throws std::exception from the constructor if the assets cannot be loaded.
class GameWidget : public QWidget {
public:
    explicit GameWidget(QWidget* parent = nullptr);
    ~GameWidget() override;
#ifdef HAVE_QT_SQL
    // Who is playing, and where their data lives. Called by main after the log-in window (used for saving).
    void setUser(Database* db, int userId, const QString& username);
#endif

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void closeEvent(QCloseEvent*) override;   // leaving in the middle of a game saves it
    void keyReleaseEvent(QKeyEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    bool focusNextPrevChild(bool) override { return false; }  // lets the Tab key reach keyPressEvent

private:
    enum class Mode { Explore, Dossier, Accuse, Dial, Ending, Talk, Present, LabMenu, LabTod, LabInk, GameOver,
                      Title, Help, Settings, Pause, Intro, Story, Reconstruction, Cutscene };

    // world
    void loadLocation(const Id& id, const Id& from);
    void refreshGates();
    void restart();
    void tick();
    // actions
    void interact();
    void undoStep();
    // Menus (Phase 3): the title screen, the pause menu, help and settings.
    void openTitle();
    void openPause();
    void openSettings();
    void refreshSettingsLabels();
    void leaveSubmenu();                 // back from Help or Settings to the title screen or the pause menu
    bool canContinue();                  // true when the player has a saved game
    void continueGame();                 // load the saved game
    bool saveGame();                     // save the snapshot and the player's position; false if it cannot
    void clearSave();                    // delete the saved game (the case is finished)
    bool canSave();
    void gameFinished(bool solved);      // the case ended: stop autosaving, delete the save, play the ending sound
    // Sound (Phase 3). Sounds are played from event handlers, never from paintEvent (which runs 60 times a second).
    void playSfx(Sfx s);
    void startAmbient();
    void stopAmbient();
    void applyAudioSettings();           // the Sound and Volume settings
    void playMusic(Sfx s);               // the looping music: Theme (title) or Ambient (investigation)
    void stopMusic();
    void playVoice(const std::string& id);   // a recorded line in a scene (skipped if not recorded)
    void stopSceneSounds();              // Esc during the scene: cut the voices and the effects
    // Cinematic touches (Phase 3). The timing lives in Cinematic.h (tested); these only start and draw them.
    void openIntro();                    // studio card and title card, with the theme, after log-in
    void openStory(int stage = 0);       // the story cards: 0 = the night before (then the scene), 1 = your case (then play)
    void openCutscene();                 // the opening scene: the body is found (Cutscene.h)
    void loadRoomView(const Id& room);   // show a room's map without moving the player (for the scene)
    void drawCutsceneOverlay(QPainter& p);   // black bars, the caption and the subtitles
    void openReconstruction();           // after the case is solved: the night of the murder, minute by minute
    void beginPlay();                    // after the story: the game starts
    void finishCinematicIfDone();
    void handleCinematicKey(int k);
    void drawCinematic(QPainter& p);
    void drawAtmosphere(QPainter& p, const QPointF& playerOnScreen);   // lantern light around the detective
    void showBanner();                   // the room's name, when you enter it
    void drawBanner(QPainter& p);
    void gadbad();                       // the moment a lie is caught: sting, shake, red flash, caption
    bool hasVoice() const;               // is the recorded voice line there?
    void drawEffects(QPainter& p);
    void drawStamp(QPainter& p, const QPointF& centre, const QString& text, const QColor& colour);
    // Objectives (Phase 3). The tracker (Objectives.h) is a queue of steps; these keep it in step with the game and draw it.
    void resyncObjectives();             // rebuild from the game state, without ticking anything (new game, Continue, undo)
    void drawObjective(QPainter& p);     // the current objective at the top of the screen, or the tick when one is done
    void drawChecklist(QPainter& p);     // O: the objectives completed so far
    void drawTick(QPainter& p, const QPointF& centre, qreal radius, int alpha);
    void activateMenu();
    void handleMenuKey(int k);           // title screen and pause menu
    void handleSettingsKey(int k);
    void handleHelpKey(int k);
    void drawMenu(QPainter& p, const QString& heading, const QString& subheading, const QString& hint);
    void drawHelp(QPainter& p);
    void startTalk(const MapObject& o);
    void showClue(const Id& clueId, Sfx sound = Sfx::Pickup);
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
#ifdef HAVE_QT_MULTIMEDIA
    std::unique_ptr<Audio> audio_;
#endif
    bool playing_ = false;               // a game is in progress (not on the title screen, not finished)
    unsigned long savedRev_ = 0;         // the engine's revision when we last saved
    Menu menu_;                          // the menu being shown
    Mode returnMode_ = Mode::Title;      // where Help and Settings return to (Title or Pause)
    bool soundOn_ = true;                // settings (sound is connected in a later step)
    int volume_ = 70;
    bool hints_ = true;                  // show the GOAL box
    bool fullscreen_ = false;
    bool atmosphere_ = true;             // lantern lighting (Settings)
    Sequence seq_;                       // the cards being shown (Intro or Story)
    Timer roomFade_, flash_, caption_, banner_;
    Shake shake_;
    QString bannerText_;
    float stepTimer_ = 0.f;              // footsteps while walking
    float endTime_ = 0.f;                // seconds since the case ended (the stamp and the typed story)
    ObjectiveTracker objectives_{makeRastogiObjectives()};
    unsigned long objRev_ = 0;           // the game revision the objectives were last checked at
    std::deque<size_t> tickQueue_;       // objectives just completed, waiting to show their tick one by one
    size_t tickShowing_ = 0;
    Timer tickTimer_;
    bool showChecklist_ = false;
    DustField dust_;                     // dust drifting in the lantern light
    Cutscene cut_;                       // the opening scene while it plays
    size_t cutShot_ = static_cast<size_t>(-1);   // the shot whose room is loaded
    int storyStage_ = 0;
#ifdef HAVE_QT_SQL
    Database* db_ = nullptr;
    int userId_ = 0;
    QString userName_;
#endif
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
