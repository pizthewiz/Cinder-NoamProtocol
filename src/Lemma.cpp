//
//  Lemma.cpp
//  SmokeSignals
//
//  Created by Jean-Pierre Mouilleseaux on 22 May 2014.
//  Copyright 2014 Chorded Constructions. All rights reserved.
//

#include "Lemma.h"
#include "cinder/app/App.h"
#include "boost/format.hpp"

namespace Cinder { namespace Noam {

using namespace ci;

static const std::string sLemmaDialect = "Cinder-NoamLemma";
static const std::string sLemmaVersion = "0.0.0";

static const std::string sAvailabilityBroadcastHost = "255.255.255.255";
static const uint16_t sAvailabilityBroadcastPort = 1030;
static const size_t sAvailabilityBroadcastInterval = 3 * 1000;
static const std::string sAvailabilityBroadcastHeader = "marco";
static const std::string sAvailabilityBroadcastResponseHeader = "polo";

static const std::string sRegistrationMessageHeader = "register";
static const std::string sHeartbeatHeader = "heartbeat";
static const size_t sHeartbeatInterval = 5 * 1000;
static const std::string sHeartbeatAckHeader = "heartbeat_ack";
static const std::string sEventMessageHeader = "event";

LemmaRef Lemma::create(const std::string& guestName, const std::string& roomName) {
    return LemmaRef(new Lemma(guestName, roomName))->shared_from_this();
}

Lemma::Lemma(const std::string& guestName, const std::string& roomName) : mConnected(false), mGuestName(guestName), mRoomName(roomName) {
}

Lemma::~Lemma() {
    end();

    mAvailabilityBroadcastTimer = nullptr;
    mUDPClient = nullptr;
    mUDPClientSession = nullptr;
    mUDPServer = nullptr;
    mUDPServerSession = nullptr;
    mTCPClient = nullptr;
    mTCPClientSession = nullptr;
    mTCPServer = nullptr;
    mTCPServerSession = nullptr;
    mHeartbeatTimer = nullptr;
}

#pragma mark -

void Lemma::connectMessageEventHandler(const std::string& eventName, const std::function<void(const std::string&, const std::string&)>& eventHandler) {
    if (mMessageEventHandlerMap.count(eventName)) {
        cinder::app::console() << "NOTICE - replacing message event handler for event \"" << eventName << "\"" << std::endl;
    }
    mMessageEventHandlerMap[eventName] = eventHandler;
}

void Lemma::sendMessage(const std::string& eventName, bool eventValue) {
    sendEventMessage(eventName, eventValue);
}

void Lemma::sendMessage(const std::string& eventName, double eventValue) {
    sendEventMessage(eventName, eventValue);
}

void Lemma::sendMessage(const std::string& eventName, float eventValue) {
    sendEventMessage(eventName, eventValue);
}

void Lemma::sendMessage(const std::string& eventName, int eventValue) {
    sendEventMessage(eventName, eventValue);
}

void Lemma::sendMessage(const std::string& eventName, const std::string& eventValue) {
    sendEventMessage(eventName, eventValue);
}

void Lemma::sendMessage(const std::string& eventName, const char* eventValue) {
    sendEventMessage(eventName, std::string(eventValue));
}

void Lemma::sendMessage(const std::string& eventName, const JsonTree& eventValue) {
    sendEventMessage(eventName, eventValue);
}

void Lemma::begin() {
    if (mConnected) {
        return;
    }
    setupDiscoveryClient();
}

#pragma mark -

void Lemma::end() {
    mConnected = false;

    if (mUDPClientSession && mUDPClientSession->getSocket()->is_open()) {
        boost::system::error_code err;
        mUDPClientSession->getSocket()->close(err);
        if (err) {
            cinder::app::console() << "ERROR - UDP client session failed to close - " << err << std::endl;
        }
    }
    if (mUDPServerSession && mUDPServerSession->getSocket()->is_open()) {
        boost::system::error_code err;
        mUDPServerSession->getSocket()->close(err);
        if (err) {
            cinder::app::console() << "ERROR - UDP server session failed to close - " << err << std::endl;
        }
    }

    if (mTCPClientSession && mTCPClientSession->getSocket()->is_open()) {
        mTCPClientSession->close();
    }
    if (mTCPServerSession && mTCPServerSession->getSocket()->is_open()) {
        mTCPServerSession->close();
    }
}

#pragma mark - DISCOVERY

void Lemma::setupDiscoveryClient() {
    mAvailabilityBroadcastTimer = WaitTimer::create(ci::app::App::get()->io_service());
    mAvailabilityBroadcastTimer->connectErrorEventHandler([](std::string message, size_t arg) {
        cinder::app::console() << "ERROR - availabilty broadcast timer - " << message << " " << arg << std::endl;
    });
    mAvailabilityBroadcastTimer->connectWaitEventHandler([&]() {
        sendAvailabilityBroadcast();
    });

    mUDPClient = UdpClient::create(ci::app::App::get()->io_service());
    mUDPClient->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
        cinder::app::console() << "ERROR - UDP client - " << err << std::endl;
    });
	mUDPClient->connectConnectEventHandler([&](UdpSessionRef session) {
        mUDPClientSession = session;
        mUDPClientSession->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
            cinder::app::console() << "ERROR - UDP client session - " << err << std::endl;
        });
        mUDPClientSession->connectWriteEventHandler([](size_t bytesTransferred) {
            cinder::app::console() << "NOTICE - UDP client session wrote " << bytesTransferred << " bytes" << std::endl;
        });

