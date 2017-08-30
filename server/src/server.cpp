#include "server.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QTextStream>
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

QAbstractSocket::SocketType protocolFromString(const QString& str)
{
    static const QHash<QString, QAbstractSocket::SocketType> protocols({ { "tcp", QAbstractSocket::TcpSocket },
                                                                         { "udp", QAbstractSocket::UdpSocket }
                                                                       });
    if (protocols.contains(str.toLower()))
    {
        return protocols[str.toLower()];
    }
    return QAbstractSocket::UnknownSocketType;
}

std::unique_ptr<Server> Server::createServer(const QString& protocolName,
                                             const NetworkAddress& address)
{
    return createServer(protocolFromString(protocolName), address);
}

std::unique_ptr<Server> Server::createServer(QAbstractSocket::SocketType protocol,
                                             const NetworkAddress& address)
{
    std::unique_ptr<Server> result;

    switch (protocol)
    {
    case QAbstractSocket::TcpSocket:
        result.reset(new TcpServer(address));
        break;
    case QAbstractSocket::UdpSocket:
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
        logging(qApp->tr("%1 - Added connection from %2:%3")
                .arg(m_activeConnections[socket].toString("hh:mm:ss.zzz"))
                .arg(socket->peerAddress().toString())
                .arg(socket->peerPort()),
                QtInfoMsg);
    }
}

void Server::removeConnection(QAbstractSocket* socket)
{
    Q_CHECK_PTR(socket);

    if (m_activeConnections.contains(socket))
    {
        m_activeConnections.remove(socket);
        logging(qApp->tr("%1 - Removed connection from %2:%3")
                .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                .arg(socket->peerAddress().toString())
                .arg(socket->peerPort()),
                QtInfoMsg);
    }
}

QString Server::errorString() const
{
    return m_lastError;
}

void Server::setLogFileName(const QString& fileName)
{
    m_logFileName = fileName;
}

void Server::incomingMessage(const Message& message, QAbstractSocket* sender)
{
    Q_CHECK_PTR(sender);

    logging(qApp->tr("%1 - Incoming message from %2:%3]:\n%4")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
            .arg(sender->peerAddress().toString())
            .arg(sender->peerPort())
            .arg(QString::fromUtf8(message.serialize())),
            QtInfoMsg);
    switch (message.type())
    {
    case Message::Type::InfoRequest:
        {
            Message response(Message::Type::InfoResponse);
            QHashIterator<QAbstractSocket*, QDateTime> it(m_activeConnections);
            while (it.hasNext())
            {
                QAbstractSocket* eachClient = it.peekNext().key();
                const QDateTime& eachDateTime = it.peekNext().value();
                response.addClientInfo(ClientInfo(eachClient->peerAddress().toString(),
                                                  eachClient->peerPort(),
                                                  eachDateTime));
                it.next();
            }

            QByteArray serialized;
            {
                QDataStream output(&serialized, QIODevice::WriteOnly);
                output << response;
            }

            auto founded = m_activeConnections.find(sender);
            if (founded != m_activeConnections.end())
            {
                founded.key()->write(serialized);
            }
        }
    default:
        break;
    }
}

