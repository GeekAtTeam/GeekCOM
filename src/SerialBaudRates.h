#pragma once

#include <QComboBox>
#include <QString>
#include <array>

namespace SerialBaudRates {

// 常用串口波特率（升序）；含 SBUS 100000 与高速 1500000
inline constexpr std::array<int, 13> kStandardRates = {
    1200,   2400,    4800,    9600,   19200,   38400,   57600,
    100000, // SBUS
    115200, 230400, 460800, 921600, 1500000,
};

inline void populateBaudCombo(QComboBox *combo)
{
    if (!combo)
        return;
    for (int b : kStandardRates)
        combo->addItem(QString::number(b), b);
}

} // namespace SerialBaudRates
