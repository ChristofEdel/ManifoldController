#include "NeohubConnection.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "EspTools.h"
#include "MyLog.h"

#undef DEBUG_LOG
#define DEBUG_LOG(fmt, ...) ;

// Opens a WebSocket connecetion to the Neohub
// 
// Calls "beginSSL" on the embedded WebsocketClient and 
// makes sure it's "loop" function is called in the loop task. The actual
// opening is done in the loop so this always returns true.
//
// If the oppening is successful, the onConnect callback is called
// If the opeening fails, the onError callback is called, followed by the onDisconnect callback
void NeohubConnection::connect(int timeoutMillis /* = -1 */)
{
    this->m_websocketClient.beginSSL(this->m_host, 4243, "/");
    this->m_websocketClient.onEvent(NeohubConnection::webSocketEventHandler, this);
    this->addToLoopTask();
    ensureLoopTask();
}

// Disconnect the WebSocket
//
// after disconnection, the onDisconnect callback is called.
void NeohubConnection::disconnect(int timeoutMillis /* = -1 */)
{
    this->m_websocketClient.disconnect();
}

void NeohubConnection::finish() {
    this->m_deleted = true;
    this->m_onConnect = nullptr; 
    this->m_onError = nullptr; 
    this->m_onDisonnect = nullptr; 
}

// Check if the underlying WebSocket is currently connected
bool NeohubConnection::isConnected()
{
    return !this->m_deleted && this->m_websocketClient.isConnected();
}

// Send a command to the Neohub.
//
// Depending on success or oerror, the onReceive or onError callback is called then
// a reply is received.
bool NeohubConnection::send(
    const String& command,
    std::function<void(String responseJson)> onReceive,
    std::function<void(String message)> onError,
    int timeoutMillis
)
{
    if (this->m_deleted) return false;
    
    // Beause all of this is asynchroneous, we do this in a Conversation
    Conversation* c = new Conversation(
        command,
        onReceive, onError,
        timeoutMillis
    );
    bool success = true;

    // Do not do anything while processing in the loop is ongoing
    if (m_loopMutex.lock(__PRETTY_FUNCTION__)) {

        // If there are no live conversations, send the command immediately
        if (m_conversations.empty()) {
            String s = wrapCommand(c->m_command);
            c->m_commandSent = this->m_websocketClient.sendTXT(s);
            // If sending was successful, put into processing queue
            // for processing the return
            if (c->m_commandSent) {
                m_conversations.push_back(c);
            }
            // If sending failed, give up and return false
            else {
                if (c->m_onError) c->m_onError("NeohubConnection: send to WebSocketClient failed");
                delete c;
                success = false;
            }
        }
        // If there are live conversations, join the queue
        else {
            m_conversations.push_back(c);
        }
        m_loopMutex.unlock();
    }
    return success;
}

// Start a conversation; remove from queue if it cannot be started
// (private function used in loop)
bool NeohubConnection::startConversation(NeohubConnection::Conversation* c)
{
    // If it is too late to start, we time out
    if (c->timeoutExceeded()) {
        if (c->m_onError) c->m_onError("NeohubConnection: Conversation timed out before sending");
        return false;
    };

    // We send the command and return true if successful
    String s = wrapCommand(c->m_command);
    c->m_commandSent = this->m_websocketClient.sendTXT(s);
    if (c->m_commandSent) return true;

    // If we failed to send it, we report an error
    if (c->m_onError) c->m_onError("NeohubConnection: Send to WebSocketClient failed");
    return false;
}

// Function to be calle in the loop to process messages to and from the Neohub
void NeohubConnection::loop()
{
    if (this->m_deleted) return;

    // NMB - Mutex already acquired in calling function, do not need to lock here

    // CHeck the first conversation in the queue and start it if required
    Conversation* c = m_conversations.empty() ? nullptr : m_conversations.front();
    if (c && !c->m_commandSent) {
        if (!startConversation(c)) {
            // if we can't start it, we get rid of it
            m_conversations.pop_front();
            delete c;
        };
    }

    // normal client processing
    m_websocketClient.loop();
    // the loop() above may process conversations and remove them from the queue!

    // Now we process the next conversation in the queue
    while ((c = (m_conversations.empty() ? nullptr : m_conversations.front()))) {
        // If it needs to be started, we try to start it.
        // and then finish this iteratioh.
        if (!c->m_commandSent) {
            if (!startConversation(c)) {
                m_conversations.pop_front();
                delete c;
            }
            break;
        }

        // if it is waiting for a response, we check for timeout.
        // if it has not timed out, we wait for it to complete and process no more
        // conversations until then
        if (!c->timeoutExceeded()) break;

        // If it has timed out, we finish it with an error and process the next
        // conversation in the queue
        c->m_onError("NeohubConnection: Timeout waiting for response");
        m_conversations.pop_front();
        delete c;
    }
}

