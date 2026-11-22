#include "GameWidget.h"
#include <QCloseEvent>
#include <QDebug>
#include <QFocusEvent>
#include <QFont>
#include <QKeyEvent>
#include <QPainter>
#include <QFontMetrics>
#include <QPainterPath>
#include <QPolygonF>
#include <QRadialGradient>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
#include "MapCheck.h"
#include "Rating.h"
#ifdef HAVE_QT_MULTIMEDIA
#include "Audio.h"
#endif
#ifdef HAVE_QT_SQL
#include "Database.h"
#endif

#ifndef ASSET_DIR
#define ASSET_DIR "assets"
#endif

namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }

// Text flags as plain ints (drawText takes an int), so we never mix Qt's flag types.
constexpr int kWrapLeftTop = static_cast<int>(Qt::AlignLeft) | static_cast<int>(Qt::AlignTop) | static_cast<int>(Qt::TextWordWrap);
constexpr int kWrapLeft = static_cast<int>(Qt::AlignLeft) | static_cast<int>(Qt::TextWordWrap);
constexpr int kWrapHCenterTop = static_cast<int>(Qt::AlignHCenter) | static_cast<int>(Qt::AlignTop) | static_cast<int>(Qt::TextWordWrap);

// The id under which this case is saved (one save per user and case).
constexpr const char* kCaseId = "the_old_way";

const char* kIntro =
    "Question everyone. When someone lies, press T and show them evidence that contradicts it; wrong evidence costs a "
    "heart. Dr. Reddy's forensic lab (press L) can tell you what the body and the ink are hiding. Accuse (press V) only "
    "when you can prove it.\n\n"
    "WASD move    E interact or talk    Tab dossier    O objectives    L lab    V accuse    U undo    Esc menu";

const char* kEnding =
    "Advocate Raghav Deshpande had stolen Rs. 2.3 crore from the Rastogi family trust for eleven years. When "
    "Dayanand found the forged certificates, he secretly called his oldest friend down the old way, hoping to hear "
    "him confess. Raghav answered with a sleeping drug in the old man's milk, then walked back through the wall.\n\n"
    "The heirs the whole house suspected were never in danger. Dayanand's real will left everything to Rohit, Meera "
    "and Kabir in equal shares.\n\n"
    "The road reopened at noon. Inspector, justice has been served.";
}  // namespace

GameWidget::GameWidget(QWidget* parent)
    : QWidget(parent),
      case_(makeRastogiCase()),
      game_(std::make_unique<Game>(case_)),
      collision_(std::vector<std::string>{"#"}),
      tileset_(q(std::string(ASSET_DIR) + "/tilesets/mansion.png")),
      characters_(q(std::string(ASSET_DIR) + "/sprites/characters.png")),
      ink_(rastogiInkPuzzle()) {
    if (tileset_.isNull()) throw std::runtime_error("Cannot load tileset: " + std::string(ASSET_DIR) + "/tilesets/mansion.png");
    if (characters_.isNull()) throw std::runtime_error("Cannot load sprites: " + std::string(ASSET_DIR) + "/sprites/characters.png");
    // Phase 3: prove the mystery is well formed before the window opens (the same check the tests run).
    const ValidationReport report = validate(case_);
    if (!report.ok()) throw std::runtime_error("The mystery failed validation: " + report.errors.front());
    for (const auto& kv : case_.suspects()) if (kv.second.accusable()) accusable_.push_back(kv.first);
    std::sort(accusable_.begin(), accusable_.end());

#ifdef HAVE_QT_MULTIMEDIA
    audio_ = std::make_unique<Audio>(q(std::string(ASSET_DIR) + "/sounds"));
    applyAudioSettings();
#endif

    setWindowTitle("The Detective's Dossier: The Old Way");
    setMinimumSize(800, 600);
    setFocusPolicy(Qt::StrongFocus);
    loadLocation(game_->location(), "");
    openIntro();                                   // studio and title cards with the theme, then the title screen
    connect(&timer_, &QTimer::timeout, this, [this] { tick(); });
    timer_.start(16);
    clock_.start();
}

#ifdef HAVE_QT_SQL
void GameWidget::setUser(Database* db, int userId, const QString& username) {
    db_ = db;
    userId_ = userId;
    userName_ = username;
    setWindowTitle(QString("The Detective's Dossier: The Old Way (%1)").arg(username));
    if (mode_ == Mode::Title) openTitle();   // rebuild the menu now that we know whose save to look for (Continue)
}
#endif

GameWidget::~GameWidget() = default;   // defined here, where Audio is a complete type

void GameWidget::restart() {
    game_ = std::make_unique<Game>(case_);
    playing_ = true;                               // an old save stays until the new game makes progress
    startAmbient();                                // (it stops when a case ends)
    savedRev_ = game_->revision();
    resyncObjectives();
    mode_ = Mode::Explore;
    keys_.clear();
    scroll_ = 0;
    loadLocation(game_->location(), "");
    openStory(0);                                  // the night before, then the opening scene, then the game
}

// ---- Cinematic touches (Phase 3). The timing is in Cinematic.h (tested); this part starts and draws them. ----

void GameWidget::openIntro() {
    mode_ = Mode::Intro;
    seq_ = Sequence(std::vector<Card>{
        {"", "Code Detektives", "presents", 3.2f},
        {"CASE FILE No. 1", "THE DETECTIVE'S DOSSIER", "The Old Way", 5.0f},
    });
    playMusic(Sfx::Theme);
}

void GameWidget::openStory(int stage) {
    mode_ = Mode::Story;
    storyStage_ = stage;
    if (stage == 0) {
        seq_ = Sequence(std::vector<Card>{
            {"MUSSOORIE, THE NIGHT BEFORE", "Rastogi Kothi",
             "The whole family came home for Dayanand Rastogi's seventy-sixth birthday. At dinner he made an "
             "announcement: a new will, to be signed at ten the next morning. That night a landslide buried the only "
             "road down the hill.", 10.f},
        });
    } else {
        seq_ = Sequence(std::vector<Card>{
            {"YOUR CASE", "Trust no one",
             "The family wants it signed off as natural causes. You do not. Question everyone, catch every lie with "
             "evidence, and name the killer only when you can prove it.", 9.f},
        });
    }
}

void GameWidget::openCutscene() {
    mode_ = Mode::Cutscene;
    showChecklist_ = false;
    cut_ = Cutscene(makeOpeningCutscene());
    cutShot_ = static_cast<size_t>(-1);                       // the first shot's room is loaded on the next frame
    stopMusic();                                              // silence, then the scene builds its own tension
}

void GameWidget::loadRoomView(const Id& room) {
    tiled_ = TiledMap::fromFile(std::string(ASSET_DIR) + "/maps/" + room + ".json");
    collision_ = tiled_.collisionMap();
}

// Cinema bars, the caption (a time and a place) at the top, and the subtitle with the speaker's name at the bottom.
void GameWidget::drawCutsceneOverlay(QPainter& p) {
    const int top = static_cast<int>(height() * 0.10), bottom = static_cast<int>(height() * 0.17);
    p.fillRect(QRect(0, 0, width(), top), QColor(0, 0, 0));
    p.fillRect(QRect(0, height() - bottom, width(), bottom), QColor(0, 0, 0));
    QFont f = p.font();
    const Shot& sh = cut_.shot();
    if (!sh.caption.empty()) {
        const int a = static_cast<int>(255 * std::clamp(cut_.time() / 1.0f, 0.f, 1.f));
        f.setFamily("Georgia"); f.setPixelSize(15); f.setBold(true); f.setLetterSpacing(QFont::AbsoluteSpacing, 2); p.setFont(f);
        p.setPen(QColor(214, 170, 70, a));
        p.drawText(QRect(0, 0, width(), top), Qt::AlignCenter, q(sh.caption));
    }
    if (const Line* l = cut_.line()) {
        const QRect area(width() / 10, height() - bottom + 10, width() * 8 / 10, bottom - 34);
        f.setFamily("Georgia"); f.setLetterSpacing(QFont::AbsoluteSpacing, 0);
        f.setPixelSize(14); f.setBold(true); p.setFont(f);
        p.setPen(QColor(214, 170, 70));
        p.drawText(area, Qt::AlignHCenter | Qt::AlignTop, q(l->speaker).toUpper());
        f.setPixelSize(18); f.setBold(false); p.setFont(f);
        p.setPen(QColor(245, 240, 228));
        p.drawText(area.adjusted(0, 22, 0, 0), kWrapHCenterTop, q(l->text));
    }
    f.setFamily(QString()); f.setPixelSize(11); f.setBold(false); f.setLetterSpacing(QFont::AbsoluteSpacing, 0); p.setFont(f);
    p.setPen(QColor(150, 145, 130));
    p.drawText(QRect(0, height() - 20, width() - 14, 16), Qt::AlignRight | Qt::AlignVCenter, "Enter: next scene      Esc: skip");
}

void GameWidget::openReconstruction() {
    mode_ = Mode::Reconstruction;
    seq_ = Sequence(std::vector<Card>{
        {"THE NIGHT OF THE MURDER", "What really happened",
         "Every lie you exposed and every clue you found, put back in order.", 5.f},
        {"AT DINNER", "A new will",
         "Dayanand announces a new will, to be signed at ten the next morning. Rohit storms out, Meera weeps and "
         "Kabir laughs too loudly. Everyone fears for their inheritance. Nobody looks at the quiet family lawyer.", 10.f},
        {"10:30 PM", "Rohit's plea",
         "Rohit begs his father not to change the will. At 10:45 he leaves him alive and calm: Raghav is coming at "
         "eleven, and they will talk after.", 9.f},
        {"10:38 PM", "A call cut short",
         "Upstairs in the Blue Room, Raghav hangs up on his wife. He will later swear the call lasted until half past "
         "eleven.", 8.f},
        {"10:50 PM", "A glass of milk",
         "Meera carries kesar milk to the library. The milk left in the jug is clean: the sleeping drug went into the "
         "glass after it left the kitchen.", 9.f},
        {"11:00 PM", "Down the old way",
         "Dayanand has found the forged certificates: Rs. 2.3 crore taken from the family trust over eleven years. He "
         "calls his oldest friend down the secret passage, hoping he will confess.", 10.f},
        {"11:05 PM", "A dark window",
         "From the verandah, Kabir sees the Blue Room window dark. Raghav is on no phone call. He is in the library.", 8.f},
        {"11:15 PM", "The murder",
         "Raghav answers with a sleeping drug in the old man's milk. Dayanand dies in his armchair, the door locked "
         "from inside. Raghav walks back through the wall, and loses a gold cufflink in the dust.", 10.f},
        {"11:40 PM", "The road falls",
         "A landslide cuts the only road down the hill. The killer is trapped in the house with everyone else.", 7.f},
        {"THE PROOF", "What gave him away",
         "His wife's text broke his alibi. His cufflink put him in the passage. And the ink on the forged "
         "certificates matched his own fountain pen.", 9.f},
    });
    playMusic(Sfx::Theme);
}

void GameWidget::beginPlay() {
    mode_ = Mode::Explore;
    keys_.clear();
    loadLocation(game_->location(), "");                          // back to the real room after the scene
    startAmbient();                                               // (the scene may have been skipped before the music)
    roomFade_.start(1.0f);
    showBanner();
    say("How to play", q(kIntro));
}

void GameWidget::finishCinematicIfDone() {
    if (!seq_.done()) return;
    if (mode_ == Mode::Intro) openTitle();
    else if (mode_ == Mode::Story) { if (storyStage_ == 0) openCutscene(); else beginPlay(); }
    else if (mode_ == Mode::Reconstruction) { mode_ = Mode::Ending; endTime_ = 1000.f; }   // back to the closed case
}

