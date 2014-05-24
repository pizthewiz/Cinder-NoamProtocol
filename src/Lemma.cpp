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
static const std::string sMarcoHeader = "marco";
static const std::string sMarcoHost = "255.255.255.255";
static const uint16_t sMarcoPort = 1030;
static const size_t sMarcoBroadcastInterval = 5 * 1000;
static const std::string sPoloHeader = "polo";

LemmaRef Lemma::create(const std::string& guestName, const std::string& roomName) {
    return LemmaRef(new Lemma(guestName, roomName))->shared_from_this();
}

Lemma::Lemma(const std::string& guestName, const std::string& roomName) : mConnected(false), mGuestName(guestName), mRoomName(roomName) {
}

Lemma::~Lemma() {
    mUDPClient = nullptr;
    mTimer = nullptr;
}

#pragma mark -

void Lemma::begin() {
    // TODO - handle overlapping begins
    setupDiscoveryClient();
}

#pragma mark -

void Lemma::setupDiscoveryClient() {
    mTimer = WaitTimer::create(ci::app::App::get()->io_service());
    mTimer->connectWaitEventHandler([&]() {
        sendMarco();
    });
    mTimer->connectErrorEventHandler([&](std::string message, size_t arg) {
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
            cinder::app::console() << "NOTICE - UDP client session wrote " << bytesTransferred << "B" << std::endl;
        });

        // enable broadcast
        boost::asio::socket_base::broadcast broadcastOption(true);
        mUDPClientSession->getSocket()->set_option(broadcastOption);

        // enable address reuse
        boost::asio::socket_base::reuse_address addressOption(true);
        boost::system::error_code errorCode;
        mUDPClientSession->getSocket()->set_option(addressOption, errorCode);
        if (errorCode) {
            cinder::app::console() << "NOTICE - UDP client session failed to enable address reuse " << errorCode << std::endl;
        }

        unsigned short port = mUDPClientSession->getSocket()->local_endpoint().port();
        cinder::app::console() << "NOTICE - UDP client session sending from port " << port << std::endl;
        setupDiscoveryServer();

        // TODO - wait until server is setup?
        sendMarco();
        mTimer->wait(sMarcoBroadcastInterval, true);
    });
    mUDPClient->connectResolveEventHandler([]() {
        cinder::app::console() << "NOTICE - UDP client end point resolved" << std::endl;
    });

    mUDPClient->connect(sMarcoHost, sMarcoPort);
}

void Lemma::setupDiscoveryServer() {
    mUDPServer = UdpServer::create(ci::app::App::get()->io_service());
    mUDPServer->connectAcceptEventHandler([&](UdpSessionRef session) {
        mUDPServerSession = session;
        mUDPServerSession->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
            cinder::app::console() << "ERROR - UDP server session - " << err << std::endl;
        });
        mUDPServerSession->connectReadCompleteEventHandler([&]() {
            mUDPServerSession->read();
        });
        mUDPServerSession->connectReadEventHandler([&](ci::Buffer buffer) {
            std::string response = UdpSession::bufferToString(buffer);
            cinder::app::console() << "NOTICE - Noam host response - " << response << std::endl;
            JsonTree data = JsonTree(response);
            if (data.getNumChildren() != 3) {
                cinder::app::console() << "ERROR - Noam host response has an invalid number of children - " << data.getNumChildren() << std::endl;
            } else {
                std::string header = data.getValueAtIndex<std::string>(0);
                if (header != sPoloHeader) {
                    cinder::app::console() << "ERROR - Noam host response has an unknown header - " << header << std::endl;
                } else {
                    std::string roomName = data.getValueAtIndex<std::string>(1);
                    uint16_t tcpPort = data.getValueAtIndex<uint16_t>(2);
                    // TODO - do stuff!
                }
            }

            mUDPServerSession->read();
        });
//        mUDPServerSession->connectWriteEventHandler();

        unsigned short port = mUDPServerSession->getSocket()->local_endpoint().port();
        cinder::app::console() << "NOTICE - UDP server session listening on port " << port << std::endl;

        mUDPServerSession->read();
    });
    mUDPServer->connectErrorEventHandler([](std::string err, size_t bytesTransferred) {
        cinder::app::console() << "ERROR - UDP server - " << err << std::endl;
    });

    unsigned short port = mUDPClientSession->getSocket()->local_endpoint().port();
    mUDPServer->accept(static_cast<uint16_t>(port));
}

void Lemma::sendMarco() {
    JsonTree root = JsonTree::makeArray();
    root.pushBack(JsonTree("", sMarcoHeader));
    root.pushBack(JsonTree("", mGuestName));
    root.pushBack(JsonTree("", mRoomName));
    root.pushBack(JsonTree("", sLemmaDialect));
    root.pushBack(JsonTree("", sLemmaVersion));
    std::string jsonString = root.serialize();
    Buffer buffer = UdpSession::stringToBuffer(jsonString);
    mUDPClientSession->write(buffer);
}

}}
