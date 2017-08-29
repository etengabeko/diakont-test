TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS += \
    protocol \
    server \
    client

server.depends = protocol
client.depends = protocol
