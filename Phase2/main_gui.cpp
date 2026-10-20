#include <QApplication>
#include <QMessageBox>
#include <exception>
#include "GameWidget.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        GameWidget w;
        w.resize(960, 720);
        w.show();
        return app.exec();
    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "Could not start", QString::fromUtf8(e.what()));
        return 1;
    }
}
