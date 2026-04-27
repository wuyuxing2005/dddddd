#include "RobotTwinBackend.h"

#include <QtGlobal>

RobotTwinBackend::RobotTwinBackend(QObject* parent)
    : QObject(parent) {
}

void RobotTwinBackend::updateJoints(double a1, double a2, double a3, double a4, double a5, double a6) {
    setJ1(a1);
    setJ2(a2);
    setJ3(a3);
    setJ4(a4);
    setJ5(a5);
    setJ6(a6);
}

double RobotTwinBackend::j1() const {
    return m_j1;
}

double RobotTwinBackend::j2() const {
    return m_j2;
}

double RobotTwinBackend::j3() const {
    return m_j3;
}

double RobotTwinBackend::j4() const {
    return m_j4;
}

double RobotTwinBackend::j5() const {
    return m_j5;
}

double RobotTwinBackend::j6() const {
    return m_j6;
}

void RobotTwinBackend::setJ1(double v) {
    if (qFuzzyCompare(m_j1, v)) return;
    m_j1 = v;
    emit j1Changed();
}

void RobotTwinBackend::setJ2(double v) {
    if (qFuzzyCompare(m_j2, v)) return;
    m_j2 = v;
    emit j2Changed();
}

void RobotTwinBackend::setJ3(double v) {
    if (qFuzzyCompare(m_j3, v)) return;
    m_j3 = v;
    emit j3Changed();
}

void RobotTwinBackend::setJ4(double v) {
    if (qFuzzyCompare(m_j4, v)) return;
    m_j4 = v;
    emit j4Changed();
}

void RobotTwinBackend::setJ5(double v) {
    if (qFuzzyCompare(m_j5, v)) return;
    m_j5 = v;
    emit j5Changed();
}

void RobotTwinBackend::setJ6(double v) {
    if (qFuzzyCompare(m_j6, v)) return;
    m_j6 = v;
    emit j6Changed();
}
