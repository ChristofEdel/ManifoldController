#include "NeohubConnection.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "EspTools.h"
#include "MyLog.h"
#include "esp_transport_ws.h"
#include "StringTools.h"

#undef DEBUG_LOG
#define DEBUG_LOG(fmt, ...) ;

// Opens a WebSocket connecetion to the Neohub
// 
// Creates and starts the WebSocket client.
// 
// Returns immediately, the actual connection process is asynchroneous.
// If the connection is established, the onConnect callback is called.
//
// Returns false if the client could not be started, in that case start 
// needs to be called again later, or this connection should be deleted.
//
bool NeohubConnection::start()
{
    esp_websocket_client_config_t cfg = {};
    cfg.host = this->m_host.c_str();               // the URL, split in its components
    cfg.port = 4243;                               //
    cfg.path = "/";                                //
    cfg.transport = WEBSOCKET_TRANSPORT_OVER_SSL;  // Neohub requires SSL, but with self-signed certs
    cfg.skip_cert_common_name_check = true;        // So we can't check anything
    cfg.use_global_ca_store = false;               // 
    cfg.cert_pem = nullptr;                        // 
    cfg.enable_close_reconnect = true;             // Reconnect even if server closed the connection
    cfg.reconnect_timeout_ms = 5000;               // Try to reconnect 5 seconds after disconnection
    cfg.ping_interval_sec = SIZE_MAX / 2;          // Disable pings, they cause connection drops on the Neohub

    this->m_websocketClient = esp_websocket_client_init(&cfg);

    esp_websocket_register_events(this->m_websocketClient, WEBSOCKET_EVENT_ANY, webSocketEventHandler, (void*)this);
    esp_err_t err = esp_websocket_client_start(this->m_websocketClient);
    if (err != ESP_OK) {
        esp_websocket_client_destroy(this->m_websocketClient);
        this->m_websocketClient = nullptr;
        return false;
    }
    return true;
}

// Disconnect the WebSocket
void NeohubConnection::stop()
{
    if (this->m_websocketClient == nullptr) return;
    esp_websocket_client_stop(this->m_websocketClient);
    esp_websocket_client_destroy(this->m_websocketClient);
    this->m_websocketClient = nullptr;
}

// Check if the underlying WebSocket is currently connected
bool NeohubConnection::isConnected()
{
    return esp_websocket_client_is_connected(this->m_websocketClient);
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
    if (!this->isConnected()) return false;
    
    // Beause all of this is asynchroneous, we do this in a Conversation
    // and store it for the processing of the results
    NeohubConversation* c = new NeohubConversation(
        this->m_nextConversationId++,
        command,
        onReceive, onError,
        timeoutMillis
    );
    m_conversations.put(c->id, c);

    // Send the command to the WebSocket, invest half of the available timeout for sending
    String message = this->wrapCommand(c->command, c->id);
    int result = esp_websocket_client_send_text(this->m_websocketClient, message.c_str(), message.length(), pdMS_TO_TICKS(timeoutMillis/2));

    // If it failed to send, we report an error and return false
    if (result < 0) {
        if (c->m_onError) c->m_onError("NeohubConnection: send to WebSocketClient failed");
        m_conversations.remove(c->id);  // don't keep this there will be no response
        delete c;
        return false;
    }
    return true;
}

void NeohubConnection::handleTimeouts()
{
    ThreadSafePtrMap::Iterator it(m_conversations);
    while (it.next()) {
        NeohubConversation* c = (NeohubConversation*)it.value();
        if (c->timeoutExceeded()) {
            m_conversations.remove(it.key());
            // call the error handler
            if (c->m_onError) {
                c->m_onError("NeohubConnection: Timeout waiting for response");
            }
            delete c;
        }
    }
} 

