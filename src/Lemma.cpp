//
//  Lemma.cpp
//  SmokeSignals
//
//  Created by Jean-Pierre Mouilleseaux on 22 May 2014.
//  Copyright 2014 Chorded Constructions. All rights reserved.
//

#include "Lemma.h"
#include "cinder/app/App.h"
#include "cinder/Json.h"

namespace Cinder { namespace Noam {

using namespace ci;

static const std::string sLemmaDialect = "Cinder-NoamLemma";
static const std::string sLemmaVersion = "0.0.0";

static const std::string sAvailabilityBroadcastHost = "255.255.255.255";
static const uint16_t sAvailabilityBroadcastPort = 1030;
static const size_t sAvailabilityBroadcastInterval = 1 * 1000;
static const std::string sAvailabilityBroadcastHeader = "marco";
static const std::string sAvailabilityBroadcastResponseHeader = "polo";

static const std::string sRegistrationMessageHeader = "register";

LemmaRef Lemma::create(const std::string& guestName, const std::string& roomName) {
    return LemmaRef(new Lemma(guestName, roomName))->shared_from_this();
}

Lemma::Lemma(const std::string& guestName, const std::string& roomName) : mConnected(false), mGuestName(guestName), mRoomName(roomName) {
}

Lemma::~Lemma() {
    mAvailabilityBroadcastTimer = nullptr;
    mUDPClient = nullptr;
    mUDPClientSession = nullptr;
    mUDPServer = nullptr;
    mTCPClient = nullptr;
    mTCPClientSession = nullptr;
}

#pragma mark -

void Lemma::begin() {
    // TODO - handle begin while isConnected() == true
    setupDiscoveryClient();
}

#pragma mark - DISCOVERY

void Lemma::setupDiscoveryClient() {
    mAvailabilityBroadcastTimer = WaitTimer::create(ci::app::App::get()->io_service());
    mAvailabilityBroadcastTimer->connectErrorEventHandler([&](std::string message, size_t arg) {
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

        unsigned short port = mUDPClientSession->getSocket()->local_endpoint().port();
        cinder::app::console() << "NOTICE - UDP client session sending from port " << port << std::endl;

        setupDiscoveryServer(static_cast<uint16_t>(port));

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
        session->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
            cinder::app::console() << "ERROR - UDP server session - " << err << std::endl;
        });
        session->connectReadEventHandler2([&](ci::Buffer buffer, boost::asio::ip::udp::endpoint endpoint) {
            std::string response = UdpSession::bufferToString(buffer);
            cinder::app::console() << "NOTICE - host response - " << response << "\" from " << endpoint << std::endl;
            JsonTree data = JsonTree(response);
            if (data.getNumChildren() != 3) {
                cinder::app::console() << "ERROR - host response has an invalid number of children - " << data.getNumChildren() << std::endl;
            } else {
                std::string header = data.getValueAtIndex<std::string>(0);
                if (header != sAvailabilityBroadcastResponseHeader) {
                    cinder::app::console() << "ERROR - host response has an unknown header - " << header << std::endl;
                } else {
//                    mRoomName = data.getValueAtIndex<std::string>(1);
                    uint16_t port = data.getValueAtIndex<uint16_t>(2);
                    setupMessagingClient(endpoint.address().to_string(), port);
                }
            }
        });

        session->read();
    });

    // listen on client's send port
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
    mTCPClient = TcpClient::create(ci::app::App::get()->io_service());
    mTCPClient->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
        cinder::app::console() << "ERROR - TCP client - " << err << std::endl;
    });
    mTCPClient->connectConnectEventHandler([&](TcpSessionRef session) {
        mTCPClientSession = session;
        mTCPClientSession->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
            cinder::app::console() << "ERROR - TCP client session - " << err << std::endl;
        });
        mTCPClientSession->connectCloseEventHandler([&]() {
            cinder::app::console() << "NOTICE - TCP client session closed" << std::endl;
            mConnected = false;
            // TODO - availabilty broadcast again?
        });
        mTCPClientSession->connectReadEventHandler([&](ci::Buffer buffer) {
            // TODO - append data
            mTCPClientSession->read();
        });
        mTCPClientSession->connectReadCompleteEventHandler([&]() {
            // TODO - process data
            mTCPClientSession->read();
        });
        mTCPClientSession->connectWriteEventHandler([&](size_t bytesTransferred) {
            cinder::app::console() << "NOTICE - TCP client session wrote " << bytesTransferred << " bytes" << std::endl;
        });

        mConnected = true;
        sendRegistration();
    });
    mTCPClient->connectResolveEventHandler([]() {
        cinder::app::console() << "NOTICE - TCP client endpoint resolved" << std::endl;
    });

    mTCPClient->connect(host, port);
}

void Lemma::sendRegistration() {
    JsonTree rootArray = JsonTree::makeArray();
    rootArray.pushBack(JsonTree("", sRegistrationMessageHeader));
    rootArray.pushBack(JsonTree("", mGuestName));
    unsigned short port = mTCPClientSession->getSocket()->local_endpoint().port();
    rootArray.pushBack(JsonTree("", port));
    JsonTree hearsArray = JsonTree::makeArray();
    for (const auto& kv : mMessageHandlerMap) {
        hearsArray.pushBack(JsonTree("", kv.first));
    }
    rootArray.pushBack(hearsArray);
    JsonTree speaksArray = JsonTree::makeArray();
    rootArray.pushBack(speaksArray);
    rootArray.pushBack(JsonTree("", sLemmaDialect));
    rootArray.pushBack(JsonTree("", sLemmaVersion));
    // NB - optional
//    JsonTree optionsObject = JsonTree::makeObject();
//    optionsObject.pushBack(JsonTree("heartbeat", 20.0));
//    optionsObject.pushBack(JsonTree("heartbeat_ack", true));
//    rootArray.pushBack(optionsObject);
    std::string jsonString = rootArray.serialize();
    Buffer jsonBuffer = UdpSession::stringToBuffer(jsonString);

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(6) << jsonBuffer.getDataSize();
    std::string data = ss.str() + jsonString;
    Buffer buffer = UdpSession::stringToBuffer(data);

    mTCPClientSession->write(buffer);
}

}}
