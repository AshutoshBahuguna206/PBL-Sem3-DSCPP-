#include "GameWidget.h"
#include <QDebug>
#include <QFocusEvent>
#include <QFont>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>
#include "MapCheck.h"

#ifndef ASSET_DIR
#define ASSET_DIR "assets"
#endif

namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }

// Text flags as plain ints (drawText takes an int), so we never mix Qt's flag types.
constexpr int kWrapLeftTop = static_cast<int>(Qt::AlignLeft) | static_cast<int>(Qt::AlignTop) | static_cast<int>(Qt::TextWordWrap);
constexpr int kWrapLeft = static_cast<int>(Qt::AlignLeft) | static_cast<int>(Qt::TextWordWrap);
constexpr int kWrapHCenterTop = static_cast<int>(Qt::AlignHCenter) | static_cast<int>(Qt::AlignTop) | static_cast<int>(Qt::TextWordWrap);

// Row order in assets/sprites/characters.png (see tools/make_mansion_assets.py).
const char* kSpriteNames[] = {"detective", "raghav", "reddy", "shanta", "rohit", "meera", "kabir"};
int spriteIndex(const std::string& name) {
    for (int i = 0; i < 7; ++i) if (name == kSpriteNames[i]) return i;
    return 1;
}

const char* kIntro =
    "Rastogi Kothi, Mussoorie. Seven in the morning.\n\n"
    "Dayanand Rastogi, 76, is dead in his locked library. Last night's landslide buried the only road, so whoever "
    "did this is still in the house. The family wants it called natural causes. You do not.\n\n"
    "Question everyone. When someone lies, press T and show them evidence that contradicts it. Dr. Reddy's forensic "
    "lab (press L) can tell you what the body and the ink are hiding.\n\n"
    "WASD move    E interact or talk    Tab dossier    L lab    V accuse";

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
    for (const auto& kv : case_.suspects()) if (kv.second.accusable()) accusable_.push_back(kv.first);
    std::sort(accusable_.begin(), accusable_.end());

    setWindowTitle("The Detective's Dossier: The Old Way");
    setMinimumSize(800, 600);
    setFocusPolicy(Qt::StrongFocus);
    loadLocation(game_->location(), "");
    say("The Old Way", q(kIntro));
    connect(&timer_, &QTimer::timeout, this, [this] { tick(); });
    timer_.start(16);
    clock_.start();
}

void GameWidget::restart() {
    game_ = std::make_unique<Game>(case_);
    mode_ = Mode::Explore;
    keys_.clear();
    scroll_ = 0;
    loadLocation(game_->location(), "");
    say("The Old Way", q(kIntro));
}

void GameWidget::loadLocation(const Id& id, const Id& from) {
    tiled_ = TiledMap::fromFile(std::string(ASSET_DIR) + "/maps/" + id + ".json");
    collision_ = tiled_.collisionMap();
    for (const auto& w : checkMap(tiled_, case_, id)) qWarning().noquote() << "MAP WARNING (" << q(id) << "):" << q(w);
    auto pt = arrivalPoint(tiled_, collision_, from, player_);
    player_.x = pt.first;
    player_.y = pt.second;
    refreshGates();
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
    if (game_->failed()) mode_ = Mode::GameOver;
}

void GameWidget::tick() {
    float dt = std::min(clock_.restart() / 1000.0f, 0.05f);  // clamp so a lag spike can't teleport us through walls
    elapsed_ += dt;
    if (mode_ == Mode::Explore && !hasMessage()) {
        float dx = (down(Qt::Key_D, Qt::Key_Right) ? 1.f : 0.f) - (down(Qt::Key_A, Qt::Key_Left) ? 1.f : 0.f);
        float dy = (down(Qt::Key_S, Qt::Key_Down) ? 1.f : 0.f) - (down(Qt::Key_W, Qt::Key_Up) ? 1.f : 0.f);
        player_.move(dx, dy, dt, collision_);
    }
    update();
}

// ---------------------------------------------------------------- actions

void GameWidget::showClue(const Id& clueId) {
    const Clue* k = case_.clue(clueId);
    if (!k) return;
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
        if (r == Result::Ok) { loadLocation(game_->location(), from); return; }
        const std::string hint = o->prop("locked_hint");
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
        case Mode::Ending: if (k == Qt::Key_R) restart(); return;
        case Mode::GameOver: if (k == Qt::Key_R) restart(); return;
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
        if (game_->accuse(s->id()) == Verdict::Solved) { mode_ = Mode::Ending; return; }
        // Wrong suspect and "right suspect, not enough exposed lies" get the SAME answer, so guessing teaches nothing,
        // and a wrong accusation costs credibility.
        game_->penalize(2);
        say("Not enough proof (credibility -2)",
            "You accuse " + q(s->name()) + ", but the case will not hold up. Keep cross-examining people: catch the real "
            "liar with evidence that contradicts what they say.");
        checkFailure();
    }
}