void GameWidget::handleCinematicKey(int k) {
    if (k == Qt::Key_Escape) seq_.skipAll();                                   // skip it all
    else if (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Space || k == Qt::Key_E) seq_.next();
    else return;
    finishCinematicIfDone();
}

void GameWidget::drawCinematic(QPainter& p) {
    p.fillRect(rect(), QColor(5, 6, 10, mode_ == Mode::Intro ? 255 : 230));
    QRadialGradient vignette(QPointF(rect().center()), std::max(width(), height()) * 0.75);
    vignette.setColorAt(0.0, QColor(40, 30, 18, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 210));
    p.fillRect(rect(), vignette);

    const Card& c = seq_.card();
    const int a = static_cast<int>(255 * seq_.alpha());
    const bool big = (mode_ == Mode::Intro && seq_.index() == 1);            // the title card
    const QRect area = rect().adjusted(width() / 8, height() / 5, -width() / 8, -height() / 6);
    QFont f = p.font();
    f.setFamily("Georgia");
    if (!c.kicker.empty()) {
        f.setPixelSize(15); f.setBold(true); f.setLetterSpacing(QFont::AbsoluteSpacing, 3); p.setFont(f);
        p.setPen(QColor(214, 170, 70, a));
        p.drawText(area, Qt::AlignHCenter | Qt::AlignTop, q(c.kicker));
    }
    f.setPixelSize(big ? 52 : 38); f.setBold(true); f.setLetterSpacing(QFont::AbsoluteSpacing, big ? 4 : 1); p.setFont(f);
    p.setPen(QColor(245, 230, 200, a));
    p.drawText(area.adjusted(0, 34, 0, 0), Qt::AlignHCenter | Qt::AlignTop, q(c.title));
    const int ruleY = area.top() + 34 + (big ? 76 : 58);
    p.fillRect(QRect(width() / 2 - 90, ruleY, 180, 2), QColor(214, 170, 70, a));     // a thin gold rule

    f.setPixelSize(mode_ == Mode::Intro ? 22 : 19); f.setBold(false); f.setItalic(mode_ == Mode::Intro);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0); p.setFont(f);
    p.setPen(QColor(225, 225, 225, a));
    const std::string shown = c.body.substr(0, seq_.visibleChars());         // the typewriter
    p.drawText(QRect(area.left() + width() / 20, ruleY + 26, area.width() - width() / 10, area.bottom() - ruleY),
               kWrapHCenterTop, q(shown));

    f.setItalic(false); f.setPixelSize(13); p.setFont(f);
    p.setPen(QColor(200, 190, 150, 170));
    p.drawText(rect().adjusted(0, 0, 0, -24), Qt::AlignHCenter | Qt::AlignBottom, "Enter: continue        Esc: skip");
}

// Lantern light: clear around the detective, falling into darkness towards the edges of the screen.
void GameWidget::drawAtmosphere(QPainter& p, const QPointF& playerOnScreen) {
    const qreal radius = std::max(width(), height()) * 0.72 * lanternFlicker(elapsed_);
    QRadialGradient g(playerOnScreen, radius);
    g.setColorAt(0.00, QColor(255, 200, 120, 16));     // a faint warm glow
    g.setColorAt(0.28, QColor(0, 0, 0, 0));
    g.setColorAt(0.70, QColor(8, 6, 4, 120));
    g.setColorAt(1.00, QColor(4, 3, 2, 205));
    p.fillRect(rect(), g);
    // dust motes, visible only near the light
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    const qreal reach = radius * 0.42;
    for (const Mote& m : dust_.motes()) {
        const QPointF at(m.x * width(), m.y * height());
        const qreal d = std::hypot(at.x() - playerOnScreen.x(), at.y() - playerOnScreen.y());
        const qreal vis = 1.0 - d / reach;
        if (vis <= 0.0) continue;
        p.setBrush(QColor(255, 230, 180, static_cast<int>(110 * vis * DustField::twinkle(m, elapsed_))));
        p.drawEllipse(at, m.size, m.size);
    }
    p.restore();
}

void GameWidget::showBanner() {
    if (const Location* l = case_.location(game_->location())) { bannerText_ = q(l->name()).toUpper(); banner_.start(2.8f); }
}

void GameWidget::drawBanner(QPainter& p) {
    const float a = banner_.envelope(0.4f, 0.9f);
    if (a <= 0.f) return;
    QFont f = p.font(); f.setFamily("Georgia"); f.setPixelSize(30); f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 3); p.setFont(f);
    const QRect r(0, height() / 6, width(), 50);
    p.setPen(QColor(0, 0, 0, static_cast<int>(170 * a)));
    p.drawText(r.translated(2, 2), Qt::AlignCenter, bannerText_);
    p.setPen(QColor(245, 230, 200, static_cast<int>(255 * a)));
    p.drawText(r, Qt::AlignCenter, bannerText_);
    p.fillRect(QRect(width() / 2 - 70, r.bottom() + 4, 140, 2), QColor(214, 170, 70, static_cast<int>(220 * a)));
}

bool GameWidget::hasVoice() const {
#ifdef HAVE_QT_MULTIMEDIA
    return audio_ && audio_->has(Sfx::Voice);
#else
    return false;
#endif
}

// The moment a lie is caught. With the recorded line: the voice first, then the sting with a shake and a red flash.
// Without it: the sting, the shake and the flash at once. The line also appears on screen.
void GameWidget::gadbad() {
    if (hasVoice()) {
        playSfx(Sfx::Voice);
        caption_.start(2.6f + kVoiceLeadMs / 1000.f);
        QTimer::singleShot(kVoiceLeadMs, this, [this] {
            playSfx(Sfx::Exposed);
            shake_.start(9.f, 0.7f);
            flash_.start(0.6f);
        });
    } else {
        playSfx(Sfx::Exposed);
        shake_.start(9.f, 0.7f);
        flash_.start(0.6f);
        caption_.start(2.6f);
    }
}

void GameWidget::drawEffects(QPainter& p) {
    if (flash_.active()) p.fillRect(rect(), QColor(170, 20, 20, static_cast<int>(120 * flash_.remaining())));
    const float a = caption_.envelope(0.15f, 0.8f);
    if (a <= 0.f) return;
    QFont f = p.font(); f.setFamily("Georgia"); f.setPixelSize(40); f.setBold(true); f.setItalic(true); p.setFont(f);
    const QRect r(0, height() / 7, width(), 70);
    p.setPen(QColor(0, 0, 0, static_cast<int>(200 * a)));
    p.drawText(r.translated(3, 3), Qt::AlignCenter, "Kuchh toh gadbad hai...");
    p.setPen(QColor(255, 214, 120, static_cast<int>(255 * a)));
    p.drawText(r, Qt::AlignCenter, "Kuchh toh gadbad hai...");
}

// ---- Objectives (Phase 3). The tracker is a queue of steps (Objectives.h, tested); this keeps it in step and draws it. ----

void GameWidget::resyncObjectives() {
    objectives_.reset();
    objectives_.update(*game_);                                   // catch up silently: no ticks for steps already done
    objRev_ = game_->revision();
    tickQueue_.clear();
    tickTimer_ = Timer();
}

// A green circle with a white tick, drawn with a path (so it does not depend on the font).
void GameWidget::drawTick(QPainter& p, const QPointF& c, qreal r, int alpha) {
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(60, 170, 90, alpha));
    p.drawEllipse(c, r, r);
    QPen pen(QColor(255, 255, 255, alpha), std::max<qreal>(1.5, r * 0.26));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QPainterPath path;
    path.moveTo(c.x() - r * 0.45, c.y() + r * 0.02);
    path.lineTo(c.x() - r * 0.10, c.y() + r * 0.38);
    path.lineTo(c.x() + r * 0.50, c.y() - r * 0.35);
    p.drawPath(path);
    p.restore();
}

// Top of the screen, between the room box and the hearts: the current objective with a progress bar,
// or, for a moment after one is done, a tick and "OBJECTIVE COMPLETE".
void GameWidget::drawObjective(QPainter& p) {
    if (!hints_) return;
    const int avail = width() - 272 - 190;                        // room box on the left, hearts on the right
    const int w = std::clamp(avail, 300, 560);
    const QRect r(272 + std::max(0, (avail - w) / 2), 12, w, 68);
    panel(p, r);
    QFont f = p.font();
    if (tickTimer_.active() && tickShowing_ < objectives_.total()) {
        const float a = tickTimer_.envelope(0.15f, 0.4f);
        const qreal grow = std::min<qreal>(1.0, tickTimer_.progress() * 6.0);   // the tick pops in
        drawTick(p, QPointF(r.left() + 30, r.center().y()), 15 * grow, static_cast<int>(255 * a));
        f.setPixelSize(11); f.setBold(true); p.setFont(f);
        p.setPen(QColor(110, 210, 130, static_cast<int>(255 * a)));
        p.drawText(r.adjusted(58, 8, -12, 0), Qt::AlignLeft | Qt::AlignTop, "OBJECTIVE COMPLETE");
        f.setPixelSize(13); f.setBold(false); p.setFont(f);
        p.setPen(QColor(235, 235, 235, static_cast<int>(255 * a)));
        p.drawText(r.adjusted(58, 25, -12, -8), kWrapLeftTop, q(objectives_.at(tickShowing_).text));
        return;
    }
    if (!objectives_.hasCurrent()) return;
    const Objective& o = objectives_.current();
    QString head = QString("OBJECTIVE %1 OF %2").arg(objectives_.currentIndex() + 1).arg(objectives_.total());
    if (!o.place.empty())
        if (const Location* l = case_.location(o.place)) head += "   |   " + q(l->name()).toUpper();
    f.setPixelSize(11); f.setBold(true); p.setFont(f);
    p.setPen(QColor(214, 170, 70));
    p.drawText(r.adjusted(14, 8, -12, 0), Qt::AlignLeft | Qt::AlignTop, head);
    f.setPixelSize(13); f.setBold(false); p.setFont(f);
    p.setPen(QColor(240, 240, 240));
    p.drawText(r.adjusted(14, 25, -12, -10), kWrapLeftTop, q(o.text));
    const double done = static_cast<double>(objectives_.completed()) / static_cast<double>(std::max<size_t>(1, objectives_.total()));
    const QRect bar(r.left() + 14, r.bottom() - 7, r.width() - 28, 3);
    p.fillRect(bar, QColor(255, 255, 255, 40));
    p.fillRect(QRect(bar.left(), bar.top(), static_cast<int>(bar.width() * done), bar.height()), QColor(214, 170, 70));
}

