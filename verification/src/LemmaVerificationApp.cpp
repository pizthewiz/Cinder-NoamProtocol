
//
//  LemmaVerificationApp.cpp
//  LemmaVerification
//
//  Created by Jean-Pierre Mouilleseaux on 11 Jun 2014.
//  Copyright 2014 Chorded Constructions. All rights reserved.
//

#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Utilities.h"
#include "Cinder-NoamProtocol.h"
#include "cinder/Json.h"
#include "boost/format.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace Cinder::Noam;

class LemmaVerificationApp : public AppNative {
public:
    void setup();
    void update();
    void draw();

private:
    LemmaRef mLemma;
};

void LemmaVerificationApp::setup() {
    mLemma = Lemma::create("cinder-noamprotocol_verification", "lemma_verification");

    // https://github.com/noam-io/host/blob/master/lemma_verification/tests/echo.rb
    mLemma->connectMessageEventHandler("Echo", [&](const std::string& eventName, const std::string& eventValue) {
        mLemma->sendMessage("EchoVerify", eventValue);
    });

    // https://github.com/noam-io/host/blob/master/lemma_verification/tests/plus_one.rb
    mLemma->connectMessageEventHandler("PlusOne", [&](const std::string& eventName, const std::string& eventValue) {
        // NB - integer inputs
        int value = fromString<int>(eventValue) + 1;
        mLemma->sendMessage("PlusOneVerify", value);
    });

    // https://github.com/noam-io/host/blob/master/lemma_verification/tests/sum.rb
    mLemma->connectMessageEventHandler("Sum", [&](const std::string& eventName, const std::string& eventValue) {
        // NB - integer inputs
        int value = 0;
        JsonTree array = JsonTree(eventValue);
        for (JsonTree::ConstIter it = array.begin(); it != array.end(); ++it) {
            value += (*it).getValue<int>();
        }
        mLemma->sendMessage("SumVerify", value);
    });

    // https://github.com/noam-io/host/blob/master/lemma_verification/tests/name.rb
    mLemma->connectMessageEventHandler("Name", [&](const std::string& eventName, const std::string& eventValue) {
        JsonTree object = JsonTree(eventValue);
        string name = str(boost::format("%1% %2%") % object.getValueForKey("firstName") % object.getValueForKey("lastName"));
        JsonTree value = JsonTree::makeObject();
        value.addChild(JsonTree("fullName", name));
        mLemma->sendMessage("NameVerify", value);
    });

    mLemma->begin();
}

void LemmaVerificationApp::update() {
}

void LemmaVerificationApp::draw() {
    gl::clear();
}

CINDER_APP_NATIVE(LemmaVerificationApp, RendererGl)
