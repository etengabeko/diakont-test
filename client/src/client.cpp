#include "client.h"
#include "ui_clientwidget.h"

#include <QCloseEvent>
#include <QDataStream>
#include <QHostAddress>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

namespace
{

int customTimerIntervalMsec() { return 1000; }

enum Column
{
    Address = 0,
    Port,
    Datetime
};

}

namespace Netcom
{

Client::Client(QWidget* parent) :
    QWidget(parent),
    m_ui(new Ui::ClientWidget()),
    m_timer(new QTimer(this))
{
    m_ui->setupUi(this);

    connect(m_ui->connectButton, &QPushButton::clicked,
            this, &Client::slotConnect);
    connect(m_ui->disconnectButton, &QPushButton::clicked,
            this, &Client::slotDisconnect);
    connect(m_ui->cancelButton, &QPushButton::clicked,
            this, &Client::close);

    m_timer->setInterval(::customTimerIntervalMsec());

    connect(m_timer, &QTimer::timeout,
            this, &Client::slotTimeout);

    m_ui->clientsTableWidget->horizontalHeader()->setResizeContentsPrecision(0);
    m_ui->clientsTableWidget->resizeColumnsToContents();
    m_ui->clientsTableWidget->horizontalHeader()->setSectionResizeMode(Datetime, QHeaderView::Stretch);
}

Client::~Client()
{
    m_timer->stop();

    if (m_socket != nullptr)
    {
        m_socket->close();
    }

    delete m_ui;
    m_ui = nullptr;
}

void Client::closeEvent(QCloseEvent* event)
{
    removeConnection();
    QWidget::closeEvent(event);
}

void Client::removeConnection()
{
    sendUnsubscribe();

    if (m_socket != nullptr)
    {
        m_socket->close();
        m_socket->deleteLater();
        m_socket = nullptr;

        m_incomingPort = 0;
        m_receivedBytes.clear();
    }

    m_ui->clientsTableWidget->clearContents();
}

void Client::slotConnect()
{
    QAbstractSocket::SocketType type = (m_ui->tcpRadioButton->isChecked() ? QAbstractSocket::TcpSocket
                                                                          : QAbstractSocket::UdpSocket);
    if (createConnection(type))
    {
        m_timer->start();
        enableControls(false);
    }
    else
    {
        removeConnection();
    }
}

void Client::slotDisconnect()
{
    m_timer->stop();
    removeConnection();
    enableControls(true);
}

void Client::enableControls(bool enabled)
{
    m_ui->tcpRadioButton->setEnabled(enabled);
    m_ui->udpRadioButton->setEnabled(enabled);
    m_ui->addressLineEdit->setReadOnly(!enabled);
    m_ui->portSpinBox->setReadOnly(!enabled);

    m_ui->connectButton->setEnabled(enabled);
    m_ui->disconnectButton->setEnabled(!enabled);
}

bool Client::createConnection(QAbstractSocket::SocketType type)
{
    if (m_socket != nullptr)
    {
        removeConnection();
    }

    QString strAddress = m_ui->addressLineEdit->text();
    QHostAddress remoteAddress = (strAddress.toLower() == "localhost") ? QHostAddress(QHostAddress::LocalHost)
                                                                       : QHostAddress(strAddress);
    quint16 remotePort = m_ui->portSpinBox->value();

    QString errorMessage;
    if (type == QAbstractSocket::TcpSocket)
    {
        m_socket = new QTcpSocket(this);
        connect(m_socket, &QTcpSocket::readyRead,
                this, &Client::slotReadTcpResponse);
        m_socket->connectToHost(remoteAddress, remotePort);
        if (m_socket->waitForConnected())
        {
            m_incomingPort = m_socket->localPort();
        }
        else
        {
            errorMessage = m_socket->errorString();
        }
    }
    else
    {
        m_socket = new QUdpSocket(this);
        QUdpSocket* incomingSocket = new QUdpSocket(m_socket);
        connect(incomingSocket, &QUdpSocket::readyRead,
                this, &Client::slotReadUdpResponse);
        if (incomingSocket->bind())
        {
            m_incomingPort = incomingSocket->localPort();
            m_socket->connectToHost(remoteAddress, remotePort);
        }
        else
        {
            errorMessage = incomingSocket->errorString();
        }
    }

    bool ok = errorMessage.isEmpty();
    if (ok)
    {
        sendSubscribe();
    }
    else
    {
        QMessageBox::warning(this,
                             tr("Connection error"),
                             errorMessage);
    }
    return ok;
}

void Client::slotTimeout()
{
    sendInfoRequest();
}

void Client::sendSubscribe()
{
    sendMessage(Message::Type::Subscribe);
}

void Client::sendUnsubscribe()
{
    sendMessage(Message::Type::Unsubscribe);
}

void Client::sendInfoRequest()
{
    sendMessage(Message::Type::InfoRequest);
}

void Client::sendMessage(Message::Type type)
{
    if (m_socket != nullptr)
    {
        Message request(type);
        request.setBackwardPort(m_incomingPort);
        QByteArray message;
        {
            QDataStream output(&message, QIODevice::WriteOnly);
            output << request;
        }
        m_socket->write(message);
    }
}

void Client::slotReadTcpResponse()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket != nullptr)
    {
        m_receivedBytes.append(socket->readAll());
        tryProcessResponse();
    }
}

