#pragma once
// The log-in window shown before the game, in the game's dark-and-gold style. It has two modes: Log in, and Create
// account (with a confirm-password field). On success the dialog is accepted and userId() says who is playing.
// Passwords are never stored or printed (see Auth.h).
#include <QDialog>
#include <QString>

class Database;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

class LoginDialog : public QDialog {
public:
    explicit LoginDialog(Database& db, QWidget* parent = nullptr);
    int userId() const { return userId_; }
    QString username() const { return username_; }

private:
    void submit();                      // the main button: log in, or create the account
    void setSignUp(bool on);            // switch between the two modes
    void doLogin();
    void doSignUp();
    void showError(const QString& message);

    Database& db_;
    bool signUp_ = false;
    QLabel* heading_ = nullptr;
    QLabel* confirmLabel_ = nullptr;
    QLabel* error_ = nullptr;
    QLineEdit* user_ = nullptr;
    QLineEdit* pass_ = nullptr;
    QLineEdit* confirm_ = nullptr;
    QCheckBox* show_ = nullptr;
    QPushButton* primary_ = nullptr;
    QPushButton* switch_ = nullptr;
    int userId_ = 0;
    QString username_;
};
