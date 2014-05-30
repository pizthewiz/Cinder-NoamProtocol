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

    bool mConnected;
    std::string mGuestName;
    std::string mRoomName;

    // discovery
    WaitTimerRef mAvailabilityBroadcastTimer;
    UdpClientRef mUDPClient;
    UdpSessionRef mUDPClientSession;
    UdpServerRef mUDPServer;
};

}}
