#include "client.h"
#include "ui_clientwidget.h"

#include <QCloseEvent>
#include <QDataStream>
#include <QDebug>
#include <QHostAddress>
#include <QMessageBox>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

namespace
{

int customTimerIntervalMsec() { return 1000; }

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
    }
}

void Client::slotConnect()
{
    if (createConnection())
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

bool Client::createConnection()
{
    if (m_socket != nullptr)
    {
        removeConnection();
    }
    m_receivedBytes.clear();

    enum
    {
        Tcp,
        Udp
    } protocol = (m_ui->tcpRadioButton->isChecked() ? Tcp : Udp);

    if (protocol == Tcp)
    {
        m_socket = new QTcpSocket(this);
    }
    else
    {
        m_socket = new QUdpSocket(this);
    }
    connect(m_socket, &QAbstractSocket::readyRead,
            this, &Client::slotReadResponse);

    QString strAddress = m_ui->addressLineEdit->text();
    QHostAddress address = (strAddress.toLower() == "localhost") ? QHostAddress(QHostAddress::LocalHost)
                                                                 : QHostAddress(strAddress);
    m_socket->connectToHost(address, m_ui->portSpinBox->value());

    bool ok = m_socket->waitForConnected();
    if (ok)
    {
        sendSubscribe();
    }
    else
    {
        QMessageBox::warning(this,
                             tr("Connection error"),
                             m_socket->errorString());
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
        QByteArray message;
        {
            QDataStream output(&message, QIODevice::WriteOnly);
            output << request;
        }
        m_socket->write(message);
    }
}

void Client::slotReadResponse()
{
    if (m_socket == sender())
    {
        m_receivedBytes.append(m_socket->readAll());
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

        // TODO
        qDebug().noquote() << tr("Received message type=%1").arg(static_cast<quint8>(response.type()));

        tryProcessResponse();
    }
}

} // Netcom
