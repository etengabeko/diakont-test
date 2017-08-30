#ifndef NETCOM_CLIENT_H
#define NETCOM_CLIENT_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QWidget>

#include <protocol.h>

class QTimer;
template <typename T> class QList;

namespace Ui
{
class ClientWidget;
} // Ui

namespace Netcom
{

class Client : public QWidget
{
    Q_OBJECT

public:
    explicit Client(QWidget* parent = nullptr);
    ~Client();

protected:
    void closeEvent(QCloseEvent* event);

private slots:
    void slotConnect();
    void slotDisconnect();
    void slotTimeout();

    void slotReadTcpResponse();
    void slotReadUdpResponse();

private:
    void enableControls(bool enabled);
    void showClientsList(const QList<ClientInfo>& clients);

    bool createConnection(QAbstractSocket::SocketType type);
    void removeConnection();

    void sendSubscribe();
    void sendUnsubscribe();
    void sendInfoRequest();
    void sendMessage(Message::Type type);

    void tryProcessResponse();

private:
    Ui::ClientWidget* m_ui;

    QAbstractSocket* m_socket = nullptr;
    quint16 m_incomingPort = 0;
    QByteArray m_receivedBytes;

    QTimer* m_timer = nullptr;

};

} // Netcm

#endif // NETCOM_CLIENT_H