        // enable broadcast
        boost::asio::socket_base::broadcast broadcastOption(true);
        mUDPClientSession->getSocket()->set_option(broadcastOption);

        // enable address reuse
        boost::asio::socket_base::reuse_address addressOption(true);
        boost::system::error_code errorCode;
        mUDPClientSession->getSocket()->set_option(addressOption, errorCode);
        if (errorCode) {
            cinder::app::console() << "ERROR - UDP client session failed to enable address reuse " << errorCode << std::endl;
        }

        unsigned short localPort = mUDPClientSession->getSocket()->local_endpoint().port();
        cinder::app::console() << "NOTICE - UDP client session sending from port " << localPort << std::endl;

        setupDiscoveryServer(static_cast<uint16_t>(localPort));

        // TODO - wait until server is setup?
        sendAvailabilityBroadcast();
    });
    mUDPClient->connectResolveEventHandler([]() {
        cinder::app::console() << "NOTICE - UDP client endpoint resolved" << std::endl;
    });

    mUDPClient->connect(sAvailabilityBroadcastHost, sAvailabilityBroadcastPort);
}

void Lemma::setupDiscoveryServer(uint16_t port) {
    mUDPServer = UdpServer::create(ci::app::App::get()->io_service());
    mUDPServer->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
        cinder::app::console() << "ERROR - UDP server - " << err << std::endl;
    });
    mUDPServer->connectAcceptEventHandler([&](UdpSessionRef session) {
        mUDPServerSession = session;
        mUDPServerSession->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
            cinder::app::console() << "ERROR - UDP server session - " << err << std::endl;
        });
        mUDPServerSession->connectReadEventHandler2([&](ci::Buffer buffer, boost::asio::ip::udp::endpoint endpoint) {
            std::string response = UdpSession::bufferToString(buffer);
            cinder::app::console() << "NOTICE - host server response - " << response << "\" from " << endpoint << std::endl;
            JsonTree data = JsonTree(response);
            if (data.getNumChildren() != 3) {
                cinder::app::console() << "ERROR - host server response has an invalid number of children - " << data.getNumChildren() << std::endl;
            } else {
                std::string header = data.getValueAtIndex<std::string>(0);
                if (header != sAvailabilityBroadcastResponseHeader) {
                    cinder::app::console() << "ERROR - host server response has an unknown header - " << header << std::endl;
                } else {
                    std::string room = data.getValueAtIndex<std::string>(1);
                    uint16_t port = data.getValueAtIndex<uint16_t>(2);
                    setupMessagingClient(endpoint.address().to_string(), port);
                }
            }
        });

        mUDPServerSession->read();
    });

    // listen on client send port
    mUDPServer->setReuseAddress(true);
    mUDPServer->accept(port);
    cinder::app::console() << "NOTICE - UDP server listening on port " << port << std::endl;
}

void Lemma::sendAvailabilityBroadcast() {
    if (mConnected) {
        return;
    }

    JsonTree rootArray = JsonTree::makeArray();
    rootArray.pushBack(JsonTree("", sAvailabilityBroadcastHeader));
    rootArray.pushBack(JsonTree("", mGuestName));
    rootArray.pushBack(JsonTree("", mRoomName));
    rootArray.pushBack(JsonTree("", sLemmaDialect));
    rootArray.pushBack(JsonTree("", sLemmaVersion));

    std::string jsonString = rootArray.serialize();
    Buffer buffer = UdpSession::stringToBuffer(jsonString);
    mUDPClientSession->write(buffer);

    mAvailabilityBroadcastTimer->wait(sAvailabilityBroadcastInterval, false);
}

