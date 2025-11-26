#include <Arduino.h>
#include "esp_websocket_client.h"

enum class NeohubConnectionResult {
    Ok = 0,
    UnableToInitialiseClient,
    UnknownEvent,
    UnknownError
};

class NeohubConnection {
private:
    String m_url;
    esp_websocket_client_handle_t m_websocketClient;
    void handleSocketConnected();   
    static void websocketEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    std::function<void()> m_on_connect;
    std::function<void(String)> m_on_receive;
    std::function<void(NeohubConnectionResult error, String details)> m_on_error;
    std::function<void()> m_on_close;

public:
    NeohubConnection(const String &hubHost);
    NeohubConnectionResult open();
    bool close();
    bool send (String json, int timeoutMillis = -1);
    void onConnect(std::function<void()> func) { m_on_connect = func; };
    void onReceive(std::function<void(String json)> func) { m_on_receive = func; }
    void onError(std::function<void(NeohubConnectionResult error, String details)> func) { m_on_error = func; }
    void onClose(std::function<void()> func) { m_on_close = func; }
};
