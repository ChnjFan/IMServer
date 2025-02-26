//
// Created by fan on 25-2-25.
//

#include "MessageHandler.h"

MessageHandler::MessageHandler(Poco::Net::StreamSocket &socket, Poco::Net::SocketReactor &reactor)
        : socket(socket)
        , reactor(reactor)
        , readObserver(*this, &MessageHandler::onReadable)
        , writeObserver(*this, &MessageHandler::onWritable)
        , shutdownObserver(*this, &MessageHandler::onShutdown)
{
    reactor.addEventHandler(socket, readObserver);
    reactor.addEventHandler(socket, writeObserver);
    reactor.addEventHandler(socket, shutdownObserver);
}

void MessageHandler::onReadable(Poco::Net::ReadableNotification *pNotification) {

}

void MessageHandler::onWritable(Poco::Net::WritableNotification *pNotification) {

}

void MessageHandler::onShutdown(Poco::Net::ShutdownNotification *pNotification) {

}
