#include "server.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

#include <protocol.h>

namespace Netcom
{

Protocol protocolFromString(const QString& str)
{
    static const QHash<QString, Protocol> protocols({ { "tcp", Protocol::Tcp },
                                                      { "udp", Protocol::Udp } });
    if (protocols.contains(str.toLower()))
    {
        return protocols[str.toLower()];
    }
    return Protocol::Unknown;
}

std::unique_ptr<Server> Server::createServer(const QString& protocol,
                                             const QHostAddress& address,
                                             int port)
{
    return createServer(protocolFromString(protocol), address, port);
}

std::unique_ptr<Server> Server::createServer(Protocol protocol,
                                             const QHostAddress& address,
                                             int port)
{
    std::unique_ptr<Server> result;

    switch (protocol)
    {
    case Protocol::Tcp:
        result.reset(new TcpServer(address, port));
        break;
    case Protocol::Udp:
        result.reset(new UdpServer(address, port));
        break;
    default:
        break;
    }

    return result;
}

Server::Server(const QHostAddress& address, int port) :
    m_address(address),
    m_port(port)
{

}

Server::~Server()
{
    QHash<QAbstractSocket*, QDateTime>::iterator it = m_activeConnections.begin();
    while (it != m_activeConnections.end())
    {
        QAbstractSocket* each = it.key();
        each->close();
        each->deleteLater();
        it = m_activeConnections.erase(it);
    }
}

bool Server::start()
{
    return run();
}

void Server::stop()
{
    finish();
}

void Server::addConnection(QAbstractSocket* socket)
{
    Q_CHECK_PTR(socket);

    if (!m_activeConnections.contains(socket))
    {
        m_activeConnections.insert(socket, QDateTime::currentDateTime());
        qDebug() << qApp->tr("Added connection %1:%2 %3")
                    .arg(socket->peerAddress().toString())
                    .arg(socket->peerPort())
                    .arg(m_activeConnections[socket].toString("hh:mm:ss.zzz"));
    }
}

void Server::removeConnection(QAbstractSocket* socket)
{
    Q_CHECK_PTR(socket);

    if (m_activeConnections.contains(socket))
    {
        m_activeConnections.remove(socket);
        qDebug() << qApp->tr("Removed connection %1:%2 %3")
                    .arg(socket->peerAddress().toString())
                    .arg(socket->peerPort())
                    .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"));
    }
}

QString Server::errorString() const
{
    return m_lastError;
}

void Server::incomingMessage(const Message& message,
                             const QHostAddress& senderAddress,
                             quint16 senderPort)
{
    // TODO
    qDebug().noquote() << qApp->tr("IN [%1:%2]:\ntype=%4")
                          .arg(senderAddress.toString())
                          .arg(senderPort)
                          .arg(static_cast<quint8>(message.type()));
}

TcpServer::TcpServer(const QHostAddress& address, int port, QObject* parent) :
    QObject(parent),
    Server(address, port),
    m_srv(new QTcpServer(this))
{
    connect(m_srv, &QTcpServer::newConnection,
            this, &TcpServer::slotOnNewConnect);
}

TcpServer::~TcpServer()
{
    stop();
}

bool TcpServer::run()
{
    QHostAddress listeningAddress = (m_address == QHostAddress::LocalHost ? m_address
                                                                          : (m_address.protocol() == QTcpSocket::IPv6Protocol ? QHostAddress::AnyIPv6
                                                                                                                              : QHostAddress::AnyIPv4));
    bool ok = m_srv->listen(listeningAddress, m_port);
    m_lastError = ok ? QString::null
                     : m_srv->errorString();
    return ok;
}

void TcpServer::finish()
{
    m_srv->close();

    QHash<QTcpSocket*, QByteArray>::iterator it = m_clients.begin();
    while (it != m_clients.end())
    {
        QTcpSocket* each = it.key();
        it = m_clients.erase(it);
        each->disconnectFromHost();
    }
}

void TcpServer::slotOnNewConnect()
{
    QTcpSocket* socket = m_srv->nextPendingConnection();
    connect(socket, static_cast<void(QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::error),
            this, &TcpServer::slotOnError);
    connect(socket, &QTcpSocket::disconnected,
            this, &TcpServer::slotOnDisconnect);

    if (   m_address != QHostAddress::LocalHost
        && m_address != QHostAddress::Any)
    {
        if (socket->peerAddress() != m_address)
        {
            socket->disconnectFromHost();
            return;
        }
    }

    connect(socket, &QTcpSocket::readyRead,
            this, &TcpServer::slotRead);

    m_clients.insert(socket, QByteArray());
    addConnection(socket);
}

void TcpServer::slotOnDisconnect()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket != nullptr)
    {
        removeConnection(socket);
        QHash<QTcpSocket*, QByteArray>::iterator founded = m_clients.find(socket);
        if (founded != m_clients.end())
        {
            m_clients.erase(founded);
        }
    }
    sender()->deleteLater();
}

