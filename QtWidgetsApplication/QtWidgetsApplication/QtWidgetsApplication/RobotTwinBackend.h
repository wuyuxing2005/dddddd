#pragma once

#include <QObject>
#include <cstdint> // 【新增】用于支持 uint64_t 等定长数据类型

// ==========================================================
// 【新增】30004 端口的 1440 字节实时反馈数据解析结构体
// ==========================================================
#pragma pack(push, 1) // 强制编译器按 1 字节对齐，完美贴合网络字节流
struct DobotRealTimeData {
    uint16_t len;                     // 0000-0001: 消息长度
    uint16_t reserved1[3];            // 0002-0007: 保留位
    uint64_t digitalInputs;           // 0008-0015: DI
    uint64_t digitalOutputs;          // 0016-0023: DO
    uint64_t robotMode;               // 0024-0031: 机器人模式
    uint64_t timeStamp;               // 0032-0039: 时间戳

    uint8_t reserved2[392];           // 0040-0431: 略过不重要的数据

    // 【核心数据】实际关节角度 (J1-J6)
    double qActual[6];                // 0432-0479: 实际关节位置

    uint8_t reserved3[144];           // 0480-0623: 略过实际速度等

    double toolVectorActual[6];       // 0624-0671: TCP 实际坐标值
    uint8_t reserved4[359];           // 0672-1030: 略过状态位等
    char robotType;                   // 1031: 机型 (3 代表 CR3)
    uint8_t reserved5[408];           // 1032-1439: 略过剩下的数据
};
#pragma pack(pop) // 恢复默认对齐方式
// ==========================================================


class RobotTwinBackend : public QObject {
    Q_OBJECT
        // 【修改】全面升级为 double，完美接收 Dobot 的 64 位双精度浮点数
        Q_PROPERTY(double j1 READ j1 WRITE setJ1 NOTIFY j1Changed)
        Q_PROPERTY(double j2 READ j2 WRITE setJ2 NOTIFY j2Changed)
        Q_PROPERTY(double j3 READ j3 WRITE setJ3 NOTIFY j3Changed)
        Q_PROPERTY(double j4 READ j4 WRITE setJ4 NOTIFY j4Changed)
        Q_PROPERTY(double j5 READ j5 WRITE setJ5 NOTIFY j5Changed)
        Q_PROPERTY(double j6 READ j6 WRITE setJ6 NOTIFY j6Changed)

public:
    explicit RobotTwinBackend(QObject* parent = nullptr) : QObject(parent) {}

    // 【修改】参数类型改为 double
    void updateJoints(double a1, double a2, double a3, double a4, double a5, double a6) {
        setJ1(a1); setJ2(a2); setJ3(a3);
        setJ4(a4); setJ5(a5); setJ6(a6);
    }

    // Getters
    double j1() const { return m_j1; }
    double j2() const { return m_j2; }
    double j3() const { return m_j3; }
    double j4() const { return m_j4; }
    double j5() const { return m_j5; }
    double j6() const { return m_j6; }

    // Setters
    void setJ1(double v) { if (qFuzzyCompare(m_j1, v)) return; m_j1 = v; emit j1Changed(); }
    void setJ2(double v) { if (qFuzzyCompare(m_j2, v)) return; m_j2 = v; emit j2Changed(); }
    void setJ3(double v) { if (qFuzzyCompare(m_j3, v)) return; m_j3 = v; emit j3Changed(); }
    void setJ4(double v) { if (qFuzzyCompare(m_j4, v)) return; m_j4 = v; emit j4Changed(); }
    void setJ5(double v) { if (qFuzzyCompare(m_j5, v)) return; m_j5 = v; emit j5Changed(); }
    void setJ6(double v) { if (qFuzzyCompare(m_j6, v)) return; m_j6 = v; emit j6Changed(); }

signals:
    void j1Changed(); void j2Changed(); void j3Changed();
    void j4Changed(); void j5Changed(); void j6Changed();

private:
    // 【修改】内部缓存变量升级为 double
    double m_j1 = 0, m_j2 = 0, m_j3 = 0, m_j4 = 0, m_j5 = 0, m_j6 = 0;
};