// O: the objectives completed so far (ticked), the current one, and how many are left. Later steps stay hidden.
void GameWidget::drawChecklist(QPainter& p) {
    const size_t total = objectives_.total(), done = objectives_.completed();
    const size_t left = total - done - (objectives_.hasCurrent() ? 1 : 0);
    const int rows = static_cast<int>(done) + (objectives_.hasCurrent() ? 1 : 0) + (left > 0 ? 1 : 0);
    const QRect r(width() - 392, 88, 380, 58 + rows * 24);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(13); f.setBold(true); p.setFont(f);
    p.setPen(QColor(214, 170, 70));
    p.drawText(r.adjusted(16, 10, -12, 0), Qt::AlignLeft | Qt::AlignTop, QString("OBJECTIVES   %1 / %2").arg(done).arg(total));
    f.setPixelSize(12); f.setBold(false); p.setFont(f);
    const QFontMetrics fm(f);
    const int textW = r.width() - 56;
    int y = r.top() + 40;
    for (size_t i = 0; i < total; ++i) {
        if (!objectives_.isDone(i)) continue;
        drawTick(p, QPointF(r.left() + 26, y + 9), 8, 255);
        p.setPen(QColor(170, 180, 175));
        p.drawText(QRect(r.left() + 42, y, textW, 20), Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(q(objectives_.at(i).text), Qt::ElideRight, textW));
        y += 24;
    }
    if (objectives_.hasCurrent()) {
        QPolygonF arrow;                                          // a small gold arrow for the current step
        arrow << QPointF(r.left() + 20, y + 3) << QPointF(r.left() + 32, y + 9) << QPointF(r.left() + 20, y + 15);
        p.setPen(Qt::NoPen); p.setBrush(QColor(214, 170, 70)); p.drawPolygon(arrow); p.setBrush(Qt::NoBrush);
        p.setPen(QColor(250, 240, 210));
        p.drawText(QRect(r.left() + 42, y, textW, 20), Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(q(objectives_.current().text), Qt::ElideRight, textW));
        y += 24;
    }
    if (left > 0) {
        f.setItalic(true); p.setFont(f);
        p.setPen(QColor(150, 150, 160));
        p.drawText(QRect(r.left() + 42, y, textW, 20), Qt::AlignLeft | Qt::AlignVCenter, QString("+ %1 more to discover").arg(left));
    }
    f.setItalic(false); f.setPixelSize(11); p.setFont(f);
    p.setPen(QColor(200, 190, 150, 170));
    p.drawText(r.adjusted(0, 0, -12, -6), Qt::AlignRight | Qt::AlignBottom, "O: close");
}

// ---- Menus (Phase 3). The logic of the cursor is in Menu.h; this part opens, draws and reacts. ----

void GameWidget::openTitle() {
    mode_ = Mode::Title;
    playing_ = false;
    playMusic(Sfx::Theme);
    std::vector<MenuItem> items = {{"New Game"}, {"Continue", canContinue()}, {"Help"}, {"Settings"}, {"Quit"}};
    menu_ = Menu(items);
}

void GameWidget::openPause() {
    mode_ = Mode::Pause;
    std::vector<MenuItem> items = {{"Resume"}, {"Save game", canSave()}, {"Help"}, {"Settings"}, {"Quit to title"}};
    menu_ = Menu(items);
}

void GameWidget::openSettings() {
    mode_ = Mode::Settings;
    std::vector<MenuItem> items = {{"Sound"}, {"Volume"}, {"Goal hints"}, {"Atmosphere"}, {"Fullscreen"}, {"Back"}};
    menu_ = Menu(items);
    refreshSettingsLabels();
}

void GameWidget::refreshSettingsLabels() {
    menu_.setLabel(0, std::string("Sound: ") + (soundOn_ ? "On" : "Off"));
    menu_.setLabel(1, "Volume: " + std::to_string(volume_) + "%");
    menu_.setLabel(2, std::string("Goal hints: ") + (hints_ ? "On" : "Off"));
    menu_.setLabel(3, std::string("Atmosphere: ") + (atmosphere_ ? "On" : "Off"));
    menu_.setLabel(4, std::string("Fullscreen: ") + (fullscreen_ ? "On" : "Off"));
}

void GameWidget::leaveSubmenu() {
    if (returnMode_ == Mode::Pause) openPause();
    else openTitle();
}

// Continue is switched on only when this player has a saved game for this case.
bool GameWidget::canContinue() {
#ifdef HAVE_QT_SQL
    return db_ && userId_ > 0 && db_->hasSave(userId_, kCaseId);
#else
    return false;
#endif
}

bool GameWidget::canSave() {
#ifdef HAVE_QT_SQL
    return db_ && userId_ > 0 && playing_;
#else
    return false;
#endif
}

// Save the game state (the engine's snapshot) and where the player stands, in one database transaction.
bool GameWidget::saveGame() {
#ifdef HAVE_QT_SQL
    if (!canSave()) return false;
    if (!db_->save(userId_, kCaseId, game_->snapshot(), player_.x, player_.y)) return false;
    savedRev_ = game_->revision();
    return true;
#else
    return false;
#endif
}

void GameWidget::clearSave() {
#ifdef HAVE_QT_SQL
    if (db_ && userId_ > 0) db_->deleteSave(userId_, kCaseId);
#endif
}

// The case is over (solved or failed): there is nothing to continue, so stop autosaving and delete the save.
void GameWidget::gameFinished(bool solved) {
    playing_ = false;
    endTime_ = 0.f;
    clearSave();
    stopAmbient();
    playSfx(solved ? Sfx::Ending : Sfx::GameOver);
}

// ---- Sound (Phase 3). Everything is a no-op when Qt Multimedia is not installed. ----
void GameWidget::playSfx(Sfx s) {
#ifdef HAVE_QT_MULTIMEDIA
    if (audio_) audio_->play(s);
#else
    (void)s;
#endif
}
void GameWidget::playMusic(Sfx s) {
#ifdef HAVE_QT_MULTIMEDIA
    if (audio_) audio_->playMusic(s);
#else
    (void)s;
#endif
}
void GameWidget::stopMusic() {
#ifdef HAVE_QT_MULTIMEDIA
    if (audio_) audio_->stopMusic();
#endif
}
void GameWidget::playVoice(const std::string& id) {
#ifdef HAVE_QT_MULTIMEDIA
    if (audio_) audio_->playVoice(id);
#else
    (void)id;
#endif
}
void GameWidget::stopSceneSounds() {
#ifdef HAVE_QT_MULTIMEDIA
    if (!audio_) return;
    audio_->stopVoices();
    for (Sfx s : {Sfx::Chaos, Sfx::Running, Sfx::Pound, Sfx::Knock, Sfx::Discovery}) audio_->stop(s);
#endif
}
void GameWidget::startAmbient() { playMusic(Sfx::Ambient); }
void GameWidget::stopAmbient() { stopMusic(); }
void GameWidget::applyAudioSettings() {
#ifdef HAVE_QT_MULTIMEDIA
    if (audio_) { audio_->setMuted(!soundOn_); audio_->setVolume(volume_); }
#endif
}

// Load the saved game. A missing, damaged or foreign save is refused by restore() and never crashes the game.
void GameWidget::continueGame() {
#ifdef HAVE_QT_SQL
    if (!db_ || userId_ <= 0) return;
    const auto saved = db_->load(userId_, kCaseId);
    if (!saved || !game_->restore(saved->snapshot)) {
        say("Continue", q("The saved game could not be loaded."));
        return;
    }
    mode_ = Mode::Explore;
    keys_.clear();
    scroll_ = 0;
    loadLocation(game_->location(), "");                                   // draws the room and refreshes the gates
    if (!player_.collides(saved->x, saved->y, collision_)) { player_.x = saved->x; player_.y = saved->y; }   // where you stood
    playing_ = true;
    savedRev_ = game_->revision();
    resyncObjectives();
    startAmbient();
    roomFade_.start(1.0f);
    showBanner();
    say("Welcome back", q("Your game has been restored."));
#endif
}

void GameWidget::activateMenu() {
    const size_t i = menu_.index();
    if (mode_ == Mode::Title) {
        switch (i) {
            case 0: restart(); break;                                      // New Game
            case 1: continueGame(); break;                                 // Continue (only reachable when switched on)
            case 2: returnMode_ = Mode::Title; mode_ = Mode::Help; break;
            case 3: returnMode_ = Mode::Title; openSettings(); break;
            case 4: close(); break;                                        // Quit
            default: break;
        }
    } else {                                                               // the pause menu
        switch (i) {
            case 0: mode_ = Mode::Explore; break;                          // Resume
            case 1:                                                        // Save game
                mode_ = Mode::Explore;
                if (saveGame()) say("Game saved", q("Your progress is saved. Choose Continue on the title screen to return to it."));
                else say("Save failed", q("Your game could not be saved."));
                break;
            case 2: returnMode_ = Mode::Pause; mode_ = Mode::Help; break;
            case 3: returnMode_ = Mode::Pause; openSettings(); break;
            case 4:                                                        // Quit to title: keep the progress
                if (playing_ && game_->revision() > 0) saveGame();
                openTitle();
                break;
            default: break;
        }
    }
}

void GameWidget::handleMenuKey(int k) {
    if (k == Qt::Key_Up || k == Qt::Key_W) { menu_.prev(); return; }
    if (k == Qt::Key_Down || k == Qt::Key_S) { menu_.next(); return; }
    if (k == Qt::Key_Escape && mode_ == Mode::Pause) { mode_ = Mode::Explore; return; }
    if (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Space || k == Qt::Key_E) activateMenu();
}

void GameWidget::handleSettingsKey(int k) {
    if (k == Qt::Key_Escape) { leaveSubmenu(); return; }
    if (k == Qt::Key_Up || k == Qt::Key_W) { menu_.prev(); return; }
    if (k == Qt::Key_Down || k == Qt::Key_S) { menu_.next(); return; }
    const size_t i = menu_.index();
    const bool left = (k == Qt::Key_Left || k == Qt::Key_A);
    const bool right = (k == Qt::Key_Right || k == Qt::Key_D);
    const bool select = (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Space || k == Qt::Key_E);
    if (i == 0 && (select || left || right)) soundOn_ = !soundOn_;
    else if (i == 1 && (left || right)) volume_ = std::clamp(volume_ + (right ? 10 : -10), 0, 100);
    else if (i == 2 && (select || left || right)) hints_ = !hints_;
    else if (i == 3 && (select || left || right)) atmosphere_ = !atmosphere_;
    else if (i == 4 && (select || left || right)) { fullscreen_ = !fullscreen_; if (fullscreen_) showFullScreen(); else showNormal(); }
    else if (i == 5 && select) { leaveSubmenu(); return; }                 // Back
    applyAudioSettings();                                                  // sound on or off, volume
    if (i == 1 && (left || right)) playSfx(Sfx::DialClick);                // a tick, so you can hear the new level
    refreshSettingsLabels();
}

void GameWidget::handleHelpKey(int k) {
    if (k == Qt::Key_Escape || k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Space || k == Qt::Key_E) leaveSubmenu();
}

// A centred panel with a heading and the menu's items; the selected one is highlighted, switched-off ones are grey.
void GameWidget::drawMenu(QPainter& p, const QString& heading, const QString& subheading, const QString& hint) {
    p.fillRect(rect(), QColor(0, 0, 0, mode_ == Mode::Title ? 205 : 170));
    const int rows = static_cast<int>(menu_.items().size());
    QRect r(width() / 2 - 260, height() / 2 - 60 - rows * 26, 520, 130 + rows * 52 + 40);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(30); f.setBold(true); p.setFont(f);
    p.setPen(QColor(240, 215, 140));
    p.drawText(r.adjusted(0, 18, 0, 0), Qt::AlignHCenter | Qt::AlignTop, heading);
    if (!subheading.isEmpty()) {
        f.setPixelSize(16); f.setBold(false); p.setFont(f);
        p.setPen(QColor(200, 190, 160));
        p.drawText(r.adjusted(0, 62, 0, 0), Qt::AlignHCenter | Qt::AlignTop, subheading);
    }
    for (int i = 0; i < rows; ++i) {
        const MenuItem& item = menu_.items()[static_cast<size_t>(i)];
        QRect row(r.left() + 40, r.top() + 110 + i * 52, r.width() - 80, 44);
        if (static_cast<size_t>(i) == menu_.index()) { p.setPen(Qt::NoPen); p.setBrush(QColor(120, 200, 240, 70)); p.drawRoundedRect(row, 8, 8); }
        f.setPixelSize(20); f.setBold(true); p.setFont(f);
        p.setPen(item.enabled ? QColor(240, 240, 240) : QColor(130, 130, 130));
        p.drawText(row, Qt::AlignCenter, q(item.label + (item.enabled ? "" : "  (no saved game)")));
    }
    f.setPixelSize(12); f.setBold(false); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r.adjusted(24, 0, -24, -12), Qt::AlignHCenter | Qt::AlignBottom, hint);
}