void Server::logging(const QString& message, QtMsgType type) const
{
    switch (type)
    {
    case QtDebugMsg:
        qDebug().noquote() << message;
        break;
    case QtInfoMsg:
        qInfo().noquote() << message;
        break;
    case QtWarningMsg:
        qWarning().noquote() << message;
        break;
    case QtCriticalMsg:
    default:
        qCritical().noquote() << message;
        break;
    }

    if (!m_logFileName.isEmpty())
    {
        QFile f(m_logFileName);
        if (f.open(QFile::Append))
        {
            QTextStream output(&f);
            output << message << endl;
        }
        else
        {
            qWarning().noquote() << f.errorString();
        }
    }
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
            logging(tr("%1 - Discard connection from %2. Expected only %3.")
                    .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                    .arg(socket->peerAddress().toString())
                    .arg(m_address.address.toString()),
                    QtWarningMsg);
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
            logging(tr("%1 - Error %2:%3: %4")
                    .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                    .arg(socket->peerAddress().toString())
                    .arg(socket->peerPort())
                    .arg(m_lastError),
                    QtWarningMsg);
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
            case Message::Type::InfoRequest:
            case Message::Type::InfoResponse:
                incomingMessage(message, sender);
                break;
            case Message::Type::Unknown:
            case Message::Type::Subscribe:
            case Message::Type::Unsubscribe:
            default:
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

    QHash<NetworkAddress, std::tuple<QUdpSocket*, QByteArray>>::iterator it = m_clients.begin();
    while (it != m_clients.end())
    {
        QUdpSocket* each = std::get<QUdpSocket*>(*it);
        if (each != nullptr)
        {
            removeConnection(each);
            each->close();
            each->deleteLater();
        }
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
                logging(tr("%1 - Discard connection from %2. Expected only %3.")
                        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                        .arg(peer.address.toString())
                        .arg(m_address.address.toString()),
                        QtWarningMsg);
                continue;
            }
        }

        if (!m_clients.contains(peer))
        {
            m_clients.insert(peer, std::make_tuple(static_cast<QUdpSocket*>(nullptr), QByteArray()));
        }
        std::get<QByteArray>(m_clients[peer]).append(datagram);

        tryProcessIncomingMessage(peer);
    }
}

void UdpServer::tryProcessIncomingMessage(const NetworkAddress& peer)
{
    if (   !m_clients.contains(peer)
        || std::get<QByteArray>(m_clients[peer]).isEmpty())
    {
        return;
    }

    QByteArray& receivedBytes = std::get<QByteArray>(m_clients[peer]);
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
        case Message::Type::InfoRequest:
        case Message::Type::InfoResponse:
            {
                QUdpSocket* backSocket = std::get<QUdpSocket*>(m_clients[peer]);
                if (backSocket != nullptr)
                {
                    incomingMessage(message, backSocket);
                }
            }
            break;
        case Message::Type::Subscribe:
            addSubscriber(peer, message.backwardPort());
            break;
        case Message::Type::Unsubscribe:
            removeSubscriber(peer);
            break;
        case Message::Type::Unknown:
        default:
            break;
        }

        tryProcessIncomingMessage(peer);
    }
}

void UdpServer::addSubscriber(const NetworkAddress& peer, quint16 peerIncomingPort)
{
    if (   m_clients.contains(peer)
        && std::get<QUdpSocket*>(m_clients[peer]) == nullptr)
    {
        QUdpSocket* socket = new QUdpSocket(this);
        connect(socket, static_cast<void(QUdpSocket::*)(QAbstractSocket::SocketError)>(&QUdpSocket::error),
                this, &UdpServer::slotOnError);

        socket->connectToHost(peer.address, peerIncomingPort);
        std::get<QUdpSocket*>(m_clients[peer]) = socket;

        addConnection(socket);
    }
}

void UdpServer::removeSubscriber(const NetworkAddress& peer)
{
    if (m_clients.contains(peer))
    {
        QUdpSocket* socket = std::get<QUdpSocket*>(m_clients[peer]);
        if (socket != nullptr)
        {
            removeConnection(socket);
            socket->close();
            socket->deleteLater();
            m_clients.remove(peer);
        }
    }
}

void UdpServer::slotOnError()
{
    QUdpSocket* socket = qobject_cast<QUdpSocket*>(sender());
    if (socket != nullptr)
    {
        m_lastError = socket->errorString();
        logging(tr("%1 - Error [%2:%3]: %4")
                .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
                .arg(socket->peerAddress().toString())
                .arg(socket->peerPort())
                .arg(m_lastError),
                QtWarningMsg);
    }
}

} // Netcom
