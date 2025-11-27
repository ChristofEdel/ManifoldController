#ifndef __NEOHUB_CONNECTION_H
#define __NEOHUB_CONNECTION_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include "MyMutex.h"
#include <unordered_set>
#include <deque>

class NeohubConnection {

private:

    // Internal class for a single message - response interaction
    class Conversation {
    public:
        String m_message;
        unsigned long m_startMillis;
        int m_timeoutMillis;
        std::function<void(String json)> m_onReceive;
        std::function<void(String message)> m_onError;
        bool m_started = false;

        Conversation(
            const String &message,
            std::function<void(String json)> onReceive,
            std::function<void(String message)> onError,
            int timeoutMillis
        ) : m_message(message), m_startMillis(millis()), m_timeoutMillis(timeoutMillis), m_onReceive(onReceive), m_onError(onError)
        {};
        
        bool timeout() {
            return (millis() - m_startMillis) > m_timeoutMillis;
        }
    };

    
private:
    String m_host;
    WebSocketsClient m_websocketClient;
    static void webSocketEventHandler(WStype_t type, uint8_t * payload, size_t length, void *clientData);

    std::function<void()> m_on_connect = nullptr;
    std::function<void()> m_on_disconnect = nullptr;
    std::function<void(String message)> m_on_error = nullptr;

    bool m_deleted = false;

    std::deque<Conversation*> m_conversations;
    

public:
    NeohubConnection(const String &hubHost);
private:
    ~NeohubConnection();        // Private so nobody can delete an object from a callback
                                // use NeohubConnection::remove() instead which will
                                // delete the object in the next loop

public:
    // Connection lifecycle
    bool open(int timeoutMillis = -1);
    bool close(int timeoutMillis = -1);
    void finish() { m_deleted = true; };  // delayed deletion - will be deleted in next loop
    bool isConnected();

    // Callbacks
    // Set callback for when after the connection was established
    void onConnect(std::function<void()> func) { m_on_connect = func; };

    // Set callback on disconnection
    // also called if establishing the connection in open() fails
    void onDisconnect(std::function<void()> func) { m_on_disconnect = func; };

    // Set callback for any errors. Called in addition to any Conversation error callback
    void onError(std::function<void(String errorText)> func) { m_on_error = func; };

    // Conversations
    bool send (
        String json, 
        std::function<void(String json)> onReceive,
        std::function<void(String message)> onError,
        int timeoutMillis = 1000
    );

private:

    // loop function for the connection - must be called regularlu
    // originally intended for running in the main loop, but we
    // use a loopTask for this
    void loop();
    bool startConversation(Conversation *c);

    // The loop task which regularly executes the loop function for all connections
    static TaskHandle_t m_loopTaskHandle;
    static void ensureLoopTask();
    static void loopTask(void *parameter);

    // a set of all live connections for which loop() has to be called
    static std::unordered_set<NeohubConnection *> m_liveConnections;

    // A mutex preventing concurrent access to the set with the live connections
    static MyMutex m_loopMutex;

    void addToLoopTask();


};

#endif
