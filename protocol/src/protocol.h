#ifndef NETCOM_PROTOCOL_H
#define NETCOM_PROTOCOL_H

#include <QDateTime>
#include <QList>
#include <QString>

class QByteArray;
class QDataStream;

namespace Netcom
{
/**
 * @struct ClientInfo
 * @brief  Информация о подключённом к серверу клиенте.
 */
struct ClientInfo
{
    QString address;    //!< ip-адрес клиента.
    quint16 port = 0;   //!< порт клиента.
    QDateTime datetime; //!< время подключения.

    ClientInfo() = default;
    ClientInfo(const QString& a, quint16 p, const QDateTime& d);

    bool operator== (const ClientInfo& rhs) const;
    bool operator!= (const ClientInfo& rhs) const;
};

/**
 * @class Message
 * @brief Сообщение инкапсулирующее протокол обмена информацией между сервером и клиентами.
 */
class Message
{
public:
    /**
     * @enum  Type
     * @brief Тип сообщения.
     */
    enum class Type
    {
        Unknown = 0, //!< тип не определён.
        Subscribe,   //!< запрос на регистрацию (клиент -> сервер).
        Unsubscribe, //!< запрос на отмену регистрации (клиент -> сервер).
        InfoRequest, //!< запрос списка всех клиентов (клиент -> сервер).
        InfoResponse //!< ответ на запрос - список клиентов (сервер -> клиент).
    };

public:
    Message() = default;
    explicit Message(Type type);

    /**
     * @brief  type - возвращает тип сообщения.
     * @return тип сообщения.
     */
    Type type() const;

    /**
     * @brief setType - устанавливает тип сообщения.
     * @param type - новый тип сообщения.
     */
    void setType(Type type);

    /**
     * @brief  backwardPort - возвращает номер порта клиента, на котором тот ожидает ответ.
     * @return номер порта.
     */
    quint16 backwardPort() const;

    /**
     * @brief setBackwardPort - устанавливает номер порта ожидания ответа.
     * @param port - новый номер порта.
     */
    void setBackwardPort(quint16 port);

    /**
     * @brief  clientsInfo - возвращает список информации о клиентах.
     * @return список клиентов.
     */
    const QList<ClientInfo>& clientsInfo() const;

    /**
     * @brief setClientsInfo - заменяет список информации о клиентах.
     * @param info - новый список клиентов.
     */
    void setClientsInfo(const QList<ClientInfo>& info);

    /**
     * @brief resetClientsInfo - очищает список информации о клиентах.
     */
    void resetClientsInfo();

    /**
     * @brief addClientInfo - добавляет информацию о клиенте в список.
     * @param info - информация о клиенте.
     */
    void addClientInfo(const ClientInfo& info);

    /**
     * @brief  serialize - сериализует объект Message в массив байт для передачи.
     * @return массив байт (UTF-8).
     */
    QByteArray serialize() const;

    /**
     * @brief  parse - заполняет объект Message из массива байт.
     * @param  raw - массив байт (UTF-8).
     * @param  ok - флаг успешности выполнения десериализации.
     * @return объект Message.
     */
    static Message parse(const QByteArray& raw, bool* ok = nullptr);

    /**
     * @brief  typeToString - преобразует значение Type в его строковое представление.
     * @param  type - значение для преобразования.
     * @return строка, соответствующая Type.
     */
    static QString typeToString(Type type);

    /**
     * @brief  typeFromString - преобразует строку в соответствующее значение Type.
     * @param  type - строка для преобразования.
     * @return значение Type (Type::Unknown - если преобразовать не удалось).
     */
    static Type typeFromString(const QString& type);

private:
    Type m_type = Type::Unknown; //!< тип сообщения.
    quint16 m_backwardPort = 0;  //!< порт приёма ответа.

    QList<ClientInfo> m_info;    //!< список клиентов.

};

QDataStream& operator<< (QDataStream& to, const Message& from);
QDataStream& operator>> (QDataStream& from, Message& to);

} // Netcom

#endif // NETCOM_PROTOCOL_H