// Internal event handler for events from the Websocket. 
// Translates these events into callbacks on the connection or conversation, as appropriate
/* static */ void NeohubConnection::webSocketEventHandler(void *arg, esp_event_base_t base, int32_t eventId, void *eventData)
{
    NeohubConnection* _this = (NeohubConnection*)arg;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *) eventData;
    
    switch (eventId) {

        // Artificial events which we don't need
        case WEBSOCKET_EVENT_BEFORE_CONNECT:    // Occurs before connecting
        case WEBSOCKET_EVENT_BEGIN:            // Occurs once after thread creation, before event loop
        case WEBSOCKET_EVENT_FINISH:            // Occurs once after event loop, before thread destruction */
            // Ignored
            break;

        // The Websocket has connected --> this NeoHubConnection's onConnect handler
        case WEBSOCKET_EVENT_CONNECTED:
            DEBUG_LOG("WEBSOCKET_EVENT_CONNECTED");
            if (_this->m_onConnect) _this->m_onConnect();
            _this->handleTimeouts();
            break;

        // The websocket has disconnected --> this NeoHubConnection's onDisconnect handler
        case WEBSOCKET_EVENT_DISCONNECTED:
            DEBUG_LOG("WEBSOCKET_EVENT_DISCONNECTED");
            if (_this->m_onDisonnect) _this->m_onDisonnect();
            _this->handleTimeouts();
            break;

        // a close frame has been received. Ususally followed by DISCONNECTED event
        case WEBSOCKET_EVENT_CLOSED:
            DEBUG_LOG("WEBSOCKET_EVENT_CLOSED");
            break;

        // TEXT received event --> the current conversation's onError or onReceive handlers
        case WEBSOCKET_EVENT_DATA: 
            DEBUG_LOG("WEBSOCKET_EVENT_DATA: op_code=%d, len=%d, offset=%d, total_len=%d",
                data->op_code, data->data_len, data->payload_offset, data->payload_len);
                switch (data->op_code) {
                    case WS_TRANSPORT_OPCODES_CONT:
                    case WS_TRANSPORT_OPCODES_TEXT:
                        _this->textReceived(data->data_ptr, data->data_len, data->payload_offset, data->payload_len);
                        break;
                    case WS_TRANSPORT_OPCODES_BINARY:
                        DEBUG_LOG("Unexpected BINARY frame received");
                        if (_this->m_onError) _this->m_onError("NeohubConnection: unexpected binary frame");
                        break;
                    case WS_TRANSPORT_OPCODES_CLOSE:
                        DEBUG_LOG("WS_TRANSPORT_OPCODES_CLOSE: %d", data->op_code);
                        break;
                    case WS_TRANSPORT_OPCODES_PING:
                    case WS_TRANSPORT_OPCODES_PONG:
                        // ignore
                        break;
                    default:
                        DEBUG_LOG("Unexpected OPCODE received: %d", data->op_code);
                        if (_this->m_onError) _this->m_onError("NeohubConnection: unexpected OPCODE: " + String(data->op_code));
                        break;
                }
            break;

        case WEBSOCKET_EVENT_ERROR:
            DEBUG_LOG("WEBSOCKET_EVENT_ERROR");
            if (_this->m_onError) _this->m_onError("NeohubConnection: WebSocket error event");
            break;

        default:
            DEBUG_LOG("WEBSOCKET_EVENT_UNKNOWN: %d", eventId);
            if (_this->m_onError) _this->m_onError(StringPrintf("NeohubConnection: unknown websocket event: %d", eventId));
            break;
            
    }
}

void NeohubConnection::textReceived(const char *buffer, size_t length, size_t offset, size_t totalLength)
{
    if (length == 0) return;
    if (buffer == nullptr) return;

    // Simple case: full message received in one go
    if (offset == 0 && length == totalLength) {
        m_receiveBuffer = String(buffer, length);
        processMessage(m_receiveBuffer);
        m_receiveBuffer = "";
        return;
    }

    // Multi-part message: accumulate

    // First part received - start a new buffer
    if (offset == 0) {
        m_receiveBuffer = String(buffer, length);
        return;
    }

    // Intermediate or final part received - append to buffer
    m_receiveBuffer += String(buffer, length);

    // If this was the last part, process the message
    if ((offset + length) >= totalLength) {
        processMessage(m_receiveBuffer);
        m_receiveBuffer = "";
    }

    // Otherwise, (offset + length) < totalLength, wait for more parts
}

void NeohubConnection::processMessage(const String& message)
{
    DEBUG_LOG("Processing message: %s\n", message.c_str());

    // Deserialise and report any erros
    JsonDocument json;
    DeserializationError error = deserializeJson(json, message);
    if (error) {
        String errorMessage = StringPrintf(
            "NeohubConnection: Deserialisation error: %s, JSON: \"%s\"",
            error.c_str(), message.c_str()
        );
        if (this->m_onError) this->m_onError(errorMessage);
    }

    // Get the conversation ID from the response
    else {
        int id = json["command_id"] | -1;
        if (id == -1) {
            String errorMessage = StringPrintf(
                "NeohubConnection: Response without command Id, JSON: \"%s\"",
                message.c_str()
            );
            if (this->m_onError) this->m_onError(errorMessage);
        }

        else {
            // Find the conversation for this ID
            NeohubConversation* c = (NeohubConversation*)this->m_conversations.take(id);
            if (!c) {
                String errorMessage = StringPrintf(
                    "NeohubConnection: Response for unknown command Id %d, JSON: \"%s\"",
                    id, message.c_str()
                );
                if (this->m_onError) this->m_onError(errorMessage);
            }
            else {
                if (c->m_onReceive) {
                    c->m_onReceive(json["response"].as<String>());
                    delete c;
                }
            }
        }
    }
    this->handleTimeouts();
}

// Internal helper function to translate a simple stringcommand into the convoluted
// multi-level JSON expected by the NeoHub API.
//
// The actual command uses a "bastardised" JSON where ' is used instead of ", which avoids
// awkward quotes like \\\"..., so at the beginning we take the command and replace
// all " we find with '.
String NeohubConnection::wrapCommand(String command, int id /* = 1 */)
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
    result += R"(\",\"COMMANDID\":)";
    result += String(id);
    result += R"(}]}"})";
    return result;
}