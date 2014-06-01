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
#include "TcpServer.h"
#include "cinder/Json.h"

namespace Cinder { namespace Noam {

using namespace ci;

typedef std::shared_ptr<class Lemma> LemmaRef;

class Lemma : public std::enable_shared_from_this<Lemma> {
public:
    static LemmaRef create(const std::string& guestName, const std::string& roomName = "");
    ~Lemma();

    template<typename T, typename Y>
    inline void connectMessageEventHandler(const std::string& eventName, T eventHandler, Y* eventHandlerObject) {
        connectMessageEventHandler(eventName, std::bind(eventHandler, eventHandlerObject, std::placeholders::_2));
    }
    void connectMessageEventHandler(const std::string& eventName, const std::function<void(const std::string&, const std::string&)>& eventHandler);

    void sendMessage(const std::string& eventName, bool eventValue);
    void sendMessage(const std::string& eventName, double eventValue);
    void sendMessage(const std::string& eventName, float eventValue);
    void sendMessage(const std::string& eventName, int eventValue);
    void sendMessage(const std::string& eventName, const std::string& eventValue);
    void sendMessage(const std::string& eventName, const char* eventValue);
    void sendMessage(const std::string& eventName, const JsonTree& eventValue);

    void begin();

    inline bool isConnected() const { return mConnected; }

private:
    Lemma(const std::string& guestName, const std::string& roomName);

    void setupDiscoveryClient();
    void setupDiscoveryServer(uint16_t port);
    void sendAvailabilityBroadcast();

    void setupMessagingClient(const std::string& host, uint16_t port);
    void setupMessagingServer(uint16_t port);
    void sendRegistrationMessage();
    void sendHeartbeatMessage();
    template<typename T>
    void sendEventMessage(const std::string& eventName, T eventValue);
    void sendEventMessage(const std::string& eventName, const JsonTree& eventValue);
    void sendJSON(const JsonTree& root);

    bool mConnected;
    std::string mGuestName;
    std::string mRoomName;

    std::map<std::string, std::function<void(const std::string&, const std::string&)>> mMessageEventHandlerMap;

    // discovery
    WaitTimerRef mAvailabilityBroadcastTimer;
    UdpClientRef mUDPClient;
    UdpSessionRef mUDPClientSession;
    UdpServerRef mUDPServer;

    // registration and messaging
    WaitTimerRef mHeartbeatTimer;
    TcpClientRef mTCPClient;
    TcpSessionRef mTCPClientSession;
    TcpServerRef mTCPServer;
    TcpSessionRef mTCPServerSession;
};

}}
