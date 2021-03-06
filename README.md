# Cinder-NoamProtocol
`Cinder-NoamProtocol` is an implementation of [IDEO](http://www.ideo.com)'s [Noam protocol](http://noam.io/) which deals in endpoint discovery and messaging.

### USAGE
This simple example creates a guest, listens for and logs *vultureKey* messages and when a key is pressed within the application, sends a message with the key code.
```C++
void SomeApp::setup() {
    mLemma = Lemma::create("buzzard");
    mLemma->connectMessageEventHandler("vultureKeys", [](const std::string& eventName, const std::string& eventValue) {
        // eventValue could contain a bool, double, float, int, stringified JSON or just a string
        console() << eventName << " - " << fromString<int>(eventValue) << std::endl;
    });
    mLemma->begin();
}

void SomeApp::keyDown(KeyEvent event) {
    mLemma->sendMessage("buzzardKeys", event.getCode());
}
```

### REQUIREMENTS
`Cinder-NoamProtocol` builds on top of [Stephen Schieberl](http://www.bantherewind.com)'s great [`Cinder-Asio`](https://github.com/BanTheRewind/Cinder-Asio) CinderBlock and more specifically, currently requires the *dev* branch. The block has only been tested with the [dev](https://github.com/Cinder/Cinder/tree/dev) branch of the [Cinder repository](https://github.com/cinder/Cinder) on OS X Mavericks.
