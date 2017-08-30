#ifndef NETCOM_PROTOCOL_H
#define NETCOM_PROTOCOL_H

#include <QDateTime>
#include <QList>
#include <QString>

class QByteArray;
class QDataStream;

namespace Netcom
{
struct ClientInfo
{
    QString address;
    quint16 port = 0;
    QDateTime datetime;

    ClientInfo() = default;
    ClientInfo(const QString& a, quint16 p, const QDateTime& d);

    bool operator== (const ClientInfo& rhs) const;
    bool operator!= (const ClientInfo& rhs) const;
};

class Message
{
public:
    enum class Type
    {
        Unknown = 0,
        Subscribe,
        Unsubscribe,
        InfoRequest,
        InfoResponse
    };

public:
    Message() = default;
    explicit Message(Type type);

    Type type() const;
    void setType(Type type);

    quint16 backwardPort() const;
    void setBackwardPort(quint16 port);

    const QList<ClientInfo>& clientsInfo() const;
    void setClientsInfo(const QList<ClientInfo>& info);
    void resetClientsInfo();
    void addClientInfo(const ClientInfo& info);

    QByteArray serialize() const;
    static Message parse(const QByteArray& raw, bool* ok = nullptr);

    static QString typeToString(Type type);
    static Type typeFromString(const QString& type);

private:
    Type m_type = Type::Unknown;
    quint16 m_backwardPort = 0;

    QList<ClientInfo> m_info;

};

QDataStream& operator<< (QDataStream& to, const Message& from);
QDataStream& operator>> (QDataStream& from, Message& to);

} // Netcom

#endif // NETCOM_PROTOCOL_H