void GameWidget::drawHelp(QPainter& p) {
    p.fillRect(rect(), QColor(0, 0, 0, 185));
    QRect r(width() / 2 - 340, height() / 2 - 250, 680, 500);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(26); f.setBold(true); p.setFont(f);
    p.setPen(QColor(240, 215, 140));
    p.drawText(r.adjusted(0, 16, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "How to play");
    f.setPixelSize(15); f.setBold(false); p.setFont(f);
    p.setPen(QColor(240, 240, 240));
    p.drawText(r.adjusted(34, 66, -34, -40), kWrapLeftTop,
               "WASD or the arrow keys: walk\n"
               "E or Space: talk, pick up, open doors\n"
               "Tab or J: your dossier (clues and people)\n"
               "L: Dr. Reddy's forensic lab\n"
               "V: accuse the killer\n"
               "U: undo your last move or pick-up\n"
               "O: the objectives you have completed\n"
               "Esc: pause menu\n\n"
               "Talk to a person, press P to press a statement and T to show evidence. A lie is only exposed by evidence "
               "that contradicts it, and wrong evidence costs a heart. Expose at least two of the killer's lies, then accuse.");
    f.setPixelSize(12); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r.adjusted(24, 0, -24, -12), Qt::AlignHCenter | Qt::AlignBottom, "Enter or Esc: back");
}

// U: undo the last move or room pick-up (the engine's stack). Earned clues and exposed lies are never undone.
void GameWidget::undoStep() {
    const Id before = game_->location();
    const Result r = game_->undo();
    if (r != Result::Ok) { say("Undo", q(resultMessage(r))); return; }       // "Nothing to undo."
    if (game_->location() != before) loadLocation(game_->location(), before); // we stepped back a room
    refreshGates();                                                           // a gate may close again
    resyncObjectives();                                                       // an undone pick-up may undo an objective
    say("Undo", q("You step back."));
}

void GameWidget::loadLocation(const Id& id, const Id& from) {
    tiled_ = TiledMap::fromFile(std::string(ASSET_DIR) + "/maps/" + id + ".json");
    collision_ = tiled_.collisionMap();
    for (const auto& w : checkMap(tiled_, case_, id)) qWarning().noquote() << "MAP WARNING (" << q(id) << "):" << q(w);
    auto pt = arrivalPoint(tiled_, collision_, from, player_);
    player_.x = pt.first;
    player_.y = pt.second;
    refreshGates();
    showBanner();
}

// Gates (the dial-lock door) block the player until their clue is earned.
void GameWidget::refreshGates() {
    const int ts = collision_.tileSize();
    for (const auto& o : tiled_.objects()) {
        if (o.type != "dial") continue;
        const bool open = game_->has(o.prop("clue_id"));
        const int x0 = static_cast<int>(std::floor(o.x / ts)), x1 = static_cast<int>(std::ceil((o.x + o.w) / ts)) - 1;
        const int y0 = static_cast<int>(std::floor(o.y / ts)), y1 = static_cast<int>(std::ceil((o.y + o.h) / ts)) - 1;
        for (int ty = y0; ty <= y1; ++ty)
            for (int tx = x0; tx <= x1; ++tx) collision_.set(tx, ty, open ? '.' : '#');
    }
}

void GameWidget::say(const QString& title, const QString& body) {
    msgTitle_ = title;
    msgBody_ = body;
}

void GameWidget::checkFailure() {
    if (game_->failed()) { mode_ = Mode::GameOver; gameFinished(false); }
}

void GameWidget::tick() {
    float dt = std::min(clock_.restart() / 1000.0f, 0.05f);  // clamp so a lag spike can't teleport us through walls
    elapsed_ += dt;
    roomFade_.update(dt); flash_.update(dt); caption_.update(dt); banner_.update(dt); shake_.update(dt); tickTimer_.update(dt);
    if (playing_ && game_->revision() != objRev_) {               // the game changed: are objectives done?
        objRev_ = game_->revision();
        for (size_t i : objectives_.update(*game_)) tickQueue_.push_back(i);
    }
    if (!tickTimer_.active() && !tickQueue_.empty()) {            // show the next tick
        tickShowing_ = tickQueue_.front();
        tickQueue_.pop_front();
        tickTimer_.start(2.4f);
        playSfx(Sfx::Objective);
    }
    if (mode_ == Mode::Intro || mode_ == Mode::Story || mode_ == Mode::Reconstruction) { seq_.update(dt); finishCinematicIfDone(); }
    dust_.update(dt);
    if (mode_ == Mode::Cutscene) {
        for (const std::string& n : cut_.update(dt)) {
            Sfx sfx;
            if (n == "shake") shake_.start(10.f, 0.5f);
            else if (n.rfind("voice:", 0) == 0) playVoice(n.substr(6));       // a recorded line or reaction
            else if (n == "music:ambient") startAmbient();
            else if (sfxFromName(n, sfx)) playSfx(sfx);
        }
        if (cut_.done()) {
            openStory(1);
        } else {
            if (cut_.index() != cutShot_) { cutShot_ = cut_.index(); loadRoomView(cut_.shot().room); roomFade_.start(0.6f); }
            const Shot& sh = cut_.shot();
            const Pose focus = sh.follow >= 0 ? poseAt(sh.tracks[static_cast<size_t>(sh.follow)], cut_.time())
                                              : Pose{sh.camX, sh.camY, 0, false};
            player_.x = focus.x;                                  // the camera centres on the player's position,
            player_.y = focus.y;                                  // so during the scene it is pointed here
        }
    }
    if (mode_ == Mode::Ending || mode_ == Mode::GameOver) endTime_ += dt;
    if (mode_ == Mode::Explore && !hasMessage()) {
        float dx = (down(Qt::Key_D, Qt::Key_Right) ? 1.f : 0.f) - (down(Qt::Key_A, Qt::Key_Left) ? 1.f : 0.f);
        float dy = (down(Qt::Key_S, Qt::Key_Down) ? 1.f : 0.f) - (down(Qt::Key_W, Qt::Key_Up) ? 1.f : 0.f);
        player_.move(dx, dy, dt, collision_);
        if (player_.moving) {                                 // soft footsteps while walking
            stepTimer_ += dt;
            if (stepTimer_ >= 0.36f) { stepTimer_ = 0.f; playSfx(Sfx::Step); }
        } else {
            stepTimer_ = 0.3f;                                // the first step sounds almost at once
        }
    }
    update();
#ifdef HAVE_QT_SQL
    // Autosave: the engine's revision goes up whenever the game state really changes.
    if (playing_ && game_->revision() != savedRev_) {
        savedRev_ = game_->revision();            // even if saving fails, do not retry 60 times a second
        if (!saveGame()) say("Saving", q("Your progress could not be saved."));
    }
#endif
}

// ---------------------------------------------------------------- actions

void GameWidget::showClue(const Id& clueId, Sfx sound) {
    const Clue* k = case_.clue(clueId);
    if (!k) return;
    playSfx(sound);
    say(q(k->name()), q(k->description()) + "\n\n(Added to your dossier. Press Tab to read it.)");
}

void GameWidget::startTalk(const MapObject& o) {
    const Suspect* s = case_.suspect(o.prop("suspect_id"));
    if (!s || game_->statements(s->id()).empty()) return;
    talkSuspect_ = s->id();
    talkSprite_ = spriteIndex(o.prop("sprite"));
    talkIdx_ = 0;
    talkTag_.clear();
    talkNote_.clear();
    talkLine_ = q(game_->statements(s->id())[0].text);
    mode_ = Mode::Talk;
}

void GameWidget::interact() {
    const MapObject* o = nearestInteractable(tiled_, *game_, player_.x, player_.y);
    if (!o) return;
    if (o->type == "door") {
        const Id from = game_->location();
        Result r = game_->move(o->prop("to"));
        if (r == Result::Ok) { playSfx(Sfx::Door); loadLocation(game_->location(), from); roomFade_.start(0.45f); return; }
        const std::string hint = o->prop("locked_hint");
        playSfx(Sfx::Locked);
        say("Locked", q(r == Result::Locked && !hint.empty() ? hint : resultMessage(r)));
    } else if (o->type == "clue") {
        const Id cid = o->prop("clue_id");
        Result r = game_->collect(cid);
        if (r == Result::Ok) { refreshGates(); showClue(cid); }
        else say(q(o->name), q(resultMessage(r)));
    } else if (o->type == "npc") {
        startTalk(*o);
    } else if (o->type == "dial") {
        const Clue* k = case_.clue(o->prop("clue_id"));
        if (k && !prerequisitesMet(*k, game_->heldSet())) {
            say("Dial lock", "An iron door with a brass dial. It wants a year in the English calendar. You have no idea which.");
        } else {
            dialObj_ = *o;
            dialInput_.clear();
            dialWrong_ = false;
            mode_ = Mode::Dial;
        }
    }
}

// ---------------------------------------------------------------- keys

void GameWidget::keyReleaseEvent(QKeyEvent* e) {
    if (!e->isAutoRepeat()) keys_.remove(e->key());
}
void GameWidget::closeEvent(QCloseEvent* e) {
    if (playing_ && game_->revision() > 0) saveGame();      // leaving in the middle of a game keeps the progress
    e->accept();
}

void GameWidget::focusOutEvent(QFocusEvent*) { keys_.clear(); }

void GameWidget::keyPressEvent(QKeyEvent* e) {
    const int k = e->key();
    if (e->isAutoRepeat() && k != Qt::Key_Up && k != Qt::Key_Down && k != Qt::Key_Backspace) return;
    keys_.insert(k);   // always remember held keys, even while a message or overlay is open
    switch (mode_) {
        case Mode::Dial: handleDialKey(e); return;
        case Mode::Accuse: handleAccuseKey(k); return;
        case Mode::Dossier: handleDossierKey(k); return;
        case Mode::Talk: handleTalkKey(k); return;
        case Mode::Present: handlePresentKey(k); return;
        case Mode::LabMenu: handleLabMenuKey(k); return;
        case Mode::LabTod: handleLabTodKey(k); return;
        case Mode::LabInk: handleLabInkKey(k); return;
        case Mode::Ending: case Mode::GameOver:
            if (k == Qt::Key_R) restart();
            else if (k == Qt::Key_T && mode_ == Mode::Ending) openReconstruction();
            else if (k == Qt::Key_Escape) openTitle();
            else if (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Space) endTime_ = 1000.f;   // show it all
            return;
        case Mode::Title: case Mode::Pause: handleMenuKey(k); return;
        case Mode::Settings: handleSettingsKey(k); return;
        case Mode::Help: handleHelpKey(k); return;
        case Mode::Intro: case Mode::Story: case Mode::Reconstruction: handleCinematicKey(k); return;
        case Mode::Cutscene:
            if (k == Qt::Key_Escape) { cut_.skipAll(); stopSceneSounds(); openStory(1); }
            else if (k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Space || k == Qt::Key_E) {
                cut_.nextShot();
                if (cut_.done()) openStory(1);
            }
            return;
        case Mode::Explore: break;
    }
    if (hasMessage()) {
        const bool walkKey = k == Qt::Key_W || k == Qt::Key_A || k == Qt::Key_S || k == Qt::Key_D ||
                             k == Qt::Key_Left || k == Qt::Key_Right || k == Qt::Key_Up || k == Qt::Key_Down;
        if (walkKey || k == Qt::Key_E || k == Qt::Key_Space || k == Qt::Key_Return || k == Qt::Key_Enter || k == Qt::Key_Escape) {
            msgTitle_.clear();   // any walking key closes the message, and you keep walking
            msgBody_.clear();
        }
        return;
    }
    if (k == Qt::Key_E || k == Qt::Key_Space) interact();
    else if (k == Qt::Key_Tab || k == Qt::Key_J) { mode_ = Mode::Dossier; dossierPage_ = 0; scroll_ = 0; }
    else if (k == Qt::Key_V) { mode_ = Mode::Accuse; accuseIdx_ = 0; }
    else if (k == Qt::Key_L) { mode_ = Mode::LabMenu; labIdx_ = 0; labNote_.clear(); }
    else if (k == Qt::Key_U) undoStep();
    else if (k == Qt::Key_Escape) openPause();
    else if (k == Qt::Key_O) showChecklist_ = !showChecklist_;
    else if (k == Qt::Key_F1) debug_ = !debug_;
}