#pragma mark - REGISTRATION AND MESSAGING

void Lemma::setupMessagingClient(const std::string& host, uint16_t port) {
    mHeartbeatTimer = WaitTimer::create(ci::app::App::get()->io_service());
    mHeartbeatTimer->connectErrorEventHandler([](std::string message, size_t arg) {
        cinder::app::console() << "ERROR - heartbeat timer - " << message << " " << arg << std::endl;
    });
    mHeartbeatTimer->connectWaitEventHandler([&]() {
        sendHeartbeatMessage();
    });

    mTCPClient = TcpClient::create(ci::app::App::get()->io_service());
    mTCPClient->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
        cinder::app::console() << "ERROR - TCP client - " << err << std::endl;
    });
    mTCPClient->connectConnectEventHandler([&](TcpSessionRef session) {
        mTCPClientSession = session;
        mTCPClientSession->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
            cinder::app::console() << "ERROR - TCP client session - " << err << std::endl;
        });
        mTCPClientSession->connectCloseEventHandler([]() {
            cinder::app::console() << "NOTICE - TCP client session closed" << std::endl;
        });
        mTCPClientSession->connectWriteEventHandler([](size_t bytesTransferred) {
            cinder::app::console() << "NOTICE - TCP client session wrote " << bytesTransferred << " bytes" << std::endl;
        });

        // NB - apparently do not need to enable address reuse

        unsigned short localPort = mTCPClientSession->getSocket()->local_endpoint().port();
        cinder::app::console() << "NOTICE - TCP client session sending from port " << localPort << std::endl;

        setupMessagingServer(localPort);

        // TODO - wait until server is setup?
        mConnected = true;
        sendRegistrationMessage();
        sendHeartbeatMessage();
    });
    mTCPClient->connectResolveEventHandler([]() {
        cinder::app::console() << "NOTICE - TCP client endpoint resolved" << std::endl;
    });

    mTCPClient->connect(host, port);
}

void Lemma::setupMessagingServer(uint16_t port) {
    mTCPServer = TcpServer::create(ci::app::App::get()->io_service());
    mTCPServer->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
        cinder::app::console() << "ERROR - TCP server - " << err << std::endl;
    });
    mTCPServer->connectCancelEventHandler([]() {
        cinder::app::console() << "NOTICE - TCP server canceled" << std::endl;
    });
    mTCPServer->connectAcceptEventHandler([&](TcpSessionRef session) {
        mTCPServerSession = session;
        mTCPServerSession->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
            cinder::app::console() << "ERROR - TCP server session - " << err << std::endl;
        });
        mTCPServerSession->connectCloseEventHandler([]() {
            cinder::app::console() << "NOTICE - TCP server session closed" << std::endl;
        });
        mTCPServerSession->connectReadEventHandler([&](ci::Buffer buffer) {
            // NB - multiple messages can be received at once. Could occur on OS X Mavericks+ via Timer Coalescing
            //  see Cinder-AppNap https://github.com/pizthewiz/Cinder-AppNap to tweak Timer Coalescing behavior

            size_t offset = 0;
            bool status = true;
            while (status) {
                // message length stored in the first 6 bytes
                std::string messageLengthString = std::string(static_cast<const char*>((char*)buffer.getData() + offset), 6);
                size_t messageLength = fromString<size_t>(messageLengthString);
                offset += 6;

                // verify length
                if (messageLength > buffer.getDataSize() - offset) {
                    cinder::app::console() << "ERROR - message length " << messageLength << " larger than remaining buffer data length " << (buffer.getDataSize() - offset) << std::endl;
                    status = false;
                } else {
                    // parse and process message
                    std::string messageString = std::string(static_cast<const char*>((char*)buffer.getData() + offset), messageLength);
                    JsonTree message = JsonTree(messageString);
                    std::string header = message[0].getValue<std::string>();
                    if (header != sEventMessageHeader && header != sHeartbeatAckHeader) {
                        cinder::app::console() << "ERROR - bad event message header \"" << header << "\"" << std::endl;
                    } else if (header == sHeartbeatAckHeader) {
                        // heartbeat ack, ignore
                    } else if (header != sEventMessageHeader) {
                        cinder::app::console() << "ERROR - bad event message header \"" << header << "\"" << std::endl;
                    } else {
                        std::string guestName = message[1].getValue<std::string>();
                        std::string eventName = message[2].getValue<std::string>();
                        std::string eventValue = message[3].getValue<std::string>();

                        // dispatch message
                        if (mMessageEventHandlerMap.count(eventName)) {
                            std::function<void(const std::string&, const std::string&)> eventHandler = mMessageEventHandlerMap[eventName];
                            eventHandler(eventName, eventValue);
                        }
                    }
                    
                    offset += messageLength;
                    if (offset + 6 >= buffer.getDataSize()) {
                        status = false;
                    }
                }
            }

            mTCPServerSession->read();
        });
        mTCPServerSession->connectReadCompleteEventHandler([&]() {
            cinder::app::console() << "NOTICE - TCP server session read complete" << std::endl;

            // occurs when fired by the host server or the host server dies, go back into discovery mode
            end();
            begin();
        });

        mTCPServerSession->read();
    });

    // listen on client send port
    mTCPServer->accept(port);
    cinder::app::console() << "NOTICE - TCP server listening on port " << port << std::endl;
}

