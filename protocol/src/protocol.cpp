#include "protocol.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QDebug>
#include <QDataStream>
#include <QDomDocument>
#include <QDomElement>
#include <QMap>

namespace
{

const QString dateTimeFormat() { return "hh:mm:ss dd-MM-yyyy"; }

const QMap<Netcom::Message::Type, QString>& messageTypes()
{
    static const QMap<Netcom::Message::Type, QString> types({
                                                                { Netcom::Message::Type::Subscribe,    "subscribe"     },
                                                                { Netcom::Message::Type::Unsubscribe,  "unsubscribe"   },
                                                                { Netcom::Message::Type::InfoRequest,  "info_request"  },
                                                                { Netcom::Message::Type::InfoResponse, "info_response" },
                                                                { Netcom::Message::Type::Unknown,      "unknown"       }
                                                            });
    return types;
}

}

namespace Netcom
{

ClientInfo::ClientInfo(const QString& a, quint16 p, const QDateTime& d) :
    address(a),
    port(p),
    datetime(d)
{

}

bool ClientInfo::operator== (const ClientInfo& rhs) const
{
    if (this == &rhs)
    {
        return true;
    }

    return (   address == rhs.address
            && port == rhs.port
            && datetime == rhs.datetime);
}

bool ClientInfo::operator!= (const ClientInfo& rhs) const
{
    return !(*this == rhs);
}

Message::Message(Type type) :
    m_type(type)
{

}

Message::Type Message::type() const
{
    return m_type;
}

void Message::setType(Type type)
{
    m_type = type;
}

quint16 Message::backwardPort() const
{
    return m_backwardPort;
}

void Message::setBackwardPort(quint16 port)
{
    m_backwardPort = port;
}

const QList<ClientInfo>& Message::clientsInfo() const
{
    return m_info;
}

void Message::setClientsInfo(const QList<ClientInfo>& info)
{
    m_info = info;
}

void Message::addClientInfo(const ClientInfo& info)
{
    m_info.append(info);
}

void Message::resetClientsInfo()
{
    m_info.clear();
}

QByteArray Message::serialize() const
{
    QDomDocument doc("netcom");
    QDomElement root = doc.createElement("netcom");
    doc.appendChild(root);
    QDomElement message = doc.createElement("message");
    message.setAttribute("type", typeToString(m_type));
    root.appendChild(message);
    if (!m_info.isEmpty())
    {
        QDomElement clients = doc.createElement("clients");
        message.appendChild(clients);

        for (const ClientInfo& each : m_info)
        {
            QDomElement eachClient = doc.createElement("client");
            eachClient.setAttribute("address", each.address);
            eachClient.setAttribute("port", each.port);
            eachClient.setAttribute("datetime", each.datetime.toString(::dateTimeFormat()));
            clients.appendChild(eachClient);
        }
    }
    if (m_backwardPort > 0)
    {
        QDomElement options = doc.createElement("options");
        options.setAttribute("backward_port", m_backwardPort);
        root.appendChild(options);
    }

    return doc.toByteArray();
}

Message Message::parse(const QByteArray& raw, bool *ok)
{
    if (ok != nullptr)
    {
        *ok = false;
    }

    Message result;

    QDomDocument doc;
    QString errorMessage;
    int errorLine = 0,
        errorColumn = 0;
    if (doc.setContent(raw, &errorMessage, &errorLine, &errorColumn))
    {
        QDomElement root = doc.documentElement();
        QDomNodeList messageChildren = root.elementsByTagName("message");
        for (int i = 0, isz = messageChildren.size(); i < isz; ++i)
        {
            if (!messageChildren.item(i).isElement())
            {
                continue;
            }

            QDomElement el = messageChildren.item(i).toElement();
            if (el.hasAttribute("type"))
            {
                result.m_type = typeFromString(el.attribute("type"));
            }

            if (!el.hasChildNodes())
            {
                continue;
            }
            QDomNodeList clientsChildren = el.elementsByTagName("clients");
            for (int j = 0, jsz = clientsChildren.size(); j < jsz; ++j)
            {
                if (!clientsChildren.item(j).isElement())
                {
                    continue;
                }
                QDomNodeList clientChildren = clientsChildren.item(j).toElement().elementsByTagName("client");
                for (int k = 0, ksz = clientChildren.size(); k < ksz; ++k)
                {
                    if (!clientChildren.item(k).isElement())
                    {
                        continue;
                    }
                    QDomElement client = clientChildren.item(k).toElement();
                    if (   client.hasAttribute("address")
                        && client.hasAttribute("port")
                        && client.hasAttribute("datetime"))
                    {
                        result.addClientInfo(ClientInfo(client.attribute("address"),
                                                        client.attribute("port").toUInt(),
                                                        QDateTime::fromString(client.attribute("datetime"), ::dateTimeFormat())));
                    }
                }
            }
        }
        QDomNodeList optionsChildren = root.elementsByTagName("options");
        for (int i = 0, sz = optionsChildren.size(); i < sz; ++i)
        {
            if (!optionsChildren.item(i).isElement())
            {
                continue;
            }
            QDomElement el = optionsChildren.item(i).toElement();
            if (el.hasAttribute("backward_port"))
            {
                result.m_backwardPort = el.attribute("backward_port").toUInt();
            }
        }
    }
    else
    {
        qWarning() << qApp->tr("Message parsing error: %1 in line %2, column %3")
                      .arg(errorMessage)
                      .arg(errorLine)
                      .arg(errorColumn);
    }

    if (ok != nullptr)
    {
        switch (result.type())
        {
        case Type::Subscribe:
        case Type::Unsubscribe:
        case Type::InfoRequest:
        case Type::InfoResponse:
            *ok = true;
            break;
        default:
            break;
        }
    }

    return result;
}

QString Message::typeToString(Type type)
{
    if (::messageTypes().contains(type))
    {
        return ::messageTypes()[type];
    }
    return ::messageTypes()[Message::Type::Unknown];
}

Message::Type Message::typeFromString(const QString& type)
{
    auto founded = std::find_if(::messageTypes().cbegin(),
                                ::messageTypes().cend(),
                                [&type](const QString& each)
                                {
                                    return type == each;
                                });
    return (founded != ::messageTypes().cend() ? founded.key()
                                               : Message::Type::Unknown);
}

QDataStream& operator<< (QDataStream& to, const Message& from)
{
    QByteArray raw = from.serialize();
    quint32 size = raw.size();
    to << size;
    to.writeRawData(raw.data(), size);

    return to;
}

QDataStream& operator>> (QDataStream& from, Message& to)
{
    quint32 size = 0;
    from >> size;
    QByteArray raw(size, '\0');
    from.readRawData(raw.data(), size);
    to = Message::parse(raw);

    return from;
}

} // Netcom