void GameWidget::handleDossierKey(int k) {
    if (k == Qt::Key_Escape || k == Qt::Key_J) { mode_ = Mode::Explore; return; }
    if (k == Qt::Key_Tab) { if (++dossierPage_ > 1) mode_ = Mode::Explore; scroll_ = 0; return; }
    const int maxScroll = std::max(0, contentH_ - bodyH_);
    if (k == Qt::Key_Down) scroll_ = std::min(maxScroll, scroll_ + 40);
    else if (k == Qt::Key_Up) scroll_ = std::max(0, scroll_ - 40);
}

void GameWidget::handleAccuseKey(int k) {
    const int n = static_cast<int>(accusable_.size());
    if (k == Qt::Key_Escape || k == Qt::Key_V) { mode_ = Mode::Explore; return; }
    if (k == Qt::Key_Up) accuseIdx_ = (accuseIdx_ + n - 1) % n;
    else if (k == Qt::Key_Down) accuseIdx_ = (accuseIdx_ + 1) % n;
    else if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        const Suspect* s = case_.suspect(accusable_[static_cast<size_t>(accuseIdx_)]);
        mode_ = Mode::Explore;
        if (game_->accuse(s->id()) == Verdict::Solved) { mode_ = Mode::Ending; gameFinished(true); return; }
        // Wrong suspect and "right suspect, not enough exposed lies" get the SAME answer, so guessing teaches nothing,
        // and a wrong accusation costs credibility.
        game_->penalize(2);
        say("Not enough proof (credibility -2)",
            "You accuse " + q(s->name()) + ", but the case will not hold up. Keep cross-examining people: catch the real "
            "liar with evidence that contradicts what they say.");
        checkFailure();
        if (mode_ != Mode::GameOver) playSfx(Sfx::Wrong);
    }
}

void GameWidget::handleDialKey(QKeyEvent* e) {
    const int k = e->key();
    if (k == Qt::Key_Escape) { mode_ = Mode::Explore; return; }
    if (k == Qt::Key_Backspace) { dialInput_.chop(1); dialWrong_ = false; return; }
    if (k >= Qt::Key_0 && k <= Qt::Key_9) {
        if (dialInput_.size() < 4) { dialInput_ += QChar('0' + (k - Qt::Key_0)); playSfx(Sfx::DialClick); }
        dialWrong_ = false;
        return;
    }
    if ((k == Qt::Key_Return || k == Qt::Key_Enter) && dialInput_.size() == 4) {
        if (dialInput_ == q(dialObj_.prop("code"))) {
            const Id cid = dialObj_.prop("clue_id");
            mode_ = Mode::Explore;
            if (game_->collect(cid) == Result::Ok) { refreshGates(); showClue(cid, Sfx::Door); }   // the iron door swings open
        } else {
            playSfx(Sfx::Wrong);
            dialWrong_ = true;
            dialInput_.clear();
        }
    }
}

// --- cross-examination
void GameWidget::handleTalkKey(int k) {
    const auto& st = game_->statements(talkSuspect_);
    const int n = static_cast<int>(st.size());
    auto show = [&](int idx) {
        talkIdx_ = idx;
        talkNote_.clear();
        if (game_->exposed(talkSuspect_, static_cast<size_t>(idx))) { talkLine_ = q(st[static_cast<size_t>(idx)].confession); talkTag_ = "LIE EXPOSED"; }
        else { talkLine_ = q(st[static_cast<size_t>(idx)].text); talkTag_.clear(); }
    };
    if (k == Qt::Key_Escape) { mode_ = Mode::Explore; return; }
    if (k == Qt::Key_Left || k == Qt::Key_A) show((talkIdx_ + n - 1) % n);
    else if (k == Qt::Key_Right || k == Qt::Key_D) show((talkIdx_ + 1) % n);
    else if (k == Qt::Key_P) {
        PressOutcome o = game_->press(talkSuspect_, static_cast<size_t>(talkIdx_));
        talkLine_ = q(o.text);
        talkTag_ = "PRESSED";
        talkNote_ = o.revealed.empty() ? QString() : "New evidence: " + q(case_.clue(o.revealed)->name()) + " (press Tab to read it)";
    } else if (k == Qt::Key_T || k == Qt::Key_Return || k == Qt::Key_Enter) {
        if (game_->dossier().empty()) { talkNote_ = "You have no evidence to present yet."; return; }
        presentIdx_ = 0;
        mode_ = Mode::Present;
    }
}

void GameWidget::handlePresentKey(int k) {
    const auto& list = game_->dossier();
    const int n = static_cast<int>(list.size());
    if (k == Qt::Key_Escape) { mode_ = Mode::Talk; return; }
    if (n == 0) { mode_ = Mode::Talk; return; }
    if (k == Qt::Key_Up) presentIdx_ = (presentIdx_ + n - 1) % n;
    else if (k == Qt::Key_Down) presentIdx_ = (presentIdx_ + 1) % n;
    else if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        PresentOutcome o = game_->present(talkSuspect_, static_cast<size_t>(talkIdx_), list[static_cast<size_t>(presentIdx_)]);
        mode_ = Mode::Talk;
        if (o.kind == Present::Exposed) {
            talkLine_ = q(o.text);
            talkTag_ = "LIE EXPOSED";
            gadbad();
            talkNote_ = o.revealed.empty() ? QString() : "New evidence: " + q(case_.clue(o.revealed)->name()) + " (press Tab to read it)";
            refreshGates();
        } else if (o.kind == Present::Wrong) {
            talkNote_ = "That does not contradict what they said. Credibility -1.";
            checkFailure();
            if (mode_ != Mode::GameOver) playSfx(Sfx::Wrong);
        } else {
            talkNote_ = q(o.text);
        }
    }
}

// --- forensic lab
void GameWidget::handleLabMenuKey(int k) {
    const auto& labs = case_.labs();
    const int n = static_cast<int>(labs.size());
    if (k == Qt::Key_Escape || k == Qt::Key_L) { mode_ = Mode::Explore; return; }
    if (n == 0) return;
    if (k == Qt::Key_Up) labIdx_ = (labIdx_ + n - 1) % n;
    else if (k == Qt::Key_Down) labIdx_ = (labIdx_ + 1) % n;
    else if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        const LabTask& t = labs[static_cast<size_t>(labIdx_)];
        labNote_.clear();
        if (game_->labDone(t.id)) labNote_ = "Already solved.";
        else if (!game_->labReady(t.id)) labNote_ = "You do not have the evidence for this test yet. Keep investigating.";
        else if (t.id == "lab_tod") { mode_ = Mode::LabTod; todField_ = 0; }
        else if (t.id == "lab_ink") mode_ = Mode::LabInk;
    }
}

void GameWidget::handleLabTodKey(int k) {
    if (k == Qt::Key_Escape) { mode_ = Mode::LabMenu; return; }
    if (k == Qt::Key_Left || k == Qt::Key_Right) todField_ = 1 - todField_;
    else if (k == Qt::Key_Up) { if (todField_ == 0) todHour_ = (todHour_ + 1) % 24; else todMinute_ = (todMinute_ + 5) % 60; }
    else if (k == Qt::Key_Down) { if (todField_ == 0) todHour_ = (todHour_ + 23) % 24; else todMinute_ = (todMinute_ + 55) % 60; }
    else if (k == Qt::Key_Return || k == Qt::Key_Enter) {
        if (todCorrect(tod_, todHour_ * 60 + todMinute_)) {
            game_->completeLab("lab_tod");
            mode_ = Mode::Explore;
            showClue("c_tod", Sfx::LabSuccess);
        } else {
            labNote_ = "That does not fit the numbers. How many hours does it take to cool from 37 to 29 degrees? Count back from 7:15 am.";
        }
    }
}

void GameWidget::handleLabInkKey(int k) {
    if (k == Qt::Key_Escape) { mode_ = Mode::LabMenu; return; }
    if (k >= Qt::Key_1 && k <= Qt::Key_3) {
        if (k - Qt::Key_1 == inkMatch(ink_)) {
            game_->completeLab("lab_ink");
            mode_ = Mode::Explore;
            showClue("c_ink_match", Sfx::LabSuccess);
        } else {
            labNote_ = "These Rf values do not match the forged ink. Work each one out: spot distance divided by solvent distance.";
        }
    }
}

// ---------------------------------------------------------------- drawing

void GameWidget::drawTile(QPainter& p, unsigned gid, const QRect& dst) {
    if (gid == 0) return;
    const TilesetInfo& ts = tiled_.tileset();
    if (tileset_.isNull() || ts.columns <= 0) { p.fillRect(dst, QColor(255, 0, 255)); return; }
    const int idx = static_cast<int>(gid) - ts.firstGid;
    const int col = idx % ts.columns, row = idx / ts.columns;
    QRect src(ts.margin + col * (ts.tileW + ts.spacing), ts.margin + row * (ts.tileH + ts.spacing), ts.tileW, ts.tileH);
    p.drawPixmap(dst, tileset_, src);
}

// Draws one frame of the sprite sheet: 4 directions x 4 walk frames per character, 64x96 pixels each,
// shown at 32x48 map units (so they are crisp at the 2x camera zoom). feetY is where the feet touch the floor.
void GameWidget::drawCharacter(QPainter& p, int sprite, int dir, int frame, float cx, float feetY) {
    const int fw = 64, fh = 96;
    QRectF src((dir * 4 + frame) * fw, sprite * fh, fw, fh);
    QRectF dst(cx - 16.0, feetY - 45.0, 32.0, 48.0);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawPixmap(dst, characters_, src);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
}

