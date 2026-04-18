#include "QtWidgetsApplication.h"
#include <QtWidgets/QApplication>
#include <QSurfaceFormat>
#include <QQuickWindow>

int main(int argc, char* argv[])
{
    // ---------------------------------------------------------
    // 抢救步骤 1：强制 QQuickWidget 放弃 Direct3D，统统使用 OpenGL
    // ---------------------------------------------------------
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    // ---------------------------------------------------------
    // 抢救步骤 2：开启全局上下文共享（让两边和平共处）
    // ---------------------------------------------------------
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // ---------------------------------------------------------
    // 抢救步骤 3：修复深度缓冲（把丢失的 3D 画面找回来）
    // ---------------------------------------------------------
    QSurfaceFormat format;
    format.setDepthBufferSize(24);    // 必须给够深度缓冲
    format.setStencilBufferSize(8);
    format.setProfile(QSurfaceFormat::CompatibilityProfile); // 兼容你手写的 glBegin
    QSurfaceFormat::setDefaultFormat(format);

    // =========================================================

    QApplication app(argc, argv);
    QtWidgetsApplication window;
    window.show();
    return app.exec();
}