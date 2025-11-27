#include "NeohubCOnnection.h"


NeohubConnection::NeohubConnection(const String &hubHost) {
    this->m_host = hubHost;
}
NeohubConnection::~NeohubConnection() {
}

bool NeohubConnection::open(int timeoutMillis /* = -1 */) {
    this->m_websocketClient.beginSSL(this->m_host, 4243, "/");
    this->m_websocketClient.onEvent(NeohubConnection::webSocketEventHandler, this);
    this->addToLoopTask();
    ensureLoopTask();
    return true;
}

bool NeohubConnection::close(int timeoutMillis /* = -1 */) {
    this->m_websocketClient.disconnect();
    return true;
}

bool NeohubConnection::isConnected() {
    return this->m_websocketClient.isConnected();
}

bool NeohubConnection::send (
    String json, 
    std::function<void(String json)> onReceive,
    std::function<void(String message)> onError,
    int timeoutMillis
) {
    Conversation *c = new Conversation(json, onReceive, onError, timeoutMillis);
    bool success = true;

    // Do not do anythinh while processing in the loop is ongoing
    if (m_loopMutex.lock("NeohubConnection::send")) {

        // If there are no live conversations, send immediately
        if (m_conversations.empty()) {
            c->m_started = this->m_websocketClient.sendTXT(json);
            // If sending was successful, put into processing queue
            // for processing the return
            if (c->m_started) {
                m_conversations.push_back(c);
            }
            // If sending failed, give up and return false
            else {
                if (c->m_onError) c->m_onError("Send failed");
                delete c;
                success = false;
            }
        }
        // If there are live conversations, jeoin the queue
        else {
            m_conversations.push_back(c);
        }
        m_loopMutex.unlock("NeohubConnection::send");
    }
    return success;
}

// Start a conversation; remove from queue if it cannot be started
bool NeohubConnection::startConversation(NeohubConnection::Conversation *c) {
    // If it is too late to start, we time out
    if (c->timeout()) {
        c->m_onError("Timeout before sending");
        return false;
    };
    c->m_started = this->m_websocketClient.sendTXT(c->m_message);
    // If we failed to send it, we remove it from the queue
    // if there are more waiting, we will deal with it the next time round in the loop
    if (!c->m_started) {
        c->m_onError("Send failed");
        return false;
    }
    else {
        return true;
    }
}

void NeohubConnection::loop() { 
    // NMB - Mutex already acquired in calling function, do not need to lock here
    // Process the first conversation in the queue and start it if required
    Conversation *c = m_conversations.empty() ? nullptr : m_conversations.front();
    if (c && !c->m_started) {
        if (!startConversation(c)) {
            m_conversations.pop_front();
            delete c;
        };
    }

    // normal client processing
    m_websocketClient.loop(); 
    // the loop() above may process conversations and remove them from the queue

    // Now we process the next conversation in the queue
    while ((c = (m_conversations.empty() ? nullptr : m_conversations.front()))) {
        // If it needs to be started, we try to start it.
        // In any case, we stop processing messages from the queue until the next
        // round trip in the loop
        if (!c->m_started) {
            if (!startConversation(c)) {
                m_conversations.pop_front();
                delete c;
            }
            break;
        }

        // if it is waiting for a response, we check for timeout.
        // if it has not timed out, we wait for it to complete and process no more 
        // conversations until then
        if (!c->timeout()) break;

        // If it has timed out, we finish it with an error and process the next
        // conversation in the queue
        c->m_onError("Timeout waiting for response");
        m_conversations.pop_front();
        delete c;
    }
}


void NeohubConnection::webSocketEventHandler(WStype_t type, uint8_t * payload, size_t length, void *clientData) {
    NeohubConnection* _this = (NeohubConnection *) clientData;
    switch (type) {
        case WStype_CONNECTED: {
            if (_this->m_on_connect) _this->m_on_connect();
            break;
        }
        case WStype_DISCONNECTED: {
            if (_this->m_on_disconnect) _this->m_on_disconnect();
            break;
        }
        case WStype_TEXT: {
            // Send the received data to the conversation's hanlder and then remove 
            // the conversation from the processing queue
            Conversation *c = _this->m_conversations.empty() ? nullptr : _this->m_conversations.front();
            if (c) {
                if (!c->m_started) {
                    // catastrophic failure! need to clear queue and reconnect
                    abort();
                }
                if (c->m_onReceive) c->m_onReceive(String(payload, length));
                _this->m_conversations.pop_front();
                delete c;
            }
            else {
                if (_this->m_on_error)  _this->m_on_error(
                    String("Message received outside a conversation: ") 
                    + String(payload, min((int)length, 30))
                );
            }
            break;
        }
        break;

        case WStype_ERROR: {
            if (_this->m_on_error) _this->m_on_error("Error event");
            break;
        }

        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN: {
            if (_this->m_on_error) _this->m_on_error("Fragment received - unhandled");
            break;
        }

        // Binary data messages - we don't need them so we don't handle them
        case WStype_BIN:
        case WStype_FRAGMENT_BIN_START:
        if (_this->m_on_error) _this->m_on_error("Binary data received - unhandled");
            break;

        // Messages we don't care for
        case WStype_PING:
        case WStype_PONG:
            break;
    }
}

TaskHandle_t NeohubConnection::m_loopTaskHandle = 0;
std::unordered_set<NeohubConnection *> NeohubConnection::m_liveConnections;
MyMutex NeohubConnection::m_loopMutex;

void NeohubConnection::loopTask(void *parameter) {
    for(;;) {
        if (m_loopMutex.lock("NeohubConnection::loopTask")) {
            for (auto iterator = m_liveConnections.begin(); iterator != m_liveConnections.end(); ) {
                NeohubConnection *connection = *iterator;
                if (connection->m_deleted) {
                    iterator = m_liveConnections.erase(iterator);
                    delete connection;
                }
                else {
                    connection->loop();
                    iterator++;
                }
            }
            m_loopMutex.unlock("NeohubConnection::loopTask");
        }
        delay(10);
    }
}

void NeohubConnection::addToLoopTask() {
    if (m_loopMutex.lock("NeohubConnection::addToLoopTask")) {
        m_liveConnections.insert(this);
        m_loopMutex.unlock("NeohubConnection::addToLoopTask");
    }
}

void NeohubConnection::ensureLoopTask() {
    if (m_loopTaskHandle) return;
    xTaskCreate(
        NeohubConnection::loopTask, // Task function
        "WebSocketLoop",            // Task name
        4096,                       // Stack size (bytes)
        NULL,                       // Parameter to pass
        1,                          // Task priority
        &m_loopTaskHandle           // Task handle
    );
}