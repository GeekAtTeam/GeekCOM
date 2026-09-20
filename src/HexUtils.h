#pragma once
#include <QString>
#include <QByteArray>

namespace HexUtils {

// Convert raw bytes to formatted hex string: "01 52 00 18 ..."
QString toHexString(const QByteArray &data);

// Accept an even-length hex string or whitespace-separated byte pairs; never pad.
// Returns empty on error and optionally supplies a validation message.
QByteArray fromHexString(const QString &hex, bool *ok = nullptr, QString *error = nullptr);

// Check if string is valid hex (spaces allowed)
bool isValidHex(const QString &text);

} // namespace HexUtils
