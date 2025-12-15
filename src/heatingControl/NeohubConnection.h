#ifndef __NEOHUB_CONNECTION_H
#define __NEOHUB_CONNECTION_H

#include <Arduino.h>

#include <deque>
#include <unordered_set>

#include "MyLog.h"
#include "MyMutex.h"
#include "ThreadSafePtrMap.h"
#include "esp_websocket_client.h"
#include "esp_idf_version.h"

class NeohubConversation;

class NeohubConnection {
  private:
    String m_host;                       // the Neohub host (name or ip address)
    String m_accessToken;                // the access token required by the Neohub

    esp_websocket_client_handle_t m_websocketClient = 0; // the Websocket client for the connection
    ThreadSafePtrMap m_conversations;                    // Map with all live conversations
    int m_nextConversationId = 1;                        // Next conversation ID to use
    String m_receiveBuffer;                              // Buffer for incoming message fragments

    // Event handler if an event is received from the Neohub connection
    static void webSocketEventHandler(void *arg, esp_event_base_t base, int32_t event_id, void *clientData);
    void textReceived(const char *buffer, size_t length, size_t offset, size_t totalLength);
    void processMessage(const String& message);

    std::function<void()> m_onConnect = nullptr;  // callbacks
    std::function<void()> m_onDisonnect = nullptr;
    std::function<void(String message)> m_onError = nullptr;

    static int nextInstanceNo;
    int instanceNo;

  public:
    // Constructor: for access to a particular hub with a particular access token
    NeohubConnection(const String& host, const String& accessToken) : m_host(host), m_accessToken(accessToken) {
        instanceNo = nextInstanceNo++;
        DEBUG_LOG("Created %d", instanceNo);
    };
    ~NeohubConnection() {
        DEBUG_LOG("Deleting %d", instanceNo);
        this->stop();
    };

    // Connecetion lifecyle -------------------------------------------------------
    bool start();
    void stop();
    bool isConnected();

    // Callbacks ------------------------------------------------------------------
    // Set callback called after the connection was established
    void onConnect(std::function<void()> func) { m_onConnect = func; };

    // Set callback called on disconnection
    // also called if establishing the connection in open() fails
    void onDisconnect(std::function<void()> func) { m_onDisonnect = func; };

    // Set callback for any errors. Called in addition to any Conversation error callback
    void onError(std::function<void(String errorText)> func) { m_onError = func; };

    // Conversation ---------------------------------------------------------------
    bool send(
        const String& command,                               // Command to send
        std::function<void(String responseJson)> onReceive,  // called when response is received
        std::function<void(String message)> onError,         // called when an error occurs
        int timeoutMillis = 2000);

  private:
    // Internal processing functions to handle timeouts: checks all conversations in 
    // m_conversations for timeouts and calls their error handlers as appropriate
    void handleTimeouts();

    // Wrap the actual message in the convoluted message queue objects expeted by the Neohub
    String wrapCommand(String command, int id = 1);

};


// Internal class for a single message/response interaction
class NeohubConversation {
  friend class NeohubConnection;

  public:
    int id;                     // An ID for this conversation. Neohub supports 32 bit so this supports decades of conversations
    String command;             // The command sent
    unsigned long startMillis;  // The time at which the request was made (not sent)
    int timeoutMillis;          // A timeout measured from m_startMillis

    // Constructor for a new conversation
    NeohubConversation(
        int id,
        const String& command,
        std::function<void(String responseJson)> onReceive,
        std::function<void(String message)> onError,
        int timeoutMillis)
    : id(id), command(command), startMillis(millis()), timeoutMillis(timeoutMillis), m_onReceive(onReceive), m_onError(onError) 
    {};

    // Check if the conversation has timed out as of now
    bool timeoutExceeded() { return (millis() - startMillis) >timeoutMillis; }

  private:
    std::function<void(String responseJson)> m_onReceive;  // called when a response has been received
    std::function<void(String message)> m_onError;         // called when an error occured (which may be a failure to send the command)

};


#endif
