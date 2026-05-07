#include "HexUtils.h"

namespace HexUtils {

QString toHexString(const QByteArray &data)
{
    if (data.isEmpty()) return QString();
    QString result;
    result.reserve(data.size() * 3);
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) result += ' ';
        result += QString("%1").arg((quint8)data[i], 2, 16, QChar('0')).toUpper();
    }
    return result;
}

QByteArray fromHexString(const QString &hex, bool *ok)
{
    QString clean = hex.simplified().remove(' ');
    // Pad odd length
    if (clean.length() % 2 != 0) clean = '0' + clean;

    QByteArray result;
    result.reserve(clean.length() / 2);
    for (int i = 0; i < clean.length(); i += 2) {
        bool byteOk = false;
        quint8 byte = clean.mid(i, 2).toUInt(&byteOk, 16);
        if (!byteOk) {
            if (ok) *ok = false;
            return {};
        }
        result.append((char)byte);
    }
    if (ok) *ok = true;
    return result;
}

bool isValidHex(const QString &text)
{
    bool ok = false;
    fromHexString(text, &ok);
    return ok || text.trimmed().isEmpty();
}

} // namespace HexUtils
