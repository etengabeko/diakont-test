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

/**
 * @class Client
 * @brief Определяет класс клиента для подключения к серверу
 *        и отображения актуальной информации обо всех подключенных к серверу клиентах.
 */
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

    QTimer* m_timer;                     //!< таймер для периодической отправки запросов на сервер.

    QAbstractSocket* m_socket = nullptr; //!< сокет, обеспечивающий связь с сервером.
    quint16 m_incomingPort = 0;          //!< порт, на котором ожидается ответ от сервера.
    QByteArray m_receivedBytes;          //!< буфер для принимаемой от сервера информации.

};

} // Netcm

#endif // NETCOM_CLIENT_H
