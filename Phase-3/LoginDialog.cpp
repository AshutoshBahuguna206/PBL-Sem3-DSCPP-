#include "LoginDialog.h"
#include <QCheckBox>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <string>
#include "Auth.h"
#include "AuthRules.h"
#include "Database.h"

namespace {
// The look: the game's dark background, warm white text, and gold for the title and the main button.
const char* kStyle = R"QSS(
QDialog#login { background: #12141c; }
QLabel { color: #e8e2d0; }
QLabel#subtitle { color: #b9ad8c; font-size: 16px; font-style: italic; }
QLabel#heading { color: #f2ecdc; font-size: 20px; font-weight: bold; }
QLabel#field { color: #9aa3b5; font-size: 12px; font-weight: bold; }
QLabel#error { color: #ff7b72; font-size: 13px; }
QLabel#note { color: #7d8597; font-size: 12px; }
QFrame#rule { background: #d6aa46; border: none; min-height: 2px; max-height: 2px; }
QLineEdit { background: #1c2030; color: #f2ecdc; border: 1px solid #343a4f; border-radius: 6px;
            padding: 10px 12px; font-size: 16px; selection-background-color: #d6aa46; }
QLineEdit:focus { border: 1px solid #d6aa46; }
QCheckBox { color: #9aa3b5; font-size: 13px; }
QPushButton#primary { background: #d6aa46; color: #12141c; border: none; border-radius: 6px;
                      padding: 12px; font-size: 17px; font-weight: bold; }
QPushButton#primary:hover { background: #e6bd5c; }
QPushButton#primary:pressed { background: #b88f33; }
QPushButton#switch { background: transparent; color: #8fb8ff; border: none; font-size: 14px; padding: 4px; }
QPushButton#switch:hover { color: #b9d3ff; }
)QSS";

QLabel* fieldLabel(const QString& text, QWidget* parent) {
    auto* l = new QLabel(text, parent);
    l->setObjectName("field");
    return l;
}
}  // namespace

LoginDialog::LoginDialog(Database& db, QWidget* parent) : QDialog(parent), db_(db) {
    setObjectName("login");
    setWindowTitle("The Detective's Dossier");
    setModal(true);
    setStyleSheet(kStyle);
    constexpr int kWidth = 600, kMargin = 56;               // wide enough for the title at any display scaling
    setFixedWidth(kWidth);

    const QString titleText = "THE DETECTIVE'S DOSSIER";
    auto* title = new QLabel(titleText, this);
    QFont tf("Georgia");
    tf.setBold(true);
    tf.setLetterSpacing(QFont::AbsoluteSpacing, 2);
    tf.setPixelSize(32);
    const int room = kWidth - 2 * kMargin - 8;
    while (tf.pixelSize() > 16 && QFontMetrics(tf).horizontalAdvance(titleText) > room) tf.setPixelSize(tf.pixelSize() - 1);
    title->setFont(tf);
    title->setWordWrap(true);                                   // never cut off: wraps onto two lines if it must
    title->setStyleSheet("color: #d6aa46;");
    title->setAlignment(Qt::AlignCenter);
    auto* subtitle = new QLabel("Case File No. 1: The Old Way", this);
    subtitle->setObjectName("subtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    auto* rule = new QFrame(this);
    rule->setObjectName("rule");
    rule->setFixedWidth(200);

    heading_ = new QLabel(this);
    heading_->setObjectName("heading");
    user_ = new QLineEdit(this);
    user_->setPlaceholderText("3 to 20 letters, digits or _");
    user_->setMaxLength(static_cast<int>(AuthRules::kMaxUsername) + 10);   // spaces are trimmed later
    pass_ = new QLineEdit(this);
    pass_->setEchoMode(QLineEdit::Password);                               // the typed password is hidden
    pass_->setPlaceholderText("at least 6 characters");
    pass_->setMaxLength(static_cast<int>(AuthRules::kMaxPassword));
    confirmLabel_ = fieldLabel("CONFIRM PASSWORD", this);
    confirm_ = new QLineEdit(this);
    confirm_->setEchoMode(QLineEdit::Password);
    confirm_->setPlaceholderText("type it again");
    confirm_->setMaxLength(static_cast<int>(AuthRules::kMaxPassword));
    show_ = new QCheckBox("Show password", this);
    error_ = new QLabel(this);
    error_->setObjectName("error");
    error_->setWordWrap(true);
    primary_ = new QPushButton(this);
    primary_->setObjectName("primary");
    primary_->setDefault(true);                                            // Enter presses it
    primary_->setCursor(Qt::PointingHandCursor);
    switch_ = new QPushButton(this);
    switch_->setObjectName("switch");
    switch_->setFlat(true);
    switch_->setCursor(Qt::PointingHandCursor);
    auto* note = new QLabel("Your password is stored only as a salted hash, never as text.", this);
    note->setObjectName("note");
    note->setAlignment(Qt::AlignCenter);
    note->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kMargin, 40, kMargin, 32);
    layout->setSpacing(8);
    layout->addWidget(title);
    layout->addWidget(subtitle);
    layout->addSpacing(6);
    layout->addWidget(rule, 0, Qt::AlignHCenter);
    layout->addSpacing(18);
    layout->addWidget(heading_);
    layout->addSpacing(8);
    layout->addWidget(fieldLabel("USERNAME", this));
    layout->addWidget(user_);
    layout->addSpacing(6);
    layout->addWidget(fieldLabel("PASSWORD", this));
    layout->addWidget(pass_);
    layout->addSpacing(6);
    layout->addWidget(confirmLabel_);
    layout->addWidget(confirm_);
    layout->addWidget(show_);
    layout->addWidget(error_);
    layout->addSpacing(6);
    layout->addWidget(primary_);
    layout->addWidget(switch_, 0, Qt::AlignHCenter);
    layout->addSpacing(8);
    layout->addWidget(note);

    connect(primary_, &QPushButton::clicked, this, [this] { submit(); });
    connect(switch_, &QPushButton::clicked, this, [this] { setSignUp(!signUp_); });
    connect(show_, &QCheckBox::toggled, this, [this](bool on) {
        pass_->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
        confirm_->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    setSignUp(false);
    user_->setFocus();
}

void LoginDialog::setSignUp(bool on) {
    signUp_ = on;
    heading_->setText(on ? "Create your detective account" : "Welcome back, detective");
    confirmLabel_->setVisible(on);
    confirm_->setVisible(on);
    primary_->setText(on ? "Create account" : "Log in");
    switch_->setText(on ? "Already have an account? Log in" : "New here? Create an account");
    error_->clear();
    pass_->clear();
    confirm_->clear();
    adjustSize();
}

void LoginDialog::submit() {
    if (signUp_) doSignUp();
    else doLogin();
}

void LoginDialog::showError(const QString& message) {
    error_->setText(message);
    pass_->clear();                                                        // never keep a wrong password on screen
    confirm_->clear();
}

void LoginDialog::doLogin() {
    const std::string name = AuthRules::normalizeUsername(user_->text().toStdString());
    const std::string password = pass_->text().toStdString();
    if (name.empty() || password.empty()) { showError("Enter your username and password."); return; }

    // The same message for an unknown user and a wrong password, so it does not reveal which names exist.
    const auto record = db_.findUser(name);
    if (!record || !Auth::verify(password, record->salt, record->hash)) {
        showError("Wrong username or password.");
        return;
    }
    userId_ = record->id;
    username_ = QString::fromStdString(name);
    accept();
}

void LoginDialog::doSignUp() {
    const std::string name = AuthRules::normalizeUsername(user_->text().toStdString());
    const std::string password = pass_->text().toStdString();
    AuthRules::Problem p = AuthRules::checkUsername(name);
    if (p == AuthRules::Problem::None) p = AuthRules::checkPassword(password);
    if (p != AuthRules::Problem::None) { showError(AuthRules::describe(p)); return; }
    if (password != confirm_->text().toStdString()) { showError("The two passwords do not match."); return; }

    const std::string salt = Auth::makeSalt();
    const auto id = db_.createUser(name, salt, Auth::hashPassword(password, salt));
    if (!id) {
        if (db_.findUser(name)) showError("That username is already taken.");
        else showError("Could not create the account: " + db_.lastError());
        return;
    }
    userId_ = *id;
    username_ = QString::fromStdString(name);
    accept();
}