void GameWidget::drawWorld(QPainter& p) {
    const int tw = tiled_.tileW(), th = tiled_.tileH();
    for (const auto& layer : tiled_.tileLayers()) {
        if (!layer.visible || layer.name == "Collision") continue;
        for (int ty = 0; ty < tiled_.height(); ++ty)
            for (int tx = 0; tx < tiled_.width(); ++tx)
                drawTile(p, layer.data[static_cast<size_t>(ty * tiled_.width() + tx)], QRect(tx * tw, ty * th, tw, th));
    }
    // closed gates
    for (const auto& o : tiled_.objects()) {
        if (o.type != "dial" || game_->has(o.prop("clue_id")) || o.prop("tile").empty()) continue;
        const unsigned gid = static_cast<unsigned>(std::stoul(o.prop("tile")));
        for (int ty = static_cast<int>(o.y) / th; ty < static_cast<int>(o.y + o.h) / th; ++ty)
            for (int tx = static_cast<int>(o.x) / tw; tx < static_cast<int>(o.x + o.w) / tw; ++tx)
                drawTile(p, gid, QRect(tx * tw, ty * th, tw, th));
    }
    // glints on things worth inspecting
    const float pulse = 0.5f + 0.5f * std::sin(elapsed_ * 4.f);
    for (const auto& o : tiled_.objects()) {
        if (mode_ == Mode::Cutscene || (o.type != "clue" && o.type != "dial") || !objectActive(o, *game_)) continue;
        const float len = 4.f + 3.f * pulse;
        p.setPen(QPen(QColor(255, 236, 150, 130 + static_cast<int>(100 * pulse)), 2));
        p.drawLine(QPointF(o.cx() - len, o.cy()), QPointF(o.cx() + len, o.cy()));
        p.drawLine(QPointF(o.cx(), o.cy() - len), QPointF(o.cx(), o.cy() + len));
    }
    // people, drawn back-to-front so whoever is lower on the screen appears in front
    struct Actor { float feetY, cx; int sprite, dir, frame; bool pending; };
    std::vector<Actor> actors;
    if (mode_ == Mode::Cutscene) {                                // the scene's characters
        const float ct = cut_.time();
        for (const Track& tr : cut_.shot().tracks) {
            const Pose ps = poseAt(tr, ct);
            actors.push_back({ps.y, ps.x, spriteIndex(tr.sprite), ps.facing, ps.moving ? static_cast<int>(ct * 9.f) % 4 : 0, false});
        }
    } else {
        for (const auto& o : tiled_.objects()) {
            if (o.type != "npc") continue;
            std::string sp = o.prop("sprite");
            if (sp.empty()) sp = "raghav";
            const float cx = o.cx(), feet = o.cy() + 9.f;
            int dir = 0;                                   // people turn to face you when you come close
            const float dx = player_.x - cx, dy = player_.y - feet;
            if (std::sqrt(dx * dx + dy * dy) < 150.f) dir = std::fabs(dx) > std::fabs(dy) ? (dx < 0 ? 1 : 2) : (dy < 0 ? 3 : 0);
            const Id sid = o.prop("suspect_id");
            bool pending = false;                          // a small "!" over people who still have a lie you can catch
            for (size_t i = 0; i < game_->statements(sid).size(); ++i)
                if (game_->statements(sid)[i].isLie() && !game_->exposed(sid, i)) pending = true;
            actors.push_back({feet, cx, spriteIndex(sp), dir, 0, pending});
        }
        const int walkFrame = player_.moving ? static_cast<int>(player_.anim * 9.f) % 4 : 0;
        actors.push_back({player_.y + player_.half, player_.x, 0, player_.facing, walkFrame, false});
    }
    std::sort(actors.begin(), actors.end(), [](const Actor& a, const Actor& b) { return a.feetY < b.feetY; });
    for (const Actor& a : actors) {
        drawCharacter(p, a.sprite, a.dir, a.frame, a.cx, a.feetY);
        if (a.pending) {
            QFont f = p.font(); f.setPixelSize(13); f.setBold(true); p.setFont(f);
            p.setPen(QColor(255, 220, 90, 200));
            p.drawText(QPointF(a.cx - 3, a.feetY - 48 + 2 * std::sin(elapsed_ * 3.f)), "?");
        }
    }

    if (debug_) {
        QFont f = p.font(); f.setPixelSize(9); p.setFont(f);
        for (const auto& o : tiled_.objects()) {
            QColor c(255, 255, 255);
            if (o.type == "clue") c = QColor(255, 220, 0);
            else if (o.type == "door") c = QColor(255, 140, 0);
            else if (o.type == "npc") c = QColor(80, 160, 255);
            else if (o.type == "dial") c = QColor(255, 80, 200);
            else if (o.type == "spawn") c = QColor(60, 220, 90);
            p.setPen(QPen(c, 1.5));
            p.setBrush(QColor(c.red(), c.green(), c.blue(), 40));
            if (o.w > 0) p.drawRect(QRectF(o.x, o.y, o.w, o.h)); else p.drawEllipse(QPointF(o.x, o.y), 4, 4);
            p.setPen(Qt::white);
            p.drawText(QPointF(o.x + 1, o.y - 2), q(o.name));
        }
    }
}

void GameWidget::panel(QPainter& p, const QRect& r) {
    p.setPen(QPen(QColor(214, 170, 70), 2));
    p.setBrush(QColor(16, 18, 28, 238));
    p.drawRoundedRect(r, 10, 10);
}

void GameWidget::drawHearts(QPainter& p) {
    p.setPen(Qt::NoPen);
    const int total = 6, have = game_->credibility();
    for (int i = 0; i < total; ++i) {
        const float cx = width() - 28.f - i * 28.f, cy = 34.f, s = 10.f;
        QPainterPath path;
        path.moveTo(cx, cy + s * 0.9);
        path.cubicTo(cx - s * 1.3, cy - s * 0.1, cx - s * 0.7, cy - s * 1.1, cx, cy - s * 0.35);
        path.cubicTo(cx + s * 0.7, cy - s * 1.1, cx + s * 1.3, cy - s * 0.1, cx, cy + s * 0.9);
        p.setBrush(i < have ? QColor(222, 64, 84) : QColor(70, 60, 70, 160));
        p.drawPath(path);
    }
    QFont f = p.font(); f.setPixelSize(11); f.setBold(false); p.setFont(f);
    p.setPen(QColor(235, 225, 200, 220));
    p.drawText(QRect(width() - 6 * 28 - 40, 48, 6 * 28 + 30, 16), Qt::AlignRight, "Credibility");
}

void GameWidget::drawHud(QPainter& p, const MapObject* target) {
    QFont f = p.font(); f.setPixelSize(16); f.setBold(true); p.setFont(f);
    const QString loc = q(case_.location(game_->location())->name());
    const QString clues = QString("Evidence %1").arg(game_->dossier().size());
    QRect box(12, 12, 250, 52);
    panel(p, box);
    p.setPen(QColor(240, 230, 200));
    p.drawText(box.adjusted(14, 6, -8, -26), Qt::AlignLeft | Qt::AlignVCenter, loc);
    f.setBold(false); f.setPixelSize(13); p.setFont(f);
    p.setPen(QColor(200, 190, 160));
    p.drawText(box.adjusted(14, 28, -8, -4), Qt::AlignLeft | Qt::AlignVCenter, clues);

    drawObjective(p);                                             // the current objective (Settings: Goal hints)
    drawHearts(p);

    if (target && mode_ == Mode::Explore && !hasMessage()) {
        QString verb = target->type == "door" ? "Go" : target->type == "npc" ? "Talk to" : "Inspect";
        QString text = "E   " + verb + ": " + q(target->name);
        f.setPixelSize(15); p.setFont(f);
        QRect r(width() / 2 - 200, height() - 76, 400, 34);
        panel(p, r);
        p.setPen(QColor(255, 240, 190));
        p.drawText(r, Qt::AlignCenter, text);
    }
    f.setPixelSize(12); p.setFont(f);
    p.setPen(QColor(235, 235, 235, 190));
    p.drawText(14, height() - 12, "WASD move    E interact    Tab dossier    O objectives    L lab    V accuse    U undo    Esc menu");
}

void GameWidget::drawMessage(QPainter& p) {
    const int m = 28, h = std::max(190, height() / 3);
    QRect r(m, height() - h - 16, width() - 2 * m, h);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(18); f.setBold(true); p.setFont(f);
    p.setPen(QColor(255, 214, 110));
    p.drawText(r.adjusted(20, 14, -20, 0), Qt::AlignLeft | Qt::AlignTop, msgTitle_);
    f.setBold(false); f.setPixelSize(15); p.setFont(f);
    p.setPen(QColor(240, 240, 240));
    p.drawText(r.adjusted(20, 46, -20, -30), kWrapLeftTop, msgBody_);
    f.setPixelSize(12); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r.adjusted(20, 0, -20, -8), Qt::AlignRight | Qt::AlignBottom, "Press E or any walking key to continue");
}

void GameWidget::drawTalk(QPainter& p) {
    p.fillRect(rect(), QColor(0, 0, 0, 175));
    QRect r = rect().adjusted(width() / 12, height() / 10, -width() / 12, -height() / 10);
    panel(p, r);
    const Suspect* s = case_.suspect(talkSuspect_);
    const auto& st = game_->statements(talkSuspect_);
    // portrait
    QRectF src(0, talkSprite_ * 96, 64, 96);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawPixmap(QRectF(r.left() + 34, r.top() + 50, 160, 240), characters_, src);
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    QFont f = p.font(); f.setPixelSize(20); f.setBold(true); p.setFont(f);
    p.setPen(QColor(255, 214, 110));
    p.drawText(QRect(r.left() + 20, r.top() + 300, 200, 56), kWrapHCenterTop, q(s->name()));
    // statement markers
    QRect right(r.left() + 240, r.top() + 24, r.width() - 270, r.height() - 48);
    f.setPixelSize(13); f.setBold(false); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(QRect(right.left(), right.top(), right.width(), 20), Qt::AlignLeft,
               QString("Statement %1 of %2").arg(talkIdx_ + 1).arg(st.size()));
    for (int i = 0; i < static_cast<int>(st.size()); ++i) {
        const bool ex = game_->exposed(talkSuspect_, static_cast<size_t>(i));
        p.setPen(Qt::NoPen);
        p.setBrush(ex ? QColor(222, 64, 84) : (i == talkIdx_ ? QColor(255, 214, 110) : QColor(120, 112, 100)));
        p.drawEllipse(QPointF(right.right() - 14 - (static_cast<int>(st.size()) - 1 - i) * 22, right.top() + 9), 7, 7);
    }
    // speech
    QRect bubble(right.left(), right.top() + 34, right.width(), right.height() - 190);
    p.setPen(QPen(QColor(120, 112, 100), 1.5));
    p.setBrush(QColor(34, 36, 50, 230));
    p.drawRoundedRect(bubble, 12, 12);
    if (!talkTag_.isEmpty()) {
        f.setPixelSize(13); f.setBold(true); p.setFont(f);
        p.setPen(talkTag_ == "LIE EXPOSED" ? QColor(255, 110, 110) : QColor(170, 210, 255));
        p.drawText(bubble.adjusted(16, 10, -16, 0), Qt::AlignLeft | Qt::AlignTop, talkTag_);
    }
    f.setPixelSize(18); f.setBold(false); p.setFont(f);
    p.setPen(QColor(244, 244, 244));
    p.drawText(bubble.adjusted(18, 34, -18, -12), kWrapLeftTop, talkLine_);
    // notes + controls
    if (!talkNote_.isEmpty()) {
        f.setPixelSize(15); f.setBold(true); p.setFont(f);
        p.setPen(talkNote_.startsWith("New evidence") ? QColor(150, 230, 160) : QColor(255, 140, 120));
        p.drawText(QRect(right.left(), bubble.bottom() + 12, right.width(), 60), kWrapLeftTop, talkNote_);
    }
    f.setPixelSize(14); f.setBold(false); p.setFont(f);
    p.setPen(QColor(214, 170, 70));
    p.drawText(QRect(right.left(), right.bottom() - 80, right.width(), 80), kWrapLeftTop,
               "Left / Right: choose a statement\nP: press (ask for more detail)\n"
               "T: present evidence that contradicts it    Esc: leave");
}

