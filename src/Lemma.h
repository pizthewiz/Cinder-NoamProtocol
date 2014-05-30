//
//  Lemma.h
//  SmokeSignals
//
//  Created by Jean-Pierre Mouilleseaux on 22 May 2014.
//  Copyright 2014 Chorded Constructions. All rights reserved.
//

#pragma once

#include "WaitTimer.h"
#include "UdpClient.h"
#include "UdpServer.h"
#include "TcpClient.h"
#include <boost/any.hpp>

namespace Cinder { namespace Noam {

using namespace ci;

typedef std::shared_ptr<class Lemma> LemmaRef;

class Lemma : public std::enable_shared_from_this<Lemma> {
public:
    static LemmaRef create(const std::string& guestName, const std::string& roomName = "");
    ~Lemma();

    void begin();

    inline bool isConnected() const { return mConnected; }

private:
    Lemma(const std::string& guestName, const std::string& roomName);

    void setupDiscoveryClient();
    void setupDiscoveryServer(uint16_t port);
    void sendAvailabilityBroadcast();

    void setupMessagingClient(const std::string& host, uint16_t port);
    void sendRegistration();

    bool mConnected;
    std::string mGuestName;
    std::string mRoomName;

    typedef std::function<void(std::string, boost::any)> MessageHandler;
    std::map<std::string, MessageHandler> mMessageHandlerMap;

    // discovery
    WaitTimerRef mAvailabilityBroadcastTimer;
    UdpClientRef mUDPClient;
    UdpSessionRef mUDPClientSession;
    UdpServerRef mUDPServer;

    // registration and messaging
    TcpClientRef mTCPClient;
    TcpSessionRef mTCPClientSession;
};

}}
