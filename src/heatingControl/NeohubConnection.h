#ifndef __NEOHUB_CONNECTION_H
#define __NEOHUB_CONNECTION_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include "MyMutex.h"
#include <unordered_set>
#include <deque>

class NeohubConnection {

private:

    // Internal class for a single message/response interaction
    class Conversation {
    public:
        String m_command;                                       // The command sent as part od the conversation
        unsigned long m_startMillis;                            // The time at which the request was made (not sent)
        int m_timeoutMillis;                                    // A timeout measured from m_startMillis
        std::function<void(String responseJson)> m_onReceive;   // called when a response has been received
        std::function<void(String message)> m_onError;          // called when an error occured (which may be a failure to send the command)
        bool m_commandSent = false;                             // A flag indicating if the command was sent, i.e., the conversation is "live"

        // Constructor for a new conversation
        Conversation(
            const String &command,
            std::function<void(String responseJson)> onReceive,
            std::function<void(String message)> onError,
            int timeoutMillis
        ) : m_command(command), m_startMillis(millis()), m_timeoutMillis(timeoutMillis), m_onReceive(onReceive), m_onError(onError)
        {};
        
        // Check if the conversation has timed out as of now
        bool timeoutExceeded() {
            return (millis() - m_startMillis) > m_timeoutMillis;
        }
    };
    
private:
    String m_host;                                              // the Neohub hosy (name or ip address)
    String m_accessToken;                                       // the access token required by the Neohub
    WebSocketsClient m_websocketClient;                         // the Websocket client for the connection
    bool m_deleted = false;                                     // Flag indicating if this connection has been deleted
                                                                // set by finish() to mark the connection for
                                                                // deletion once all processing is complete

    std::deque<Conversation*> m_conversations;                  // Queue with all lives and pending conversations

    // Event handler if an event is received from the Neohub connection
    static void webSocketEventHandler(WStype_t type, uint8_t * payload, size_t length, void *clientData);

    std::function<void()> m_on_connect = nullptr;               // callbacks
    std::function<void()> m_on_disconnect = nullptr;
    std::function<void(String message)> m_on_error = nullptr;

public:
    // Constructor: for access to a particular hub with a particular access token
    NeohubConnection(const String &host, const String &accessToken) : m_host(host), m_accessToken(accessToken) {};

private:
    // Private destructor so nobody can delete an object from a callback
    // use NeohubConnection::finish() instead which will delete the object in the next loop
    ~NeohubConnection() {};     

public:
    // Connecetion lifecyle -------------------------------------------------------
    bool open(int timeoutMillis = -1);
    bool close(int timeoutMillis = -1);
    void finish() { m_deleted = true; };  // delayed deletion - will be deleted in next loop
    bool isConnected();

    // Callbacks ------------------------------------------------------------------
    // Set callback called after the connection was established
    void onConnect(std::function<void()> func) { m_on_connect = func; };

    // Set callback called on disconnection
    // also called if establishing the connection in open() fails
    void onDisconnect(std::function<void()> func) { m_on_disconnect = func; };

    // Set callback for any errors. Called in addition to any Conversation error callback
    void onError(std::function<void(String errorText)> func) { m_on_error = func; };

    // Conversation ---------------------------------------------------------------
    bool send (
        const String &command,                                  // Command to send
        std::function<void(String responseJson)> onReceive,     // called when response is received
        std::function<void(String message)> onError,            // called when an error occurs
        int timeoutMillis = 2000
    );

private:

    // loop function for the connection - called regularly by the loop task
    void loop();
    bool startConversation(Conversation *c);

    // The loop task which regularly executes the loop function for all connections
    // (one for all NeohubConnection objects)
    static TaskHandle_t m_loopTaskHandle;
    static void ensureLoopTask();
    static void loopTask(void *parameter);

    // a set of all live connections for which loop() has to be called
    static std::unordered_set<NeohubConnection *> m_liveConnections;

    // A mutex preventing concurrent access to the set with the live connections
    static MyMutex m_loopMutex;

    // Add this connecetion to the processing in the loop
    void addToLoopTask();

    // Wrap the actual message in the convoluted message queue objects expeted by the Neohub
    String wrapCommand(String command);

};

#endif