void GameWidget::drawPresent(QPainter& p) {
    p.fillRect(rect(), QColor(0, 0, 0, 190));
    QRect r = rect().adjusted(width() / 12, height() / 10, -width() / 12, -height() / 10);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(22); f.setBold(true); p.setFont(f);
    p.setPen(QColor(255, 214, 110));
    p.drawText(r.adjusted(0, 14, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "Which evidence contradicts this statement?");
    const auto& list = game_->dossier();
    const int rowH = 32, visible = std::max(3, (r.height() - 220) / rowH);
    const int first = std::max(0, std::min(presentIdx_ - visible / 2, static_cast<int>(list.size()) - visible));
    for (int i = first; i < static_cast<int>(list.size()) && i < first + visible; ++i) {
        QRect row(r.left() + 24, r.top() + 60 + (i - first) * rowH, r.width() / 2 - 40, rowH - 4);
        if (i == presentIdx_) { p.setPen(Qt::NoPen); p.setBrush(QColor(214, 170, 70, 80)); p.drawRoundedRect(row, 6, 6); }
        f.setPixelSize(15); f.setBold(i == presentIdx_); p.setFont(f);
        p.setPen(i == presentIdx_ ? QColor(255, 240, 190) : QColor(220, 220, 220));
        p.drawText(row.adjusted(10, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, q(case_.clue(list[static_cast<size_t>(i)])->name()));
    }
    if (!list.empty()) {
        QRect info(r.left() + r.width() / 2, r.top() + 60, r.width() / 2 - 24, r.height() - 150);
        f.setPixelSize(15); f.setBold(false); p.setFont(f);
        p.setPen(QColor(235, 235, 235));
        p.drawText(info, kWrapLeftTop, q(case_.clue(list[static_cast<size_t>(presentIdx_)])->description()));
    }
    const auto& st = game_->statements(talkSuspect_);
    f.setPixelSize(14); p.setFont(f);
    p.setPen(QColor(255, 140, 120));
    p.drawText(QRect(r.left() + 24, r.bottom() - 84, r.width() - 48, 50), kWrapLeftTop,
               "Their statement: \"" + q(st[static_cast<size_t>(talkIdx_)].text) + "\"");
    p.setPen(QColor(200, 190, 150));
    p.drawText(r.adjusted(24, 0, -24, -10), Qt::AlignLeft | Qt::AlignBottom, "Up/Down: choose    Enter: present    Esc: back.   A wrong piece of evidence costs credibility.");
}

void GameWidget::drawLabMenu(QPainter& p) {
    p.fillRect(rect(), QColor(0, 0, 0, 180));
    QRect r(width() / 2 - 340, height() / 2 - 220, 680, 440);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(24); f.setBold(true); p.setFont(f);
    p.setPen(QColor(150, 220, 255));
    p.drawText(r.adjusted(0, 16, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "Dr. Reddy's forensic kit");
    const auto& labs = case_.labs();
    for (int i = 0; i < static_cast<int>(labs.size()); ++i) {
        const LabTask& t = labs[static_cast<size_t>(i)];
        QRect row(r.left() + 30, r.top() + 76 + i * 64, r.width() - 60, 56);
        if (i == labIdx_) { p.setPen(Qt::NoPen); p.setBrush(QColor(120, 200, 240, 60)); p.drawRoundedRect(row, 8, 8); }
        f.setPixelSize(17); f.setBold(true); p.setFont(f);
        p.setPen(QColor(240, 240, 240));
        p.drawText(row.adjusted(14, 4, 0, -24), Qt::AlignLeft | Qt::AlignVCenter, q(t.title));
        const bool done = game_->labDone(t.id), ready = game_->labReady(t.id);
        f.setPixelSize(13); f.setBold(false); p.setFont(f);
        p.setPen(done ? QColor(150, 230, 160) : ready ? QColor(255, 220, 100) : QColor(170, 170, 170));
        p.drawText(row.adjusted(14, 28, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
                   done ? "Solved" : ready ? "Ready to test. Press Enter." : "Waiting for more evidence");
    }
    if (!labNote_.isEmpty()) {
        f.setPixelSize(14); p.setFont(f);
        p.setPen(QColor(255, 180, 120));
        p.drawText(QRect(r.left() + 30, r.bottom() - 90, r.width() - 60, 50), kWrapLeftTop, labNote_);
    }
    f.setPixelSize(12); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r.adjusted(24, 0, -24, -10), Qt::AlignLeft | Qt::AlignBottom, "Up/Down: choose    Enter: run the test    Esc: close");
}

void GameWidget::drawLabTod(QPainter& p) {
    p.fillRect(rect(), QColor(0, 0, 0, 185));
    QRect r(width() / 2 - 360, height() / 2 - 250, 720, 500);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(24); f.setBold(true); p.setFont(f);
    p.setPen(QColor(150, 220, 255));
    p.drawText(r.adjusted(0, 16, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "Biology: when did he die?");
    f.setPixelSize(16); f.setBold(false); p.setFont(f);
    p.setPen(QColor(240, 240, 240));
    p.drawText(r.adjusted(34, 66, -34, 0), kWrapLeftTop,
               "At 7:15 in the morning Dr. Reddy measured Dayanand's body temperature: 29 degrees C.\n"
               "A living body is 37 degrees C.\n\n"
               "Doctor's rule: after death a body cools by about 1 degree C every hour.\n\n"
               "Work out how many hours ago he died, then set the time of death.");
    f.setPixelSize(54); f.setBold(true); p.setFont(f);
    const QString hh = QString("%1").arg(todHour_, 2, 10, QChar('0')), mm = QString("%1").arg(todMinute_, 2, 10, QChar('0'));
    QRect hr(r.center().x() - 120, r.top() + 290, 100, 70), mr(r.center().x() + 20, r.top() + 290, 100, 70);
    p.setPen(QPen(todField_ == 0 ? QColor(255, 214, 110) : QColor(100, 96, 90), 3)); p.setBrush(QColor(30, 32, 46)); p.drawRoundedRect(hr, 8, 8);
    p.setPen(QPen(todField_ == 1 ? QColor(255, 214, 110) : QColor(100, 96, 90), 3)); p.drawRoundedRect(mr, 8, 8);
    p.setPen(QColor(255, 240, 190));
    p.drawText(hr, Qt::AlignCenter, hh); p.drawText(mr, Qt::AlignCenter, mm);
    p.drawText(QRect(r.center().x() - 18, r.top() + 290, 36, 70), Qt::AlignCenter, ":");
    f.setPixelSize(14); f.setBold(false); p.setFont(f);
    p.setPen(QColor(255, 180, 120));
    p.drawText(QRect(r.left() + 34, r.top() + 380, r.width() - 68, 60), kWrapLeftTop, labNote_);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r.adjusted(24, 0, -24, -10), Qt::AlignLeft | Qt::AlignBottom, "Left/Right: hours or minutes    Up/Down: change (24-hour clock)    Enter: submit    Esc: back");
}

void GameWidget::drawLabInk(QPainter& p) {
    p.fillRect(rect(), QColor(0, 0, 0, 185));
    QRect r(width() / 2 - 420, height() / 2 - 270, 840, 540);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(24); f.setBold(true); p.setFont(f);
    p.setPen(QColor(150, 220, 255));
    p.drawText(r.adjusted(0, 14, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "Chemistry: whose pen forged the signature?");
    f.setPixelSize(14); f.setBold(false); p.setFont(f);
    p.setPen(QColor(240, 240, 240));
    p.drawText(r.adjusted(30, 52, -30, 0), kWrapLeftTop,
               "Paper chromatography: a drop of ink is placed at the start line and a solvent climbs the paper. Each ink travels a "
               "fixed fraction of the way:   Rf = distance moved by the spot / distance moved by the solvent.   The same ink always gives the same Rf.");
    std::vector<InkSample> all;
    all.push_back(ink_.forged);
    for (const auto& pen : ink_.pens) all.push_back(pen);
    const double scale = 26.0;                       // pixels per centimetre
    const int baseY = r.top() + 410, colW = (r.width() - 60) / 4;
    for (size_t i = 0; i < all.size(); ++i) {
        const int cx = r.left() + 30 + static_cast<int>(i) * colW + colW / 2;
        const double h = all[i].frontCm * scale;
        QRectF paper(cx - 38, baseY - h - 10, 76, h + 20);
        p.setPen(QPen(QColor(150, 140, 120), 1.5)); p.setBrush(QColor(246, 242, 228)); p.drawRect(paper);
        p.setPen(QPen(QColor(120, 120, 130), 1, Qt::DashLine)); p.drawLine(QPointF(cx - 38, baseY), QPointF(cx + 38, baseY));
        p.setPen(QPen(QColor(70, 130, 220), 1.5)); p.drawLine(QPointF(cx - 38, baseY - h), QPointF(cx + 38, baseY - h));
        p.setPen(Qt::NoPen); p.setBrush(QColor(60, 50, 140)); p.drawEllipse(QPointF(cx, baseY - all[i].spotCm * scale), 9, 7);
        f.setPixelSize(12); f.setBold(i == 0); p.setFont(f);
        p.setPen(i == 0 ? QColor(255, 214, 110) : QColor(235, 235, 235));
        p.drawText(QRect(cx - colW / 2 + 4, baseY + 14, colW - 8, 40), kWrapHCenterTop,
                   (i == 0 ? QString("FORGED\n") : QString("[%1]\n").arg(i)) + q(all[i].label));
        f.setBold(false); p.setFont(f);
        p.setPen(QColor(190, 210, 240));
        p.drawText(QRect(cx - colW / 2 + 4, baseY + 62, colW - 8, 34), kWrapHCenterTop,
                   QString("spot %1 cm\nsolvent %2 cm").arg(all[i].spotCm, 0, 'f', 2).arg(all[i].frontCm, 0, 'f', 2));
    }
    f.setPixelSize(14); f.setBold(true); p.setFont(f);
    p.setPen(QColor(255, 180, 120));
    p.drawText(QRect(r.left() + 30, r.top() + 104, r.width() - 60, 40), kWrapLeftTop, labNote_);
    f.setBold(false); f.setPixelSize(12); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r.adjusted(24, 0, -24, -10), Qt::AlignLeft | Qt::AlignBottom, "Work out each Rf, then press 1, 2 or 3 for the pen that matches the forged ink.    Esc: back");
}

void GameWidget::drawDossier(QPainter& p) {
    p.fillRect(rect(), QColor(0, 0, 0, 150));
    QRect r = rect().adjusted(width() / 14, height() / 14, -width() / 14, -height() / 14);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(20); f.setBold(true); p.setFont(f);
    p.setPen(QColor(255, 214, 110));
    p.drawText(r.adjusted(24, 14, -24, 0), Qt::AlignLeft | Qt::AlignTop,
               dossierPage_ == 0 ? QString("Dossier: evidence (%1)").arg(game_->dossier().size()) : "Dossier: people and their statements");
    f.setPixelSize(12); f.setBold(false); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r.adjusted(24, 0, -24, -10), Qt::AlignLeft | Qt::AlignBottom, "Tab: next page    Up/Down: scroll    Esc: close");

    QRect body = r.adjusted(24, 52, -24, -34);
    bodyH_ = body.height();
    p.setClipRect(body);
    int y = body.top() - scroll_;
    const int start = y;
    auto block = [&](const QString& title, const QString& text, const QColor& titleColor) {
        QFont tf = p.font(); tf.setPixelSize(16); tf.setBold(true); p.setFont(tf);
        p.setPen(titleColor);
        p.drawText(QRect(body.left(), y, body.width(), 24), Qt::AlignLeft | Qt::AlignTop, title);
        y += 24;
        QFont bf = p.font(); bf.setPixelSize(14); bf.setBold(false); p.setFont(bf);
        p.setPen(QColor(235, 235, 235));
        QRect br = p.boundingRect(QRect(body.left(), y, body.width(), 5000), kWrapLeft, text);
        p.drawText(br, kWrapLeft, text);
        y += br.height() + 16;
    };
    if (dossierPage_ == 0) {
        if (game_->dossier().empty()) block("Nothing yet", "Explore the house. Look for objects that glint, and talk to everyone.", QColor(200, 200, 200));
        for (const Id& id : game_->dossier()) block(q(case_.clue(id)->name()), q(case_.clue(id)->description()), QColor(255, 230, 150));
    } else {
        std::vector<Id> people;
        for (const auto& kv : case_.suspects()) people.push_back(kv.first);
        std::sort(people.begin(), people.end());
        for (const Id& id : people) {
            const Suspect* s = case_.suspect(id);
            const auto& st = game_->statements(id);
            QString text = q(s->description());
            for (size_t i = 0; i < st.size(); ++i) {
                QString mark = game_->exposed(id, i) ? "[LIE CAUGHT] " : st[i].isLie() ? "[?] " : "[...] ";
                text += "\n" + mark + "\"" + q(st[i].text) + "\"";
            }
            const int caught = game_->exposedLies(id);
            block(q(s->name()) + (s->accusable() ? "" : "  (witness)"), text, caught > 0 ? QColor(255, 130, 120) : QColor(160, 210, 255));
        }
    }
    contentH_ = y - start;
    p.setClipping(false);
}

void GameWidget::drawAccuse(QPainter& p) {
    p.fillRect(rect(), QColor(0, 0, 0, 170));
    QRect r(width() / 2 - 300, height() / 2 - 220, 600, 440);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(22); f.setBold(true); p.setFont(f);
    p.setPen(QColor(255, 214, 110));
    p.drawText(r.adjusted(0, 16, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "Who killed Dayanand Rastogi?");
    for (int i = 0; i < static_cast<int>(accusable_.size()); ++i) {
        const Suspect* s = case_.suspect(accusable_[static_cast<size_t>(i)]);
        QRect row(r.left() + 30, r.top() + 66 + i * 52, r.width() - 60, 46);
        if (i == accuseIdx_) { p.setPen(Qt::NoPen); p.setBrush(QColor(214, 170, 70, 70)); p.drawRoundedRect(row, 6, 6); }
        f.setPixelSize(17); f.setBold(i == accuseIdx_); p.setFont(f);
        p.setPen(i == accuseIdx_ ? QColor(255, 240, 190) : QColor(220, 220, 220));
        p.drawText(row.adjusted(12, 0, 0, -16), Qt::AlignLeft | Qt::AlignVCenter, q(s->name()));
        f.setPixelSize(12); f.setBold(false); p.setFont(f);
        p.setPen(QColor(170, 170, 180));
        p.drawText(row.adjusted(12, 20, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, QString("Lies caught: %1").arg(game_->exposedLies(s->id())));
    }
    f.setPixelSize(12); f.setBold(false); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r.adjusted(0, 0, 0, -12), Qt::AlignHCenter | Qt::AlignBottom, "Up/Down choose    Enter accuse    Esc cancel.   A wrong accusation costs 2 credibility.");
}

void GameWidget::drawDial(QPainter& p) {
    p.fillRect(rect(), QColor(0, 0, 0, 170));
    QRect r(width() / 2 - 230, height() / 2 - 150, 460, 300);
    panel(p, r);
    QFont f = p.font(); f.setPixelSize(22); f.setBold(true); p.setFont(f);
    p.setPen(QColor(255, 214, 110));
    p.drawText(r.adjusted(0, 16, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "The iron door");
    f.setPixelSize(15); f.setBold(false); p.setFont(f);
    p.setPen(QColor(235, 235, 235));
    p.drawText(r.adjusted(24, 56, -24, 0), kWrapHCenterTop,
               "The dial wants a year in the English calendar. Four digits, then Enter.");
    f.setPixelSize(44); f.setBold(true); p.setFont(f);
    QString shown;
    for (int i = 0; i < 4; ++i) shown += (i < dialInput_.size() ? QString(dialInput_[i]) : QString("_")) + (i < 3 ? "  " : "");
    p.setPen(QColor(255, 240, 190));
    p.drawText(r.adjusted(0, 120, 0, 0), Qt::AlignHCenter | Qt::AlignTop, shown);
    f.setPixelSize(14); f.setBold(false); p.setFont(f);
    p.setPen(dialWrong_ ? QColor(255, 130, 120) : QColor(200, 190, 150));
    p.drawText(r.adjusted(0, 0, 0, -16), Qt::AlignHCenter | Qt::AlignBottom,
               dialWrong_ ? "Click. Nothing happens. Wrong year." : "Esc to step back");
}

// A rubber stamp that lands once: large and faint at first, then at its size, tilted, in a ruled box.
void GameWidget::drawStamp(QPainter& p, const QPointF& centre, const QString& text, const QColor& colour) {
    const float k = stampScale(endTime_);
    QColor c = colour;
    c.setAlpha(static_cast<int>(255 * std::min(1.f, endTime_ / 0.2f)));
    p.save();
    p.translate(centre);
    p.rotate(-6);
    p.scale(k, k);
    QFont f = p.font(); f.setFamily("Georgia"); f.setPixelSize(40); f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 4); p.setFont(f);
    const QRectF box(-240, -34, 480, 68);
    p.setPen(QPen(c, 4));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(box, 6, 6);
    p.drawText(box, Qt::AlignCenter, text);
    p.restore();
}

void GameWidget::drawEnding(QPainter& p) {
    p.fillRect(rect(), QColor(8, 10, 18, 245));
    const QRect r = rect().adjusted(width() / 8, height() / 9, -width() / 8, -height() / 9);
    drawStamp(p, QPointF(width() / 2.0, r.top() + 40), "CASE CLOSED", QColor(214, 64, 64));
    QFont f = p.font(); f.setFamily("Georgia"); f.setPixelSize(18); f.setBold(false); p.setFont(f);
    p.setPen(QColor(235, 235, 235));
    const std::string story = kEnding;                                       // the true story, typed out
    p.drawText(r.adjusted(0, 110, 0, -60), kWrapLeftTop, q(story.substr(0, typedChars(story, endTime_, 1.0f))));
    f.setPixelSize(15); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r, Qt::AlignHCenter | Qt::AlignBottom,
               QString("Evidence found: %1      Credibility left: %2 of 6\n"
                       "T: the night, minute by minute      R: play again      Esc: title      Enter: show all")
                   .arg(game_->dossier().size()).arg(game_->credibility()));

    // The detective's rating, which appears once the stamp has landed.
    const float ra = std::clamp((endTime_ - 0.8f) / 0.5f, 0.f, 1.f);
    if (ra > 0.f) {
        const Rating rt = rateDetective(*game_, case_);
        const int a = static_cast<int>(255 * ra);
        const QPointF c(r.right() - 46.0, r.top() + 40.0);
        p.save();
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(214, 170, 70, a), 3));
        p.setBrush(QColor(30, 26, 18, a));
        p.drawEllipse(c, 34.0, 34.0);
        QFont g = p.font(); g.setFamily("Georgia"); g.setPixelSize(38); g.setBold(true); p.setFont(g);
        p.setPen(QColor(240, 215, 140, a));
        p.drawText(QRectF(c.x() - 34, c.y() - 34, 68, 68), Qt::AlignCenter, QString::fromLatin1(&rt.grade, 1));
        g.setPixelSize(12); g.setBold(false); p.setFont(g);
        p.setPen(QColor(220, 210, 180, a));
        p.drawText(QRectF(c.x() - 90, c.y() + 38, 180, 18), Qt::AlignHCenter | Qt::AlignTop,
                   QString("%1  (%2/100)").arg(q(rt.title)).arg(rt.score));
        p.restore();
    }
}

