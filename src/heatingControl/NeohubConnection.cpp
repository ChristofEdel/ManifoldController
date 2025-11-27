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

bool NeohubConnection::send(String json, int timeoutMillis /* = -1 */) {
    return this->m_websocketClient.sendTXT(json);
}

bool NeohubConnection::close(int timeoutMillis /* = -1 */) {
    this->m_websocketClient.disconnect();
    return true;
}

bool NeohubConnection::isConnected() {
    return this->m_websocketClient.isConnected();
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
        case WStype_TEXT:
            if (_this->m_on_receive) _this->m_on_receive(String(payload, length));
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
        if (m_loopMutex.lock()) {
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
            m_loopMutex.unlock();
        }
        delay(10);
    }
}

void NeohubConnection::addToLoopTask() {
    if (m_loopMutex.lock()) {
        m_liveConnections.insert(this);
        m_loopMutex.unlock();
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

//     NeohubConnection* _this = (NeohubConnection *) arg;
//     switch (event_id) {
//         case WEBSOCKET_EVENT_DATA: {
//             if (_this->m_on_receive) {
//                 esp_websocket_event_data_t *dataReceived = (esp_websocket_event_data_t *)event_data;
//                 Serial.printf(
//                     "************************ Receive opcode = %d, data_len = %d, payload_len = %d, payload_offset=%d\n************************ %s\n",
//                     dataReceived->op_code,
//                     dataReceived->data_len,
//                     dataReceived->payload_len,
//                     dataReceived->payload_offset,
//                     String(dataReceived->data_ptr, dataReceived->data_len).c_str()
//                 );
//                 // _this->m_responseText += String(dataReceived->data_ptr, dataReceived->data_len);
//                 // if (dataReceived->payload_offset + dataReceived->data_len == dataReceived->payload_len) {
//                 //     _this->m_on_receive(_this->m_responseText);
//                 // }
//             }
//             break;
            
//         }

//         case WEBSOCKET_EVENT_ERROR: {
//             if (_this->m_on_error) {
//                 // esp_websocket_event_data_t *errorData = (esp_websocket_event_data_t *) event_data;
//                 // String message = String::format(
//                 //     "TLS: 0x%x, stack: 0x%x, transport: 0x%x, errno: %d",
//                 //     errorData->error_handle->esp_tls_last_error,
//                 //     errorData->error_handle->esp_tls_stack_err,
//                 //     errorData->error_handle->esp_transport_err,
//                 //     errorData->error_handle->sock_errno
//                 // );
//                 _this->m_on_error("?"); 
//                 // Current implementation of esp_websocket_client does not have error reporting
//             }
//             break;
//         }

//         case WEBSOCKET_EVENT_DISCONNECTED: {
//             if (_this->m_on_disconnect) _this->m_on_disconnect("Unkown reason");
//             break;
//         }

//         case WEBSOCKET_EVENT_CLOSED: {
//             if (_this->m_on_close) _this->m_on_close();
//             break;
//         }

//         default: {
//             if (_this->m_on_error) _this->m_on_error(String("Unknown event, Event ID: ") + String(event_id,10)) ;
//             break;
//         }
//     }
// }

