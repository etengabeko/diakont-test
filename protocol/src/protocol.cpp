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

QByteArray Message::body() const
{
    return m_body;
}

void Message::setBody(const QByteArray& body)
{
    m_body = body;
}

QByteArray Message::serialize() const
{
    // TODO

    QByteArray result;
    {
        QDataStream output(&result, QIODevice::WriteOnly);
        output << static_cast<quint8>(m_type);
        output << static_cast<quint16>(m_body.size());
        output.writeRawData(m_body.data(), m_body.size());
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
        quint16 bodySize = 0;
        input >> bodySize;
        result.m_body.resize(bodySize);
        input.readRawData(result.m_body.data(), bodySize);
    }

    if (ok != nullptr)
    {
        switch (result.type())
        {
        case Type::Subscribe:
        case Type::Unsubscribe:
        case Type::InfoRequest:
        case Type::InfoResponse:
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
