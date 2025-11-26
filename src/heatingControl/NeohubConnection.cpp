#include "NeohubCOnnection.h"

NeohubConnection::NeohubConnection(const String &hubUrl) {
    m_url = hubUrl;
    if (!m_url.startsWith("ws:")) m_url = "wss:/" + m_url;
    if (!m_url.endsWith("/")) m_url += ":4243/"; 
}

NeohubConnectionResult NeohubConnection::open() {

    // Configure the client for access without certificate checking
    // (beause the hub provides a self-signed certificate)
    esp_websocket_client_config_t cfg = {};
    cfg.uri = this->m_url.c_str();
    cfg.skip_cert_common_name_check = true;
    cfg.cert_pem = nullptr;            // accept any cert (insecure)
    cfg.disable_auto_reconnect = true; // just one shot

    // Initialise the client and register our event handler
    this->m_websocketClient = esp_websocket_client_init(&cfg);
    if (!this->m_websocketClient) return NeohubConnectionResult::UnableToInitialiseClient;

    esp_websocket_register_events(this->m_websocketClient, WEBSOCKET_EVENT_ANY, NeohubConnection::websocketEventHandler, this);
    esp_websocket_client_start(this->m_websocketClient);

    return NeohubConnectionResult::Ok;
}

bool NeohubConnection::send(String json, int timeoutMillis /* = -1 */) {
    // Upstream WS is connected → send the request payload
    int result = esp_websocket_client_send_text(
        this->m_websocketClient,
        json.c_str(),
        json.length(),
        timeoutMillis <= 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMillis)
    );
    return result >= 0;
    // On error, result is -1 and actual errors are reported using the onerror callback
}

void NeohubConnection::websocketEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    NeohubConnection* _this = (NeohubConnection *) arg;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED: {
            if (_this->m_on_connect) _this->m_on_connect();
            break;
        }

        case WEBSOCKET_EVENT_DATA: {
            if (_this->m_on_receive) {
                esp_websocket_event_data_t *dataReceived = (esp_websocket_event_data_t *)event_data;
                String data(dataReceived->data_ptr, dataReceived->data_len);
                _this->m_on_receive(data);
            }
            break;
        }

        case WEBSOCKET_EVENT_ERROR: {
            if (_this->m_on_error) {
                _this->m_on_error(NeohubConnectionResult::UnknownError, "???");
            }
            break;
        }

        case WEBSOCKET_EVENT_CLOSED: {
            if (_this->m_on_close) _this->m_on_close();
            break;
        }

        default: {
            if (_this->m_on_error) _this->m_on_error(NeohubConnectionResult::UnknownEvent, String("Event ID: ") + String(event_id,10)) ;
            break;
        }
    }
}

