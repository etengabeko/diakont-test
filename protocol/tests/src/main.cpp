#include <QtTest>

#include <QByteArray>
#include <QDataStream>
#include <QString>

#include "protocol.h"

namespace
{

QString loremIpsum()
{
    return "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
}

}

class SerializeTest : public QObject
{
    Q_OBJECT

private slots:
    void slotSerializeTest()
    {
        using namespace Netcom;

        Message original(Message::Type::InfoResponse);
        original.setBody(::loremIpsum().toUtf8());
        
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
        QCOMPARE(original.body(), parsed.body());
        QCOMPARE(QString::fromUtf8(parsed.body()), ::loremIpsum());
    }

};

QTEST_MAIN(SerializeTest)

#include "main.moc"
