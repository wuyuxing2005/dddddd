#pragma once

#include <QObject>

class RobotTwinBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(double j1 READ j1 WRITE setJ1 NOTIFY j1Changed)
    Q_PROPERTY(double j2 READ j2 WRITE setJ2 NOTIFY j2Changed)
    Q_PROPERTY(double j3 READ j3 WRITE setJ3 NOTIFY j3Changed)
    Q_PROPERTY(double j4 READ j4 WRITE setJ4 NOTIFY j4Changed)
    Q_PROPERTY(double j5 READ j5 WRITE setJ5 NOTIFY j5Changed)
    Q_PROPERTY(double j6 READ j6 WRITE setJ6 NOTIFY j6Changed)

public:
    explicit RobotTwinBackend(QObject* parent = nullptr);

    void updateJoints(double a1, double a2, double a3, double a4, double a5, double a6);

    double j1() const;
    double j2() const;
    double j3() const;
    double j4() const;
    double j5() const;
    double j6() const;

    void setJ1(double v);
    void setJ2(double v);
    void setJ3(double v);
    void setJ4(double v);
    void setJ5(double v);
    void setJ6(double v);

signals:
    void j1Changed();
    void j2Changed();
    void j3Changed();
    void j4Changed();
    void j5Changed();
    void j6Changed();

private:
    double m_j1 = 0.0;
    double m_j2 = 0.0;
    double m_j3 = 0.0;
    double m_j4 = 0.0;
    double m_j5 = 0.0;
    double m_j6 = 0.0;
};
