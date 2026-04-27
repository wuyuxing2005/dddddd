#pragma once

#include <cstdint>

#pragma pack(push, 1)
struct DobotRealTimeData {
    uint8_t reserved1[432];
    double qActual[6];
    uint8_t reserved2[144];
    double toolVectorActual[6];
    uint8_t reserved3[768];
};
#pragma pack(pop)

static_assert(sizeof(DobotRealTimeData) == 1440, "DobotRealTimeData size must be exactly 1440 bytes");
