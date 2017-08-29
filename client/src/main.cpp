#include <csignal>

#include <QApplication>

#include "client.h"

int main(int argc, char *argv[])
{
    auto sighandler = [](int sigcode) { return qApp->exit(sigcode); };

    ::signal(SIGINT,  sighandler);
    ::signal(SIGTERM, sighandler);

    QApplication app(argc, argv);
    app.setApplicationName(app.tr("Test Network Client"));

    Netcom::Client client;
    client.show();

    return app.exec();
}
