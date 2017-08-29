#ifndef NETCOM_CLIENT_H
#define NETCOM_CLIENT_H

#include <QByteArray>
#include <QWidget>

#include <protocol.h>

class QAbstractSocket;
class QTimer;

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
    void slotReadResponse();

private:
    void enableControls(bool enabled);

    bool createConnection();
    void removeConnection();

    void sendSubscribe();
    void sendUnsubscribe();
    void sendInfoRequest();
    void sendMessage(Message::Type type);

    void tryProcessResponse();

private:
    Ui::ClientWidget* m_ui;

    QAbstractSocket* m_socket = nullptr;
    QByteArray m_receivedBytes;

    QTimer* m_timer = nullptr;

};

} // Netcm

#endif // NETCOM_CLIENT_H
