#pragma once

#include <QtOpenGLWidgets/QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMatrix4x4>
#include <QVector>
#include <QVector3D>
#include <QMouseEvent>
#include <QWheelEvent>
#include <deque>

class TouchOpenGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit TouchOpenGLWidget(QWidget* parent = nullptr);
    ~TouchOpenGLWidget();

public slots:
    void updateTransform(const QVector<double>& transformMatrix);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // 完美复刻原版鼠标交互
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // 状态数据
    QVector<double> m_currentTransform;
    QVector3D m_currentPos;
    std::deque<QVector3D> m_trailPoints;

    // 视角交互控制 (复刻原版变量)
    float rotateX;
    float rotateY;
    float camDistance;
    QPoint lastMousePos;
    bool isDragging;

    // 复刻的绘制函数
    void drawFloor();
    void drawCubeBorders();
    void draw3DAxis();
    void drawCursor();
    void drawTrail();

    // 替代 glutSolidSphere 的手写完美球体
    void drawSolidSphere(float radius, int slices, int stacks);
    // 替代 glutBitmapCharacter 的 3D 线段文字
    void drawLabelX(const QVector3D& pos);
    void drawLabelY(const QVector3D& pos);
    void drawLabelZ(const QVector3D& pos);
};

