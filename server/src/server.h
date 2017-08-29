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

class Server
{
public:
    Server(const QHostAddress& address, int port);
    virtual ~Server() = 0;

    static std::unique_ptr<Server> createServer(Protocol protocol,
                                                const QHostAddress& address,
                                                int port);
    static std::unique_ptr<Server> createServer(const QString& protocol,
                                                const QHostAddress& address,
                                                int port);

    bool start();

    void stop();

    QString errorString() const;

protected:
    virtual bool run() = 0;
    virtual void finish() = 0;

    void incomingMessage(const Message& message,
                         const QHostAddress& senderAddress,
                         quint16 senderPort);

    void addConnection(QAbstractSocket* socket);
    void removeConnection(QAbstractSocket* socket);

protected:
    QString m_lastError;

    QHostAddress m_address;
    int m_port = 0;

private:
    QHash<QAbstractSocket*, QDateTime> m_activeConnections;

};

class TcpServer : public QObject, public Server
{
    Q_OBJECT

public:
    TcpServer(const QHostAddress& address, int port, QObject* parent = nullptr);
    ~TcpServer();

private:
    virtual bool run() override;
    virtual void finish() override;

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
    UdpServer(const QHostAddress& address, int port, QObject* parent = nullptr);
    ~UdpServer();

private:
    virtual bool run() override;
    virtual void finish() override;

private slots:
    void slotOnError();
    void slotReadDatagram();

private:
    QUdpSocket* m_incoming;
    QHash<QUdpSocket*, QByteArray> m_clients;

};

} // Netcom

#endif // NETCOM_SERVER_H
