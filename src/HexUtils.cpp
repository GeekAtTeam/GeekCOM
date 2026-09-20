#include "HexUtils.h"
#include <QRegularExpression>

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

QByteArray fromHexString(const QString &hex, bool *ok, QString *error)
{
    if (ok) *ok = false;
    if (error) error->clear();
    const QString clean = hex.trimmed();
    static const QRegularExpression valid(
        QStringLiteral("^(?:[0-9A-Fa-f]{2})+$|^[0-9A-Fa-f]{2}(?:\\s+[0-9A-Fa-f]{2})+$"));
    if (!clean.isEmpty() && !valid.match(clean).hasMatch()) {
        if (error) *error = QStringLiteral("HEX 必须为完整字节对，例如 01 23 或 0123；不能包含单个半字节或非十六进制字符。");
        return {};
    }
    if (ok) *ok = true;
    return QByteArray::fromHex(clean.toLatin1());
}

bool isValidHex(const QString &text)
{
    bool ok = false;
    fromHexString(text, &ok);
    return ok || text.trimmed().isEmpty();
}

} // namespace HexUtils
