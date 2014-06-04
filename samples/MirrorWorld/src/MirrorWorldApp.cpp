//
//  MirrorWorldApp.cpp
//  MirrorWorld
//
//  Created by Jean-Pierre Mouilleseaux on 03 Jun 2014.
//  Copyright 2014 Chorded Constructions. All rights reserved.
//

#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Utilities.h"
#include "Cinder-NoamProtocol.h"
#include "cinder/Json.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace Cinder::Noam;

class MirrorWorldApp : public AppNative {
public:
    void prepareSettings(Settings* settings);
    void setup();
    void mouseDown(MouseEvent event);
    void update();
    void draw();

private:
    LemmaRef mLemmaNorth;
    Vec2f mPointNorth;
    Color mColorNorth;

    LemmaRef mLemmaSouth;
    Vec2f mPointSouth;
    Color mColorSouth;
};

void MirrorWorldApp::prepareSettings(Settings* settings) {
    settings->enableHighDensityDisplay();
    settings->setWindowSize(640, 480);
    settings->prepareWindow(Window::Format().fullScreenButton());
}

void MirrorWorldApp::setup() {
    mLemmaNorth = Lemma::create("North");
    mLemmaNorth->connectMessageEventHandler("southPoint", [&](const std::string& eventName, const std::string& eventValue) {
        JsonTree object = JsonTree(eventValue);
        mPointNorth = Vec2f(object.getValueForKey<float>("x"), object.getValueForKey<float>("y"));
        mColorNorth = Color(0, 1, 1);
    });
    mLemmaNorth->begin();
    mPointNorth = Vec2f(-50.0f, -50.0f);

    mLemmaSouth = Lemma::create("South");
    mLemmaSouth->connectMessageEventHandler("northPoint", [&](const std::string& eventName, const std::string& eventValue) {
        JsonTree object = JsonTree(eventValue);
        mPointSouth = Vec2f(object.getValueForKey<float>("x"), object.getValueForKey<float>("y"));
        mColorSouth = Color(1, 0, 1);
    });
    mLemmaSouth->begin();
    mPointSouth = Vec2f(-50.0f, -50.0f);
}

void MirrorWorldApp::mouseDown(MouseEvent event) {
    if (event.getY() <= getWindowHeight() / 2.0f) {
        mPointNorth = Vec2f(event.getX(), event.getY());
        mColorNorth = Color(1, 0, 0);

        if (!mLemmaNorth->isConnected()) {
            return;
        }
        JsonTree object = JsonTree::makeObject();
        object.addChild(JsonTree("x", mPointNorth.x));
        object.addChild(JsonTree("y", mPointNorth.y));
        mLemmaNorth->sendMessage("northPoint", object);
    } else {
        mPointSouth = Vec2f(event.getX(), event.getY() - getWindowHeight() / 2.0f);
        mColorSouth = Color(0, 0, 1);

        if (!mLemmaSouth->isConnected()) {
            return;
        }
        JsonTree object = JsonTree::makeObject();
        object.addChild(JsonTree("x", mPointSouth.x));
        object.addChild(JsonTree("y", mPointSouth.y));
        mLemmaSouth->sendMessage("southPoint", object);
    }
}

void MirrorWorldApp::update() {
}

void MirrorWorldApp::draw() {
	gl::clear(Color::black());

    gl::color(mColorNorth);
    gl::drawSolidCircle(mPointNorth, 10.0f * getWindowContentScale());

    gl::color(mColorSouth);
    gl::drawSolidCircle(mPointSouth + Vec2f(0.0f, getWindowHeight() / 2.0f), 10.0f * getWindowContentScale());

    // draw equator
    gl::color(Color::white());
    gl::lineWidth(2.0f * getWindowContentScale());
    gl::drawLine(Vec2f(0.0f, getWindowHeight() / 2.0f), Vec2f(getWindowWidth(), getWindowHeight() / 2.0f));
}

CINDER_APP_NATIVE(MirrorWorldApp, RendererGl)
