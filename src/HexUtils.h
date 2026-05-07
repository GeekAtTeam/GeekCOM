#pragma once
#include <QString>
#include <QByteArray>

namespace HexUtils {

// Convert raw bytes to formatted hex string: "01 52 00 18 ..."
QString toHexString(const QByteArray &data);

// Parse hex string (with or without spaces) to bytes; returns empty on error
QByteArray fromHexString(const QString &hex, bool *ok = nullptr);

// Check if string is valid hex (spaces allowed)
bool isValidHex(const QString &text);

} // namespace HexUtils
