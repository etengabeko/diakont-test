#ifndef NETCOM_SERVER_H
#define NETCOM_SERVER_H

#include <memory>
#include <tuple>

#include <QAbstractSocket>
#include <QDateTime>
#include <QHostAddress>
#include <QObject>
#include <QHash>
#include <QString>

class QTcpServer;
class QTcpSocket;
class QUdpSocket;

namespace Netcom
{
class Message;

QAbstractSocket::SocketType protocolFromString(const QString& str);

/**
 * @struct NetworkAddress
 * @brief  структура для объединения параметров подключения: ip-адрес и порт.
 */
struct NetworkAddress
{
    QHostAddress address; //!< ip-адрес подключения.
    quint16 port = 0;     //!< порт подключения.

    NetworkAddress() = default;
    NetworkAddress(const QHostAddress& a, quint16 p);

    bool operator== (const NetworkAddress& rhs) const;
    bool operator!= (const NetworkAddress& rhs) const;
};

/**
 * @class Server
 * @brief Определяет класс сервера, открывающего порт для приёма входящих подключений (TCP/UDP),
 *        сохраняющего информацию о подключенных к нему клиентах
 *        и по запросу возвращающего список всех подключенных клиентов.
 */
class Server
{
public:
    explicit Server(const NetworkAddress& address);
    virtual ~Server() = 0;

    /**
     * @brief  createServer - функии-фабрики для создания объекта-сервера в зависимости от переданного типа подключения: TCP/UDP.
     * @param  protocol - требуемый тип сервера (TCP/UDP).
     * @param  address - адрес приёма: порт, который будет открыт для приёма подключений и
     *                   ip-адрес клиента, которому разрешается выполнять подключения (@ - для приёма подключений от любых клиентов).
     * @return созданный объект-сервер. nullptr в случае ошибки.
     */
    static std::unique_ptr<Server> createServer(QAbstractSocket::SocketType protocol,
                                                const NetworkAddress& address);
    static std::unique_ptr<Server> createServer(const QString& protocolName,
                                                const NetworkAddress& address);

    /**
     * @brief  start - запускает сервер.
     * @return флаг успешности запуска сервера.
     */
    bool start();

    /**
     * @brief stop - останавливает сервер.
     */
    void stop();

    /**
     * @brief  errorString - возвращает текст сообщения о последней ошибке.
     * @return текст сообщения об ошибке.
     */
    QString errorString() const;

    /**
     * @brief setLogFileName - устанавливает имя файла журнала.
     * @param fileName - новое имя файла журнала.
     */
    void setLogFileName(const QString& fileName);

protected:
    virtual bool run() = 0;
    virtual void finish() = 0;

    /**
     * @brief incomingMessage - общий обработчик полученных от клиентов запросов.
     * @param message - запрос для обработки.
     * @param sender - отправитель запроса.
     *
     * @note  Формирует и отправляет обратно отправителю ответ на запрос информации об активных клиентах.
     */
    void incomingMessage(const Message& message, QAbstractSocket* sender);

    /**
     * @brief addConnection - добавляет клиента в список активных клиентов.
     * @param socket - добавляемый клиент.
     */
    void addConnection(QAbstractSocket* socket);

    /**
     * @brief removeConnection - удаляет клиента из списка активных клиентов.
     * @param socket - удаляемый клиент.
     */
    void removeConnection(QAbstractSocket* socket);

    /**
     * @brief logging - вывод сообщения в консоль и при необходимости в журнал.
     * @param message - текст выводимого сообщения.
     * @param type - тип выводимого сообщения.
     */
    void logging(const QString& message, QtMsgType type) const;

protected:
    QString m_lastError;      //!< последнее сообщение об ошибке.
    NetworkAddress m_address; //!< параметры сервера: порт для входящих подключений, ip-адрес разрешённого клиента.

private:
    QHash<QAbstractSocket*, QDateTime> m_activeConnections; //!< список активных клиентов со временем их подключения.
    QString m_logFileName;    //!< имя файла журнала (если пустое - журнал не ведётся).

};

/**
 * @class TcpServer
 * @brief Реализация TCP-сервера.
 */
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
    QTcpServer* m_srv; //!< объект-приёник TCP-подключений.
    QHash<QTcpSocket*, QByteArray> m_clients; //!< активные соединения и буферы приёма входящей информации для них.

};

/**
 * @class UdpServer
 * @brief Реализация UDP-сервера.
 */
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

    void addSubscriber(const NetworkAddress& peer, quint16 peerIncomingPort);
    void removeSubscriber(const NetworkAddress& peer);

    void tryProcessIncomingMessage(const NetworkAddress& peer);

private:
    QUdpSocket* m_incoming; //!< объект-приёмник UDP-датаграмм.
    QHash<NetworkAddress, std::tuple<QUdpSocket*, QByteArray>> m_clients; //!< объекты для отправки сообщений зарегистрировавшимся клиентам и буферы приёма входящей от клиентов информации.

};

inline uint qHash(const NetworkAddress& key, uint seed = 0)
{
    return (::qHash(key.address.toString(), seed) ^ ::qHash(key.port, seed));
}

} // Netcom

#endif // NETCOM_SERVER_H