void Client::slotReadUdpResponse()
{
    QUdpSocket* socket = qobject_cast<QUdpSocket*>(sender());
    if (socket != nullptr)
    {
        while (socket->hasPendingDatagrams())
        {
            QByteArray datagram(socket->pendingDatagramSize(), '\0');
            socket->readDatagram(datagram.data(), datagram.size());
            m_receivedBytes.append(datagram);
        }
        tryProcessResponse();
    }
}

void Client::tryProcessResponse()
{
    if (m_receivedBytes.isEmpty())
    {
        return;
    }

    quint32 expectedSize = 0;
    {
        QDataStream input(&m_receivedBytes, QIODevice::ReadOnly);
        input >> expectedSize;
    }

    if (   expectedSize > 0
        && static_cast<quint32>(m_receivedBytes.size() - sizeof(expectedSize)) >= expectedSize)
    {
        Message response;
        {
            QDataStream input(&m_receivedBytes, QIODevice::ReadOnly);
            input >> response;
        }

        m_receivedBytes = m_receivedBytes.right(m_receivedBytes.size() - sizeof(expectedSize) - expectedSize);

        showClientsList(response.clientsInfo());

        tryProcessResponse();
    }
}

void Client::showClientsList(const QList<ClientInfo>& clients)
{
    m_ui->clientsTableWidget->clearContents();
    m_ui->clientsTableWidget->setRowCount(clients.count());

    for (int row = 0, sz = clients.size(); row < sz; ++row)
    {
        const ClientInfo& each = clients.at(row);

        QTableWidgetItem* item = new QTableWidgetItem(each.address);
        item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        item->setTextAlignment(Qt::AlignCenter);
        m_ui->clientsTableWidget->setItem(row, Address, item);

        item = new QTableWidgetItem(QString::number(each.port));
        item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        item->setTextAlignment(Qt::AlignCenter);
        m_ui->clientsTableWidget->setItem(row, Port, item);

        item = new QTableWidgetItem(each.datetime.toString("hh:mm:ss dd-MM-yyyy"));
        item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        item->setTextAlignment(Qt::AlignCenter);
        m_ui->clientsTableWidget->setItem(row, Datetime, item);
    }

    m_ui->clientsTableWidget->horizontalHeader()->setResizeContentsPrecision(0);
    m_ui->clientsTableWidget->resizeColumnsToContents();
    m_ui->clientsTableWidget->horizontalHeader()->setSectionResizeMode(Datetime, QHeaderView::Stretch);
}

} // Netcom