// Internal event handler for events from the Websocket. 
// Translates these events into callbacks on the connection or conversation, as appropriate
void NeohubConnection::webSocketEventHandler(WStype_t type, uint8_t* payload, size_t length, void* clientData)
{
    NeohubConnection* _this = (NeohubConnection*)clientData;
    if (_this->m_deleted) return;

    switch (type) {
        // Connect event --> this NeoHubConnection's onConnect handler
        case WStype_CONNECTED:
            DEBUG_LOG("WStype_CONNECTED");
            if (_this->m_onConnect) _this->m_onConnect();
            break;

        // Disconnect event --> this NeoHubConnection's onDisconnect handler
        case WStype_DISCONNECTED:
            DEBUG_LOG("WStype_DISCONNECTED");
            if (length > 0) {
                DEBUG_LOG("%s", String(payload, length).c_str());
            }
            if (_this->m_onDisonnect) _this->m_onDisonnect();
            break;

        // TEXT received event --> the current conversation's onError or onReceive handlers
        case WStype_TEXT: {
            DEBUG_LOG("WStype_TEXT");
            // Send the received data to the conversation's hanlder and then remove
            // the conversation from the processing queue
            Conversation* c = _this->m_conversations.empty() ? nullptr : _this->m_conversations.front();
            if (c) {
                if (!c->m_commandSent) {
                    // catastrophic failure! need to clear queue and reconnect
                    softwareAbort(SW_RESET_WEBSOCKET_ABORT);
                }

                // Deserialise and report any erros
                JsonDocument json;
                DeserializationError error = deserializeJson(json, payload, length);
                if (error) {
                    String message = "NeohubConnection: Failed to parse response: ";
                    message += error.c_str();
                    message += "\n";
                    message += String(payload, length);
                    MyLog.println(message);
                    if (_this->m_onError) _this->m_onError(message);
                    if (c->m_onError) c->m_onError(message);
                }

                // Message is ok, deliver the result
                else {
                    if (c->m_onReceive) {
                        c->m_onReceive(json["response"].as<String>());
                    }
                }

                // In all cases, we are done and can remove this from the processing queue
                _this->m_conversations.pop_front();
                delete c;
            }
            else {
                String message = String("NeohubConnection: Message received outside of a conversation: ");
                message += String(payload, min((int)length, 30));
                MyLog.println(message);
                if (_this->m_onError) _this->m_onError(message);
            }
        }
            break;

        case WStype_ERROR:
            DEBUG_LOG("WStype_ERROR");
            if (_this->m_onError) _this->m_onError("NeohubConnection: WebSocket error event");
            break;

        case WStype_FRAGMENT_TEXT_START:
        case WStype_FRAGMENT:
        case WStype_FRAGMENT_FIN:
            DEBUG_LOG("WStype_FRAGMENT_xxxx");
            if (_this->m_onError) _this->m_onError("NeohubConnection: Fragment received - unhandled");
            break;

        // Binary data messages - we don't need them so we don't handle them
        case WStype_BIN:
        case WStype_FRAGMENT_BIN_START:
            DEBUG_LOG("WStype_BIN or WStype_FRAGMENT_BIN_START");
            if (_this->m_onError) _this->m_onError("NeohubConnection: Binary data received - unhandled");
            break;

        // Messages we don't care for
        case WStype_PING:
        case WStype_PONG:
            break;

        default:
            DEBUG_LOG("WStype_UNKNOWN");
            break;
            
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
// The loop task, which calls the loop function on a regular basis

// Handle for the loop task
TaskHandle_t NeohubConnection::m_loopTaskHandle = 0;

// Colleciton of all the live connections for which the loop function has to be called
std::unordered_set<NeohubConnection*> NeohubConnection::m_liveConnections;

// A MUTEX to protect the above collection and to avoid running certain code while
// the loop function performs its tasks
MyMutex NeohubConnection::m_loopMutex("NeohubConnection::m_loopMutex");

// The actual loop task
void NeohubConnection::loopTask(void* parameter)
{
    for (;;) {
        if (m_loopMutex.lock(__PRETTY_FUNCTION__)) {

            // Iterate over all connections
            for (auto iterator = m_liveConnections.begin(); iterator != m_liveConnections.end();) {
                NeohubConnection* connection = *iterator;

                // if the connection has been deleted, collect its memory
                if (connection->m_deleted) {
                    iterator = m_liveConnections.erase(iterator);
                    delete connection;
                }
                else {
                    connection->loop();
                    iterator++;
                }
            }
            m_loopMutex.unlock();
        }
        delay(10);
    }
}

// Add this connection to the loop
void NeohubConnection::addToLoopTask()
{
    if (m_loopMutex.lock(__PRETTY_FUNCTION__)) {
        m_liveConnections.insert(this);
        m_loopMutex.unlock();
    }
}

// Make sure the loop task is running
void NeohubConnection::ensureLoopTask()
{
    if (m_loopTaskHandle) return;
    xTaskCreate(
        NeohubConnection::loopTask,  // Task function
        "WebSocketLoop",             // Task name
        4096 * 4,                    // Stack size (bytes)
        NULL,                        // Parameter to pass
        1,                           // Task priority
        &m_loopTaskHandle            // Task handle
    );
}

// Internal helper function to translate a simple stringcommand into the convoluted
// multi-level JSON expected by the NeoHub API.
//
// The actual command uses a "bastardised" JSON where ' is used instead of ", which avoids
// awkward quotes like \\\"..., so at the beginning we take the command and replace
// all " we find with '.
String NeohubConnection::wrapCommand(String command)
{
    // Commands to the Neohub are in a three-level JSON, where a second and third level JSON
    // is embedded in a string.
    //
    // Top level:
    // {
    //      "message_type": "hm_get_command_queue",
    //      "message": "<SECOND LEVEL MESSAGE>"
    // }
    //
    // Second level:
    // {
    //      token: "........",
    //      COMMANDS: [
    //          { COMMAND: "<ACTUAL COMMAND>", COMMANDID: 1 },
    //          { COMMAND: "<ACTUAL COMMAND>", COMMANDID: 2 }
    //          ...
    //      ]
    // }
    //
    // because the second and third levels are embedded in a string, we need to escape any '"'
    //
    // The actual command uses a "bastardised" JSON where ' is used instead of ", which avoids
    // awkward quotes like \\\"..., so at the beginning we take the command and replace
    // all " we find with '.
    //

    // First we turn JSON into the non-standard, single quote format
    int i = 0;
    const char* cp = command.c_str();
    while (*cp) {
        if (*cp == '"') command[i] = '\'';
        cp++, i++;
    }

    // This is the slow but "perfect" JSON construction
    //    JsonDocument jsonLevel2;
    //    String level2String;
    //    jsonLevel2["token"] = this->m_accessToken;
    //    jsonLevel2["COMMANDS"][0]["COMMAND"] = command;
    //    jsonLevel2["COMMANDS"][0]["COMMANDID"] = 1;
    //    serializeJson(jsonLevel2, level2String);

    //    JsonDocument jsonResult;
    //    String result;
    //    jsonResult["message_type"] = "hm_get_command_queue";
    //    jsonResult["message"] = level2String;
    //
    //    serializeJson(jsonResult, result);

    // to make this less resource consuming, we simply wrap the command in the always identical JSON for level 1 and level 2
    String commandQuoted = command;
    String result = R"({"message_type":"hm_get_command_queue","message":"{\"token\":\")";
    result += this->m_accessToken;
    result += R"(\",\"COMMANDS\":[{\"COMMAND\":\")";
    result += command;
    result += R"(\",\"COMMANDID\":1}]}"})";
    return result;
}