void GameWidget::handleDialKey(QKeyEvent* e) {
    const int k = e->key();
    if (k == Qt::Key_Escape) { mode_ = Mode::Explore; return; }
    if (k == Qt::Key_Backspace) { dialInput_.chop(1); dialWrong_ = false; return; }
    if (k >= Qt::Key_0 && k <= Qt::Key_9) {
        if (dialInput_.size() < 4) dialInput_ += QChar('0' + (k - Qt::Key_0));
        dialWrong_ = false;
        return;
    }
    if ((k == Qt::Key_Return || k == Qt::Key_Enter) && dialInput_.size() == 4) {
        if (dialInput_ == q(dialObj_.prop("code"))) {
            const Id cid = dialObj_.prop("clue_id");
            mode_ = Mode::Explore;
            if (game_->collect(cid) == Result::Ok) { refreshGates(); showClue(cid); }
        } else {
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
            talkNote_ = o.revealed.empty() ? QString() : "New evidence: " + q(case_.clue(o.revealed)->name()) + " (press Tab to read it)";
            refreshGates();
        } else if (o.kind == Present::Wrong) {
            talkNote_ = "That does not contradict what they said. Credibility -1.";
            checkFailure();
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
            showClue("c_tod");
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
            showClue("c_ink_match");
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
        if ((o.type != "clue" && o.type != "dial") || !objectActive(o, *game_)) continue;
        const float len = 4.f + 3.f * pulse;
        p.setPen(QPen(QColor(255, 236, 150, 130 + static_cast<int>(100 * pulse)), 2));
        p.drawLine(QPointF(o.cx() - len, o.cy()), QPointF(o.cx() + len, o.cy()));
        p.drawLine(QPointF(o.cx(), o.cy() - len), QPointF(o.cx(), o.cy() + len));
    }
    // people, drawn back-to-front so whoever is lower on the screen appears in front
    struct Actor { float feetY, cx; int sprite, dir, frame; bool pending; };
    std::vector<Actor> actors;
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

    // current goal
    QRect goal(12, 70, 380, 50);
    panel(p, goal);
    f.setPixelSize(11); f.setBold(true); p.setFont(f);
    p.setPen(QColor(214, 170, 70));
    p.drawText(goal.adjusted(12, 5, -8, 0), Qt::AlignLeft | Qt::AlignTop, "GOAL");
    f.setBold(false); f.setPixelSize(13); p.setFont(f);
    p.setPen(QColor(240, 240, 240));
    p.drawText(goal.adjusted(12, 20, -8, -4), kWrapLeftTop, q(currentObjective(*game_)));
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
    p.drawText(14, height() - 12, "WASD move    E interact    Tab dossier    L lab    V accuse");
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

void GameWidget::drawEnding(QPainter& p) {
    p.fillRect(rect(), QColor(8, 10, 18, 245));
    QRect r = rect().adjusted(width() / 8, height() / 9, -width() / 8, -height() / 9);
    QFont f = p.font(); f.setPixelSize(34); f.setBold(true); p.setFont(f);
    p.setPen(QColor(255, 214, 110));
    p.drawText(r, Qt::AlignHCenter | Qt::AlignTop, "CASE CLOSED");
    f.setPixelSize(17); f.setBold(false); p.setFont(f);
    p.setPen(QColor(235, 235, 235));
    p.drawText(r.adjusted(0, 70, 0, -60), kWrapLeftTop, q(kEnding));
    f.setPixelSize(15); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r, Qt::AlignHCenter | Qt::AlignBottom,
               QString("Evidence found: %1.   Credibility left: %2 of 6.    Press R to play again.").arg(game_->dossier().size()).arg(game_->credibility()));
}

void GameWidget::drawGameOver(QPainter& p) {
    p.fillRect(rect(), QColor(20, 6, 8, 245));
    QRect r = rect().adjusted(width() / 6, height() / 4, -width() / 6, -height() / 4);
    QFont f = p.font(); f.setPixelSize(32); f.setBold(true); p.setFont(f);
    p.setPen(QColor(235, 100, 100));
    p.drawText(r, Qt::AlignHCenter | Qt::AlignTop, "CASE REASSIGNED");
    f.setPixelSize(18); f.setBold(false); p.setFont(f);
    p.setPen(QColor(235, 235, 235));
    p.drawText(r.adjusted(0, 70, 0, 0), kWrapHCenterTop,
               "You made too many accusations without proof, and the senior officer has taken you off the case. "
               "The family will sign it off as natural causes.\n\nThink before you present evidence: it must truly contradict what the person said.");
    f.setPixelSize(16); p.setFont(f);
    p.setPen(QColor(200, 190, 150));
    p.drawText(r, Qt::AlignHCenter | Qt::AlignBottom, "Press R to start again");
}

void GameWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(14, 15, 22));
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

    const MapObject* target = nearestInteractable(tiled_, *game_, player_.x, player_.y);
    drawHud(p, target);
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
        case Mode::Explore: if (hasMessage()) drawMessage(p); break;
    }
}
