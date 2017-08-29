#ifndef NETCOM_SERVER_H
#define NETCOM_SERVER_H

#include <memory>

#include <QDateTime>
#include <QHostAddress>
#include <QObject>
#include <QHash>
#include <QString>

class QAbstractSocket;
class QTcpServer;
class QTcpSocket;
class QUdpSocket;

namespace Netcom
{
class Message;

enum class Protocol
{
    Unknown,
    Tcp,
    Udp
};

Protocol protocolFromString(const QString& str);

struct NetworkAddress
{
    QHostAddress address;
    quint16 port = 0;

    NetworkAddress() = default;
    NetworkAddress(const QHostAddress& a, quint16 p);

    bool operator== (const NetworkAddress& rhs) const;
    bool operator!= (const NetworkAddress& rhs) const;
};

class Server
{
public:
    explicit Server(const NetworkAddress& address);
    virtual ~Server() = 0;

    static std::unique_ptr<Server> createServer(Protocol protocol,
                                                const NetworkAddress& address);
    static std::unique_ptr<Server> createServer(const QString& protocol,
                                                const NetworkAddress& address);

    bool start();

    void stop();

    QString errorString() const;

protected:
    virtual bool run() = 0;
    virtual void finish() = 0;

    void incomingMessage(const Message& message,
                         const NetworkAddress& sender);

    void addConnection(QAbstractSocket* socket);
    void removeConnection(QAbstractSocket* socket);

protected:
    QString m_lastError;

    NetworkAddress m_address;

private:
    QHash<QAbstractSocket*, QDateTime> m_activeConnections;

};

class TcpServer : public QObject, public Server
{
    Q_OBJECT

public:
    explicit TcpServer(const NetworkAddress& address, QObject* parent = nullptr);
    ~TcpServer();

private:
    virtual bool run() override;
    virtual void finish() override;

    void tryProcessIncomingMessage(QTcpSocket* sender);

private slots:
    void slotOnNewConnect();
    void slotOnDisconnect();
    void slotOnError();
    void slotRead();

private:
    QTcpServer* m_srv;
    QHash<QTcpSocket*, QByteArray> m_clients;

};

class UdpServer : public QObject, public Server
{
    Q_OBJECT

public:
    explicit UdpServer(const NetworkAddress& address, QObject* parent = nullptr);
    ~UdpServer();

private:
    virtual bool run() override;
    virtual void finish() override;

private slots:
    void slotOnError();
    void slotReadDatagram();

    void addSubscriber(const NetworkAddress& peer);
    void removeSubscriber(const NetworkAddress& peer);

    void tryProcessIncomingMessage(const NetworkAddress& peer, QByteArray& rawBytes);

private:
    QUdpSocket* m_incoming;
    QHash<NetworkAddress, QByteArray> m_receivedBytes;

    QHash<QUdpSocket*, QByteArray> m_clients;

};

inline uint qHash(const NetworkAddress& key, uint seed = 0)
{
    return (::qHash(key.address.toString(), seed) ^ ::qHash(key.port, seed));
}

} // Netcom

#endif // NETCOM_SERVER_H
