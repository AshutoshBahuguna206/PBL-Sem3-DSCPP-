#include <QApplication>
#include <QMessageBox>
#include <exception>
#include <stdexcept>
#include "GameWidget.h"
#ifdef HAVE_QT_SQL
#include "Database.h"
#include "LoginDialog.h"
#endif

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
#ifdef HAVE_QT_SQL
        // Open the database, then ask the player to log in or sign up. Closing the window ends the program.
        Database db;
        if (!db.open(Database::defaultPath())) throw std::runtime_error("Cannot open the database: " + db.lastError().toStdString());
        LoginDialog login(db);
        if (login.exec() != QDialog::Accepted) return 0;
#endif
        GameWidget w;
#ifdef HAVE_QT_SQL
        w.setUser(&db, login.userId(), login.username());
#endif
        w.resize(960, 720);
        w.show();
        return app.exec();
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "Could not start", QString::fromUtf8(e.what()));
        return 1;
    }
}
