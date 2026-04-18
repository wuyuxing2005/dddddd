#include "TouchOpenGLWidget.h"
#include <cmath>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <GL/gl.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==========================================================
// 完美复刻：test8.3.cpp 中的所有硬核配置参数
// ==========================================================
namespace Config3D {
    const double MAX_ABS = 150.0;
    const double DEV_X_MIN = -MAX_ABS, DEV_X_MAX = MAX_ABS;
    const double DEV_Y_MIN = -MAX_ABS, DEV_Y_MAX = MAX_ABS;
    const double DEV_Z_MIN = -MAX_ABS, DEV_Z_MAX = MAX_ABS;
    const double CENTER_X = 0, CENTER_Y = 0, CENTER_Z = 0;

    // 原版颜色配置
    const float COLOR_BG[4] = { 0.05f, 0.07f, 0.10f, 1.0f };       // 深邃背景
    const float COLOR_FLOOR[4] = { 0.22f, 0.25f, 0.30f, 0.55f };   // 地板网格
    const float COLOR_BORDER[4] = { 0.62f, 0.68f, 0.78f, 0.50f };  // 外框
    const float COLOR_AXIS_X[4] = { 1.00f, 0.35f, 0.35f, 0.95f };  // 红-X
    const float COLOR_AXIS_Y[4] = { 0.35f, 0.95f, 0.45f, 0.95f };  // 绿-Y
    const float COLOR_AXIS_Z[4] = { 0.35f, 0.55f, 1.00f, 0.95f };  // 蓝-Z
    const float COLOR_CURSOR_DOT[4] = { 1.00f, 1.00f, 1.00f, 0.95f };

    const float AXIS_LINE_WIDTH = 3.0f;
    const double BASE_CAM_X = 0.0, BASE_CAM_Y = 70.0, BASE_CAM_Z = 240.0;
    const double NEAR_CLIP = 1.0, FAR_CLIP = 800.0, FOV = 45.0;

    // 交互参数
    const float MIN_ZOOM = 0.3f, MAX_ZOOM = 5.0f, ZOOM_STEP = 1.5f;
    const int MAX_TRAIL = 30;
    const float ROTATION_SPEED = 0.5f;
}

// 兼容原版自定义 clamp 函数
template<typename T>
const T& local_clamp(const T& value, const T& min, const T& max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

TouchOpenGLWidget::TouchOpenGLWidget(QWidget* parent)
    : QOpenGLWidget(parent),
    rotateX(15.0f), rotateY(10.0f), camDistance(1.0f), isDragging(false)
{
    m_currentTransform.resize(16);
    m_currentTransform.fill(0.0);
    m_currentTransform[0] = 1.0; m_currentTransform[5] = 1.0;
    m_currentTransform[10] = 1.0; m_currentTransform[15] = 1.0;
    m_currentPos = QVector3D(0.0f, 0.0f, 0.0f);
}

TouchOpenGLWidget::~TouchOpenGLWidget() {}

// ==========================================================
// 数据更新逻辑 (复刻数据截断与轨迹记录)
// ==========================================================
void TouchOpenGLWidget::updateTransform(const QVector<double>& transformMatrix)
{
    if (transformMatrix.size() == 16) {
        m_currentTransform = transformMatrix;

        // 从矩阵的 12, 13, 14 提取 X,Y,Z (由于 Touch 传的是列主序)
        double x = transformMatrix[12];
        double y = transformMatrix[13];
        double z = transformMatrix[14];

        // 复刻原版的坐标截断
        x = local_clamp(x, Config3D::DEV_X_MIN, Config3D::DEV_X_MAX);
        y = local_clamp(y, Config3D::DEV_Y_MIN, Config3D::DEV_Y_MAX);
        z = local_clamp(z, Config3D::DEV_Z_MIN, Config3D::DEV_Z_MAX);

        m_currentPos = QVector3D((float)x, (float)y, (float)z);

        // 复刻原版的轨迹记录
        m_trailPoints.push_back(m_currentPos);
        while (m_trailPoints.size() > Config3D::MAX_TRAIL) {
            m_trailPoints.pop_front();
        }

        update();
    }
}

// ==========================================================
// 生命周期控制 (复刻 glut 初始化)
// ==========================================================
void TouchOpenGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    // 原版背景色
    glClearColor(Config3D::COLOR_BG[0], Config3D::COLOR_BG[1], Config3D::COLOR_BG[2], Config3D::COLOR_BG[3]);

    // 原版 OpenGL 状态
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 原版抗锯齿
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
}

void TouchOpenGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    if (h == 0) h = 1;
    GLfloat ratio = (GLfloat)w / (GLfloat)h;

    QMatrix4x4 projection;
    projection.perspective(Config3D::FOV, ratio, Config3D::NEAR_CLIP, Config3D::FAR_CLIP);
    glLoadMatrixf(projection.constData());
}

void TouchOpenGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);

    // 复刻原版通过调整相机距离实现缩放
    double camX = Config3D::BASE_CAM_X;
    double camY = Config3D::BASE_CAM_Y;
    double camZ = Config3D::BASE_CAM_Z * camDistance;

    QMatrix4x4 view;
    view.lookAt(QVector3D(camX, camY, camZ),
        QVector3D(Config3D::CENTER_X, Config3D::CENTER_Y, Config3D::CENTER_Z),
        QVector3D(0.0f, 1.0f, 0.0f));

    glLoadMatrixf(view.constData());

    // 复刻原版应用鼠标拖拽旋转
    glRotatef((float)rotateX, 1.0f, 0.0f, 0.0f);
    glRotatef((float)rotateY, 0.0f, 1.0f, 0.0f);

    // 绘制原始场景元素
    drawFloor();
    drawCubeBorders();
    draw3DAxis();
    drawTrail();
    drawCursor();
}

// ==========================================================
// 鼠标交互 (完美平移 glutMouseFunc / glutMotionFunc)
// ==========================================================
void TouchOpenGLWidget::mousePressEvent(QMouseEvent* event) {
    // 1. 左键：记录拖拽起点
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        lastMousePos = event->pos();
    }
    // 2. 右键：放大 (距离变近)
    else if (event->button() == Qt::RightButton) {
        camDistance /= Config3D::ZOOM_STEP;
        camDistance = local_clamp(camDistance, Config3D::MIN_ZOOM, Config3D::MAX_ZOOM);
        update(); // 触发重绘
    }
    // 3. 中键：缩小 (距离变远)
    else if (event->button() == Qt::MiddleButton) {
        camDistance *= Config3D::ZOOM_STEP;
        camDistance = local_clamp(camDistance, Config3D::MIN_ZOOM, Config3D::MAX_ZOOM);
        update(); // 触发重绘
    }
}
void TouchOpenGLWidget::mouseMoveEvent(QMouseEvent* event) {
    if (isDragging) {
        rotateY += (event->pos().x() - lastMousePos.x()) * Config3D::ROTATION_SPEED;
        rotateX -= (event->pos().y() - lastMousePos.y()) * Config3D::ROTATION_SPEED;
        rotateX = local_clamp(rotateX, -45.0f, 60.0f); // 原版角度限制
        lastMousePos = event->pos();
        update();
    }
}

void TouchOpenGLWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
    }
}

void TouchOpenGLWidget::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() > 0) {
        camDistance /= Config3D::ZOOM_STEP; // 滚轮向上放大
    }
    else {
        camDistance *= Config3D::ZOOM_STEP; // 滚轮向下缩小
    }
    camDistance = local_clamp(camDistance, Config3D::MIN_ZOOM, Config3D::MAX_ZOOM);
    update();
}

// ==========================================================
// 绘制实现 (1:1 照搬你的渲染逻辑)
// ==========================================================

