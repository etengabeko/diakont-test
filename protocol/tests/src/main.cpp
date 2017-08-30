#include <QtTest>

#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QString>

#include "protocol.h"

class SerializeTest : public QObject
{
    Q_OBJECT

private slots:
    void slotSerializeTest()
    {
        using namespace Netcom;

        Message original(Message::Type::InfoResponse);
        original.setBackwardPort(54321);

        original.addClientInfo(ClientInfo("127.0.0.1",   12345, QDateTime::fromString("10:00:00 28-06-2017", "hh:mm:ss dd-MM-yyyy")));
        original.addClientInfo(ClientInfo("localhost",   23456, QDateTime::fromString("11:11:11 29-07-2017", "hh:mm:ss dd-MM-yyyy")));
        original.addClientInfo(ClientInfo("lorem_ipsum", 34567, QDateTime::fromString("12:12:12 30-08-2017", "hh:mm:ss dd-MM-yyyy")));

        QByteArray serialized;
        {
            QDataStream output(&serialized, QIODevice::WriteOnly);
            output << original;
        }
        
        Message parsed;
        {
            QDataStream input(serialized);
            input >> parsed;
        }

        QCOMPARE(original.type(), parsed.type());
        QCOMPARE(original.clientsInfo(), parsed.clientsInfo());
    }

};

QTEST_MAIN(SerializeTest)

#include "main.moc"
