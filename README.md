# Cinder-NoamProtocol
`Cinder-NoamProtocol` is an implementation of [IDEO](http://www.ideo.com)'s [Noam protocol](http://noam.io/) which deals in endpoint discovery and messaging.

### REQUIREMENTS
`Cinder-NoamProtocol` builds on top of [Stephen Schieberl](http://www.bantherewind.com)'s great [`Cinder-Asio`](https://github.com/BanTheRewind/Cinder-Asio) CinderBlock and more specifically, currently requires the [noam-dev](https://github.com/pizthewiz/Cinder-Asio/tree/noam-dev) branch on my own fork until some UDP server issues are sorted out.

### USAGE
```C++
void SomeApp::setup() {
    mLemma = Lemma::create("buzzard");
    mLemma->connectMessageEventHandler("vultureKeys", [&](const std::string& eventName, const std::string& eventValue) {
        // eventValue could be a bool, double, float, int, string or JsonTree
        console() << eventName << " - " << fromString<int>(eventValue) << std::endl;
    });
    mLemma->begin();
}

void SomeApp::keyDown(KeyEvent event) {
    mLemma->sendMessage("buzzardKeys", event.getCode());
}
```