void GameWidget::drawGameOver(QPainter& p) {
    p.fillRect(rect(), QColor(20, 6, 8, 245));
    const QRect r = rect().adjusted(width() / 6, height() / 5, -width() / 6, -height() / 5);
    drawStamp(p, QPointF(width() / 2.0, r.top() + 40), "CASE REASSIGNED", QColor(235, 100, 100));
    QFont f = p.font(); f.setFamily("Georgia"); f.setPixelSize(18); f.setBold(false); p.setFont(f);
    p.setPen(QColor(235, 235, 235));
    const std::string text =
        "You made too many accusations without proof, and the senior officer has taken you off the case. "
        "The family will sign it off as natural causes.\n\nThink before you present evidence: it must truly contradict "
        "what the person said.";
    p.drawText(r.adjusted(0, 110, 0, -40), kWrapHCenterTop, q(text.substr(0, typedChars(text, endTime_, 1.0f))));
    f.setPixelSize(15); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r, Qt::AlignHCenter | Qt::AlignBottom, "R: start again      Esc: title      Enter: show all");
}

void GameWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(14, 15, 22));
    p.translate(shake_.x(), shake_.y());                      // the shake when a lie is caught
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);  // crisp pixel art when scaled

    p.save();
    const float mapW = static_cast<float>(tiled_.width() * tiled_.tileW());
    const float mapH = static_cast<float>(tiled_.height() * tiled_.tileH());
    // Zoom in so the art is readable; the camera follows the player and stops at the map edges.
    const float fit = std::min(width() / mapW, height() / mapH);
    const float sc = std::max(fit, 2.0f);
    float offX = (width() - mapW * sc) / 2, offY = (height() - mapH * sc) / 2;   // map smaller than window: centre it
    if (mapW * sc > width()) offX = std::min(0.f, std::max(width() - mapW * sc, width() / 2.f - player_.x * sc));
    if (mapH * sc > height()) offY = std::min(0.f, std::max(height() - mapH * sc, height() / 2.f - player_.y * sc));
    p.translate(offX, offY);
    p.scale(sc, sc);
    drawWorld(p);
    p.restore();

    const bool cinematic = (mode_ == Mode::Title || mode_ == Mode::Intro || mode_ == Mode::Story || mode_ == Mode::Reconstruction ||
                            mode_ == Mode::Cutscene);
    if (atmosphere_ && !cinematic) drawAtmosphere(p, QPointF(offX + player_.x * sc, offY + player_.y * sc));
    if (roomFade_.active()) p.fillRect(rect(), QColor(0, 0, 0, static_cast<int>(255 * roomFade_.remaining())));
    const MapObject* target = nearestInteractable(tiled_, *game_, player_.x, player_.y);
    if (!cinematic) { drawHud(p, target); drawBanner(p); }
    if (showChecklist_ && mode_ == Mode::Explore) drawChecklist(p);
    switch (mode_) {
        case Mode::Dossier: drawDossier(p); break;
        case Mode::Accuse: drawAccuse(p); break;
        case Mode::Dial: drawDial(p); break;
        case Mode::Talk: drawTalk(p); break;
        case Mode::Present: drawPresent(p); break;
        case Mode::LabMenu: drawLabMenu(p); break;
        case Mode::LabTod: drawLabTod(p); break;
        case Mode::LabInk: drawLabInk(p); break;
        case Mode::Ending: drawEnding(p); break;
        case Mode::GameOver: drawGameOver(p); break;
        case Mode::Title: drawMenu(p, "The Detective's Dossier", "The Old Way", "Up/Down: choose    Enter: select"); break;
        case Mode::Pause: drawMenu(p, "Paused", "", "Up/Down: choose    Enter: select    Esc: resume"); break;
        case Mode::Settings: drawMenu(p, "Settings", "", "Up/Down: choose    Left/Right or Enter: change    Esc: back"); break;
        case Mode::Help: drawHelp(p); break;
        case Mode::Intro: case Mode::Story: case Mode::Reconstruction: drawCinematic(p); break;
        case Mode::Cutscene: drawCutsceneOverlay(p); break;
        case Mode::Explore: if (hasMessage()) drawMessage(p); break;
    }
    drawEffects(p);
}
