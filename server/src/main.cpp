#include <csignal>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "server.h"

int main(int argc, char *argv[])
{
    auto sighandler = [](int sigcode) { return qApp->exit(sigcode); };

    ::signal(SIGINT,  sighandler);
    ::signal(SIGTERM, sighandler);

    QCoreApplication app(argc, argv);
    app.setApplicationName(app.tr("Test Network Server"));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addPositionalArgument("url", app.tr("Server options: <protocol>://<address>:<port>."));

    parser.process(app);

    if (parser.isSet("help"))
    {
        parser.showHelp();
    }

    QStringList args = parser.positionalArguments();
    if (args.isEmpty())
    {
        qCritical().noquote() << app.tr("Expected Server options.");
        parser.showHelp(EXIT_FAILURE);
    }

    QUrl serverOptions(args.first());
    if (!serverOptions.isValid())
    {
        qWarning().noquote() << serverOptions.errorString();
        return EXIT_FAILURE;
    }

    QString protocol = serverOptions.scheme();
    QString address = args.first().contains('@') ? "@"
                                                 : serverOptions.host();
    int port = serverOptions.port();

    std::unique_ptr<Netcom::Server> server = Netcom::Server::createServer(protocol,
                                                                          (address == "localhost" ? QHostAddress(QHostAddress::LocalHost)
                                                                                                  : (address == "@" ? QHostAddress::Any
                                                                                                                    : QHostAddress(address))),
                                                                          port);
    if (   server != nullptr
        && server->start())
    {
        return app.exec();
    }

    qWarning().noquote() << app.tr("Failed start server: protocol=%1, address=%2, port=%3:\n%4")
                            .arg(protocol)
                            .arg(address)
                            .arg(port)
                            .arg(server->errorString());
    return EXIT_FAILURE;
}