void Lemma::sendRegistrationMessage() {
    JsonTree rootArray = JsonTree::makeArray();
    rootArray.pushBack(JsonTree("", sRegistrationMessageHeader));
    rootArray.pushBack(JsonTree("", mGuestName));
    unsigned short port = mTCPClientSession->getSocket()->local_endpoint().port();
    rootArray.pushBack(JsonTree("", port));
    JsonTree hearsArray = JsonTree::makeArray();
    for (const auto& kv : mMessageEventHandlerMap) {
        hearsArray.pushBack(JsonTree("", kv.first));
    }
    rootArray.pushBack(hearsArray);
    JsonTree speaksArray = JsonTree::makeArray();
    rootArray.pushBack(speaksArray);
    rootArray.pushBack(JsonTree("", sLemmaDialect));
    rootArray.pushBack(JsonTree("", sLemmaVersion));
    JsonTree optionsObject = JsonTree::makeObject();
    optionsObject.pushBack(JsonTree("heartbeat", (int)sHeartbeatInterval / 1000));
//    optionsObject.pushBack(JsonTree("heartbeat_ack", true));
    rootArray.pushBack(optionsObject);
    sendJSON(rootArray);
}

void Lemma::sendHeartbeatMessage() {
    if (!mConnected) {
        return;
    }

    JsonTree rootArray = JsonTree::makeArray();
    rootArray.pushBack(JsonTree("", sHeartbeatHeader));
    rootArray.pushBack(JsonTree("", mGuestName));
    sendJSON(rootArray);

    mHeartbeatTimer->wait(sHeartbeatInterval, false);
}

template<typename T>
void Lemma::sendEventMessage(const std::string& eventName, T eventValue) {
    if (!mConnected) {
        return;
    }

    JsonTree rootArray = JsonTree::makeArray();
    rootArray.pushBack(JsonTree("", sEventMessageHeader));
    rootArray.pushBack(JsonTree("", mGuestName));
    rootArray.pushBack(JsonTree("", eventName));
    rootArray.pushBack(JsonTree("", eventValue));
    sendJSON(rootArray);
}

void Lemma::sendEventMessage(const std::string& eventName, const JsonTree& eventValue) {
    // NB - duplicated from above without wrapping eventValue in a JsonTree
    //  couldn't sort out how to RTT it with a single method
    if (!mConnected) {
        return;
    }

    JsonTree rootArray = JsonTree::makeArray();
    rootArray.pushBack(JsonTree("", sEventMessageHeader));
    rootArray.pushBack(JsonTree("", mGuestName));
    rootArray.pushBack(JsonTree("", eventName));
    rootArray.pushBack(eventValue);
    sendJSON(rootArray);
}

void Lemma::sendJSON(const JsonTree& root) {
    std::string jsonString = root.serialize();
    Buffer jsonBuffer = TcpSession::stringToBuffer(jsonString);
    std::string dataString = str(boost::format("%06d") % jsonBuffer.getDataSize()) + jsonString;
    Buffer dataBuffer = TcpSession::stringToBuffer(dataString);
    mTCPClientSession->write(dataBuffer);
}

}}