void TouchOpenGLWidget::drawFloor()
{
    const double GRID_SPACING = 20.0;
    glColor4fv(Config3D::COLOR_FLOOR);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (double z = Config3D::DEV_Z_MIN; z <= Config3D::DEV_Z_MAX; z += GRID_SPACING) {
        glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MIN, z);
        glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MIN, z);
    }
    for (double x = Config3D::DEV_X_MIN; x <= Config3D::DEV_X_MAX; x += GRID_SPACING) {
        glVertex3d(x, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MIN);
        glVertex3d(x, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MAX);
    }
    glEnd();

    glColor3f(0.4f, 0.4f, 0.4f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
    glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MIN);
    glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MIN);
    glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MAX);
    glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MAX);
    glEnd();
}

void TouchOpenGLWidget::drawCubeBorders()
{
    glColor4fv(Config3D::COLOR_BORDER);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MAX);
    glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MAX);
    glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MAX, Config3D::DEV_Z_MAX);
    glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MAX, Config3D::DEV_Z_MAX);
    glEnd();

    glBegin(GL_LINE_LOOP);
    glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MIN);
    glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MIN);
    glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MAX, Config3D::DEV_Z_MIN);
    glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MAX, Config3D::DEV_Z_MIN);
    glEnd();

    glBegin(GL_LINES);
    glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MAX);
    glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MIN);
    glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MAX, Config3D::DEV_Z_MAX);
    glVertex3d(Config3D::DEV_X_MIN, Config3D::DEV_Y_MAX, Config3D::DEV_Z_MIN);
    glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MAX);
    glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MIN, Config3D::DEV_Z_MIN);
    glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MAX, Config3D::DEV_Z_MAX);
    glVertex3d(Config3D::DEV_X_MAX, Config3D::DEV_Y_MAX, Config3D::DEV_Z_MIN);
    glEnd();
}

void TouchOpenGLWidget::draw3DAxis()
{
    const double AXIS_LEN = 100.0;
    const double TICK_SPACING = 20.0;
    const float TICK_LENGTH = 3.0;

    glPushMatrix();
    glTranslatef((float)Config3D::CENTER_X, (float)Config3D::CENTER_Y, (float)Config3D::CENTER_Z);

    // --- 绘制 X 轴 ---
    glColor4fv(Config3D::COLOR_AXIS_X);
    glLineWidth(Config3D::AXIS_LINE_WIDTH);
    glBegin(GL_LINES);
    glVertex3d(-AXIS_LEN, 0.0, 0.0); glVertex3d(AXIS_LEN, 0.0, 0.0);
    glEnd();
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (double pos = -AXIS_LEN; pos <= AXIS_LEN; pos += TICK_SPACING) {
        if (pos == 0.0) continue;
        glVertex3d(pos, -TICK_LENGTH / 2, 0.0); glVertex3d(pos, TICK_LENGTH / 2, 0.0);
    }
    glEnd();
    drawLabelX(QVector3D(AXIS_LEN + 10, 0, 0)); // 替代文字绘制

    // --- 绘制 Y 轴 ---
    glColor4fv(Config3D::COLOR_AXIS_Y);
    glLineWidth(Config3D::AXIS_LINE_WIDTH);
    glBegin(GL_LINES);
    glVertex3d(0.0, 0.0, -AXIS_LEN); glVertex3d(0.0, 0.0, AXIS_LEN);
    glEnd();
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (double pos = -AXIS_LEN; pos <= AXIS_LEN; pos += TICK_SPACING) {
        if (pos == 0.0) continue;
        glVertex3d(-TICK_LENGTH / 2, 0.0, pos); glVertex3d(TICK_LENGTH / 2, 0.0, pos);
    }
    glEnd();
    drawLabelY(QVector3D(0, 0, -AXIS_LEN - 10)); // 替代文字绘制

    // --- 绘制 Z 轴 ---
    glColor4fv(Config3D::COLOR_AXIS_Z);
    glLineWidth(Config3D::AXIS_LINE_WIDTH);
    glBegin(GL_LINES);
    glVertex3d(0.0, -AXIS_LEN, 0.0); glVertex3d(0.0, AXIS_LEN, 0.0);
    glEnd();
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (double pos = -AXIS_LEN; pos <= AXIS_LEN; pos += TICK_SPACING) {
        if (pos == 0.0) continue;
        glVertex3d(-TICK_LENGTH / 2, pos, 0.0); glVertex3d(TICK_LENGTH / 2, pos, 0.0);
    }
    glEnd();
    drawLabelZ(QVector3D(0, AXIS_LEN + 10, 0)); // 替代文字绘制

    glPopMatrix();
}