void TcpServer::slotOnError()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket != nullptr)
    {
        if (socket->error() != QTcpSocket::RemoteHostClosedError)
        {

            m_lastError = socket->errorString();
            qWarning().noquote() << tr("Error [%1:%2]: %3")
                                    .arg(socket->peerAddress().toString())
                                    .arg(socket->peerPort())
                                    .arg(m_lastError);
        }
    }
}

void TcpServer::slotRead()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket != nullptr)
    {
        bool ok = false;
        Message message = Message::parse(socket->readAll(), &ok);

        if (ok)
        {
            switch (message.type())
            {
            case Message::Type::Unknown:
            case Message::Type::Subscribe:
            case Message::Type::Unsubscribe:
                qt_noop();
                break;
            default:
                incomingMessage(message,
                                socket->peerAddress(),
                                socket->peerPort());
                break;
            }
        }
    }
}

UdpServer::UdpServer(const QHostAddress &address, int port, QObject* parent) :
    QObject(parent),
    Server(address, port),
    m_incoming(new QUdpSocket(this))
{
    connect(m_incoming, &QUdpSocket::readyRead,
            this, &UdpServer::slotReadDatagram);
    connect(m_incoming, static_cast<void(QUdpSocket::*)(QAbstractSocket::SocketError)>(&QUdpSocket::error),
            this, &UdpServer::slotOnError);
}

UdpServer::~UdpServer()
{
    stop();
}

bool UdpServer::run()
{
    QHostAddress bindingAddress = (m_address == QHostAddress::LocalHost ? m_address
                                                                        : (m_address.protocol() == QUdpSocket::IPv6Protocol ? QHostAddress::AnyIPv6
                                                                                                                            : QHostAddress::AnyIPv4));
    bool ok = m_incoming->bind(bindingAddress, m_port);
    m_lastError = ok ? QString::null
                     : m_incoming->errorString();
    return ok;
}

void UdpServer::finish()
{
    m_incoming->close();

    QHash<QUdpSocket*, QByteArray>::iterator it = m_clients.begin();
    while (it != m_clients.end())
    {
        QUdpSocket* each = it.key();
        removeConnection(each);
        each->close();
        each->deleteLater();
        it = m_clients.erase(it);
    }
}

void UdpServer::slotReadDatagram()
{
    QHostAddress peerAddress;
    quint16 peerPort = 0;
    QByteArray datagram;

    while (m_incoming->hasPendingDatagrams())
    {
        datagram.resize(m_incoming->pendingDatagramSize());
        m_incoming->readDatagram(datagram.data(), datagram.size(), &peerAddress, &peerPort);
        if (   m_address != QHostAddress::LocalHost
            && m_address != QHostAddress::Any)
        {
            if (peerAddress != m_address)
            {
                continue;
            }
        }

        bool ok = false;
        Message message = Message::parse(datagram, &ok);
        if (ok)
        {
            switch (message.type())
            {
            case Message::Type::Subscribe:
                {
                    QUdpSocket* socket = new QUdpSocket(this);
                    m_clients.insert(socket, QByteArray());
                    connect(socket, &QUdpSocket::readyRead,
                            this, &UdpServer::slotReadDatagram);
                    connect(socket, static_cast<void(QUdpSocket::*)(QAbstractSocket::SocketError)>(&QUdpSocket::error),
                            this, &UdpServer::slotOnError);

                    socket->connectToHost(peerAddress, peerPort);
                    addConnection(socket);
                }
                break;
            case Message::Type::Unsubscribe:
                {
                    QHash<QUdpSocket*, QByteArray>::iterator it = m_clients.begin();
                    while (it != m_clients.end())
                    {
                        QUdpSocket* socket = it.key();
                        if (   socket->peerAddress() == peerAddress
                            && socket->peerPort() == peerPort)
                        {
                            removeConnection(socket);
                            socket->close();
                            socket->deleteLater();
                            m_clients.erase(it);
                            break;
                        }
                    }
                }
                break;
            default:
                incomingMessage(message, peerAddress, peerPort);
                break;
            }
        }
    }
}

void UdpServer::slotOnError()
{
    QUdpSocket* socket = qobject_cast<QUdpSocket*>(sender());
    if (socket != nullptr)
    {
        // TODO
        m_lastError = socket->errorString();
        qWarning().noquote() << tr("Error [%1:%2]: %3")
                                .arg(socket->peerAddress().toString())
                                .arg(socket->peerPort())
                                .arg(m_lastError);
    }
}

} // Netcom
