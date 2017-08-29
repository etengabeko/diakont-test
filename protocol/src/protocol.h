#ifndef NETCOM_PROTOCOL_H
#define NETCOM_PROTOCOL_H

#include <QByteArray>

class QDataStream;

namespace Netcom
{

class Message
{
public:
    enum class Type
    {
        Unknown = 0,
        Subscribe,
        Unsubscribe,
        Ping,
        InfoRequest,
        InfoResponse
    };

public:
    Message() = default;
    explicit Message(Type type);

    Type type() const;
    void setType(Type type);

    QByteArray body() const;
    void setBody(const QByteArray& body);

    QByteArray serialize() const;
    static Message parse(const QByteArray& raw, bool* ok = nullptr);

private:
    Type m_type = Type::Unknown;
    QByteArray m_body;

};

QDataStream& operator<< (QDataStream& to, const Message& from);
QDataStream& operator>> (QDataStream& from, Message& to);

} // Netcom

#endif // NETCOM_PROTOCOL_H