void TouchOpenGLWidget::drawCursor()
{
    const float DOT_SIZE = 2.0f;
    glPushMatrix();
    glTranslatef(m_currentPos.x() + Config3D::CENTER_X,
        m_currentPos.y() + Config3D::CENTER_Y,
        m_currentPos.z() + Config3D::CENTER_Z);

    glColor4fv(Config3D::COLOR_CURSOR_DOT);
    // 使用纯原生算法替代 glutSolidSphere，保证 100% 相同圆滑视觉
    drawSolidSphere(DOT_SIZE, 16, 16);

    glPopMatrix();
}

void TouchOpenGLWidget::drawTrail()
{
    if (m_trailPoints.size() < 2) return;

    glLineWidth(2.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int pointCount = m_trailPoints.size();
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < pointCount; i++) {
        // 完全复刻原版的尾迹渐变消散算法
        float alpha = 0.8f * (1.0f - (float)i / pointCount * 0.7f);
        glColor4f(0.2f, 0.7f, 1.0f, alpha);

        const QVector3D& p = m_trailPoints[i];
        glVertex3d(p.x() + Config3D::CENTER_X, p.y() + Config3D::CENTER_Y, p.z() + Config3D::CENTER_Z);
    }
    glEnd();
    glDisable(GL_BLEND);
}

// ==========================================================
// 脱离 GLUT 依赖的原生 OpenGL 图形生成
// ==========================================================
void TouchOpenGLWidget::drawSolidSphere(float radius, int slices, int stacks) {
    for (int i = 0; i < stacks; ++i) {
        float lat0 = M_PI * (-0.5f + (float)(i) / stacks);
        float z0 = radius * sin(lat0);
        float zr0 = radius * cos(lat0);
        float lat1 = M_PI * (-0.5f + (float)(i + 1) / stacks);
        float z1 = radius * sin(lat1);
        float zr1 = radius * cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; ++j) {
            float lng = 2 * M_PI * (float)(j - 1) / slices;
            float x = cos(lng);
            float y = sin(lng);
            glNormal3f(x * zr1, y * zr1, z1);
            glVertex3f(x * zr1, y * zr1, z1);
            glNormal3f(x * zr0, y * zr0, z0);
            glVertex3f(x * zr0, y * zr0, z0);
        }
        glEnd();
    }
}

// 手动画 X、Y、Z 字母（因为去掉了 glutBitmapCharacter）
void TouchOpenGLWidget::drawLabelX(const QVector3D& pos) {
    glPushMatrix(); glTranslatef(pos.x(), pos.y(), pos.z());
    glBegin(GL_LINES); glVertex3f(-2, -2, 0); glVertex3f(2, 2, 0); glVertex3f(-2, 2, 0); glVertex3f(2, -2, 0); glEnd();
    glPopMatrix();
}
void TouchOpenGLWidget::drawLabelY(const QVector3D& pos) {
    glPushMatrix(); glTranslatef(pos.x(), pos.y(), pos.z());
    glBegin(GL_LINES); glVertex3f(0, 0, 0); glVertex3f(0, -3, 0); glVertex3f(0, 0, 0); glVertex3f(-2, 3, 0); glVertex3f(0, 0, 0); glVertex3f(2, 3, 0); glEnd();
    glPopMatrix();
}
void TouchOpenGLWidget::drawLabelZ(const QVector3D& pos) {
    glPushMatrix(); glTranslatef(pos.x(), pos.y(), pos.z());
    glBegin(GL_LINE_STRIP); glVertex3f(-2, 3, 0); glVertex3f(2, 3, 0); glVertex3f(-2, -3, 0); glVertex3f(2, -3, 0); glEnd();
    glPopMatrix();
}