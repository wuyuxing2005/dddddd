#include "test1.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    test1 window;
    window.show();
    return app.exec();
}
