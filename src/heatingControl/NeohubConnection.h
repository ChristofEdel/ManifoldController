#ifndef __NEOHUB_CONNECTION_H
#define __NEOHUB_CONNECTION_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include "MyMutex.h"
#include <unordered_set>

class NeohubConnection {
    
private:
    String m_host;
    WebSocketsClient m_websocketClient;
    static void webSocketEventHandler(WStype_t type, uint8_t * payload, size_t length, void *clientData);
    std::function<void()> m_on_connect = nullptr;
    std::function<void(String)> m_on_receive = nullptr;
    std::function<void(String errorText)> m_on_error = nullptr;
    std::function<void()> m_on_disconnect = nullptr;

    String m_responseText;
    bool m_deleted = false;

public:
    NeohubConnection(const String &hubHost);
private:
    ~NeohubConnection();        // Private so nobody can delete an object from a callback
                                // use NeohubConnection::remove() instead which will
                                // delete the object in the next loop

public:
    bool open(int timeoutMillis = -1);
    bool send (String json, int timeoutMillis = -1);
    bool close(int timeoutMillis = -1);
    void finish() { m_deleted = true; };  // delayed deletion - will be deleted in next loop

    // Set callback for when after the connection was established
    void onConnect(std::function<void()> func) { m_on_connect = func; };

    // Set call back for when data is received
    void onReceive(std::function<void(String json)> func) { m_on_receive = func; }
    void onError(std::function<void(String errorText)> func) { m_on_error = func; }

    // Set callback on disconnection
    // also called if establishing the connection in open() fails
    void onDisconnect(std::function<void()> func) { m_on_disconnect = func; }

    // check it this connection is (still) connected
    bool isConnected();

private:
    // loop function for the connection - must be called regularlu
    // originally intended for running in the main loop, but we
    // use a loopTask for this
    void loop() { 
        Serial.println("LOOP");
        m_websocketClient.loop(); 
        Serial.println("DONE");
    };

    // The loop task which regularly executes the loop function for all connections
    static TaskHandle_t m_loopTaskHandle;
    static void ensureLoopTask();
    static void loopTask(void *parameter);

    // a set of all live connections for which loop() has to be called
    static std::unordered_set<NeohubConnection *> m_liveConnections;

    // A mutex preventinb concurrent access to the set with the live connections
    static MyMutex m_loopMutex;

    void addToLoopTask();


};



#endif
