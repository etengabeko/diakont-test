#include "server.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

#include <protocol.h>

namespace Netcom
{

NetworkAddress::NetworkAddress(const QHostAddress& a, quint16 p) :
    address(a),
    port(p)
{

}

bool NetworkAddress::operator== (const NetworkAddress& rhs) const
{
    if (this == &rhs)
    {
        return true;
    }

    return (   this->address == rhs.address
            && this->port == rhs.port);
}

bool NetworkAddress::operator!= (const NetworkAddress& rhs) const
{
    return !(*this == rhs);
}

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
                                             const NetworkAddress& address)
{
    return createServer(protocolFromString(protocol), address);
}

std::unique_ptr<Server> Server::createServer(Protocol protocol,
                                             const NetworkAddress& address)
{
    std::unique_ptr<Server> result;

    switch (protocol)
    {
    case Protocol::Tcp:
        result.reset(new TcpServer(address));
        break;
    case Protocol::Udp:
        result.reset(new UdpServer(address));
        break;
    default:
        break;
    }

    return result;
}

Server::Server(const NetworkAddress& address) :
    m_address(address)
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
                             const NetworkAddress& sender)
{
    // TODO
    qDebug().noquote() << qApp->tr("IN [%1:%2]:\ntype=%4")
                          .arg(sender.address.toString())
                          .arg(sender.port)
                          .arg(static_cast<quint8>(message.type()));
}

TcpServer::TcpServer(const NetworkAddress& address, QObject* parent) :
    QObject(parent),
    Server(address),
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
    QHostAddress listeningAddress = (m_address.address == QHostAddress::LocalHost ? m_address.address
                                                                                  : (m_address.address.protocol() == QTcpSocket::IPv6Protocol ? QHostAddress::AnyIPv6
                                                                                                                                              : QHostAddress::AnyIPv4));
    bool ok = m_srv->listen(listeningAddress, m_address.port);
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

    if (   m_address.address != QHostAddress::LocalHost
        && m_address.address != QHostAddress::Any)
    {
        if (socket->peerAddress() != m_address.address)
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
    if (   socket != nullptr
        && m_clients.contains(socket))
    {
        m_clients[socket].append(socket->readAll());
        tryProcessIncomingMessage(socket);
    }
}

void TcpServer::tryProcessIncomingMessage(QTcpSocket* sender)
{
    Q_CHECK_PTR(sender);

    if (m_clients.contains(sender))
    {
        QByteArray& receivedBytes = m_clients[sender];
        if (receivedBytes.isEmpty())
        {
            return;
        }

        quint32 expectedSize = 0;
        {
            QDataStream input(&receivedBytes, QIODevice::ReadOnly);
            input >> expectedSize;
        }

        if (   expectedSize > 0
            && static_cast<quint32>(receivedBytes.size() - sizeof(expectedSize)) >= expectedSize)
        {
            Message message;
            {
                QDataStream input(receivedBytes);
                input >> message;
            }
            receivedBytes = receivedBytes.right(receivedBytes.size() - sizeof(expectedSize) - expectedSize);

            switch (message.type())
            {
            case Message::Type::Unknown:
            case Message::Type::Subscribe:
            case Message::Type::Unsubscribe:
                qt_noop();
                break;
            default:
                incomingMessage(message,
                                NetworkAddress(sender->peerAddress(),
                                               sender->peerPort()));
                break;
            }

            tryProcessIncomingMessage(sender);
        }
    }
}

UdpServer::UdpServer(const NetworkAddress& address, QObject* parent) :
    QObject(parent),
    Server(address),
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
    QHostAddress bindingAddress = (m_address.address == QHostAddress::LocalHost ? m_address.address
                                                                                : (m_address.address.protocol() == QUdpSocket::IPv6Protocol ? QHostAddress::AnyIPv6
                                                                                                                                            : QHostAddress::AnyIPv4));
    bool ok = m_incoming->bind(bindingAddress, m_address.port);
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
    NetworkAddress peer;
    QByteArray datagram;

    while (m_incoming->hasPendingDatagrams())
    {
        datagram.resize(m_incoming->pendingDatagramSize());
        m_incoming->readDatagram(datagram.data(), datagram.size(), &peer.address, &peer.port);
        if (   m_address.address != QHostAddress::LocalHost
            && m_address.address != QHostAddress::Any)
        {
            if (peer.address != m_address.address)
            {
                continue;
            }
        }

        QHash<QUdpSocket*, QByteArray>::iterator it = m_clients.begin();
        while (it != m_clients.end())
        {
            QUdpSocket* socket = it.key();
            if (   socket->peerAddress() == peer.address
                && socket->peerPort() == peer.port)
            {
                break;
            }
        }

        if (it == m_clients.end())
        {
            m_receivedBytes[peer].append(datagram);
            tryProcessIncomingMessage(peer, m_receivedBytes[peer]);
        }
        else
        {
            it->append(datagram);
            tryProcessIncomingMessage(peer, *it);
        }
    }
}

void UdpServer::tryProcessIncomingMessage(const NetworkAddress& peer, QByteArray& rawBytes)
{
    if (rawBytes.isEmpty())
    {
        return;
    }

    quint32 expectedSize = 0;
    {
        QDataStream input(&rawBytes, QIODevice::ReadOnly);
        input >> expectedSize;
    }
    if (   expectedSize > 0
        && static_cast<quint32>(rawBytes.size() - sizeof(expectedSize)) >= expectedSize)
    {
        Message message;
        {
            QDataStream input(rawBytes);
            input >> message;
        }
        rawBytes = rawBytes.right(rawBytes.size() - sizeof(expectedSize) - expectedSize);

        switch (message.type())
        {
        case Message::Type::Unknown:
            qt_noop();
            break;
        case Message::Type::Subscribe:
            addSubscriber(peer);
            if (rawBytes.isEmpty())
            {
                m_receivedBytes.remove(peer);
            }
            else
            {
                tryProcessIncomingMessage(peer, rawBytes);
            }
            break;
        case Message::Type::Unsubscribe:
            removeSubscriber(peer);
            break;
        default:
            incomingMessage(message, peer);
            tryProcessIncomingMessage(peer, rawBytes);
            break;
        }
    }
}

void UdpServer::addSubscriber(const NetworkAddress& peer)
{
    QUdpSocket* socket = new QUdpSocket(this);
    connect(socket, &QUdpSocket::readyRead,
            this, &UdpServer::slotReadDatagram);
    connect(socket, static_cast<void(QUdpSocket::*)(QAbstractSocket::SocketError)>(&QUdpSocket::error),
            this, &UdpServer::slotOnError);

    m_clients.insert(socket, QByteArray());

    socket->connectToHost(peer.address, peer.port);
    addConnection(socket);
}

void UdpServer::removeSubscriber(const NetworkAddress& peer)
{
    QHash<QUdpSocket*, QByteArray>::iterator it = m_clients.begin();
    while (it != m_clients.end())
    {
        QUdpSocket* socket = it.key();
        if (   socket->peerAddress() == peer.address
            && socket->peerPort() == peer.port)
        {
            removeConnection(socket);
            socket->close();
            socket->deleteLater();
            m_clients.erase(it);
            break;
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
