#include "protocol.h"

#include <QDataStream>

namespace Netcom
{

Message::Message(Type type) :
    m_type(type)
{

}

Message::Type Message::type() const
{
    return m_type;
}

void Message::setType(Type type)
{
    m_type = type;
}

QByteArray Message::serialize() const
{
    // TODO

    QByteArray result;
    {
        QDataStream output(&result, QIODevice::WriteOnly);
        output << static_cast<quint8>(m_type);
    }

    return result;
}

Message Message::parse(const QByteArray& raw, bool *ok)
{
    // TODO

    if (ok != nullptr)
    {
        *ok = false;
    }

    Message result;
    {
        QDataStream input(raw);
        quint8 type = 0;
        input >> type;
        result.setType(static_cast<Type>(type));
    }

    if (ok != nullptr)
    {
        switch (result.type())
        {
        case Type::Subscribe:
        case Type::Unsubscribe:
        case Type::InfoRequest:
        case Type::InfoResponse:
        case Type::Ping:
            *ok = true;
            break;
        default:
            break;
        }
    }

    return result;
}

QDataStream& operator<< (QDataStream& to, const Message& from)
{
    QByteArray raw = from.serialize();
    quint32 size = raw.size();
    to << size;
    to.writeRawData(raw.data(), size);

    return to;
}

QDataStream& operator>> (QDataStream& from, Message& to)
{
    quint32 size = 0;
    from >> size;
    QByteArray raw(size, '\0');
    from.readRawData(raw.data(), size);
    to = Message::parse(raw);

    return from;
}

} // Netcom
