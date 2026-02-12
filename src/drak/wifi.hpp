#ifndef WIFI_HPP
#define WIFI_HPP

#include "result.hpp"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "cstring"
#include "optional"
#include "vector"
#include "algorithm"
#include "string"

class Wifi;
class IpInfo;

using wifi_cb = void (*)(Wifi *);

static bool prop_is_connected = false;
static bool prop_is_conn_failed = false;
static bool prop_is_wifi_initialized = false;

class IpInfo
{
private:
    esp_ip4_addr_t ipv4_addr{};
    esp_ip4_addr_t netmask_addr{};
    esp_ip4_addr_t gateway_addr{};

public:
    explicit IpInfo(const esp_netif_ip_info_t &info)
    {
        this->ipv4_addr.addr = info.ip.addr;
        this->netmask_addr.addr = info.netmask.addr;
        this->gateway_addr.addr = info.gw.addr;
    }

    std::vector<uint8_t> get_ipv4_addr() const
    {
        std::vector<uint8_t> e(4);
        e[0] = esp_ip4_addr1(&this->ipv4_addr);
        e[1] = esp_ip4_addr2(&this->ipv4_addr);
        e[2] = esp_ip4_addr3(&this->ipv4_addr);
        e[3] = esp_ip4_addr4(&this->ipv4_addr);
        return e;
    }

    std::vector<uint8_t> get_netmask_addr() const
    {
        std::vector<uint8_t> e(4);
        e[0] = esp_ip4_addr1(&this->netmask_addr);
        e[1] = esp_ip4_addr2(&this->netmask_addr);
        e[2] = esp_ip4_addr3(&this->netmask_addr);
        e[3] = esp_ip4_addr4(&this->netmask_addr);
        return e;
    }

    std::vector<uint8_t> get_gateway_addr() const
    {
        std::vector<uint8_t> e(4);
        e[0] = esp_ip4_addr1(&this->gateway_addr);
        e[1] = esp_ip4_addr2(&this->gateway_addr);
        e[2] = esp_ip4_addr3(&this->gateway_addr);
        e[3] = esp_ip4_addr4(&this->gateway_addr);
        return e;
    }
};

class Wifi
{
public:
    enum Error
    {
        ALREADY_INITIALIZED,
        FAILED_INITIALIZATION,
        WIFI_NOT_INITIALIZED,
        SET_WIFI_MODE_STA,
        SET_WIFI_CONFIG,
        WIFI_START,
        SSID_TOO_SHORT,
        SSID_TOO_LARGE,
        PASSWORD_TOO_SHORT,
        PASSWORD_TOO_LONG,
        REGISTER_EVENT_HANDLER,
        FAILED_CONNECT,
        FAILED_DISCONNECT,
        LISTENER_ALREADY_PRESENT
    };

private:
    esp_netif_t *wifi_obj;
    wifi_config_t wifi_config;
    EventGroupHandle_t wifi_event_grp;

    std::optional<std::string> ssid;
    std::optional<std::string> password;
    std::optional<IpInfo> ip_info;

    std::vector<wifi_cb> on_start_handlers;
    std::vector<wifi_cb> on_stop_handlers;
    std::vector<wifi_cb> on_connected_handlers;
    std::vector<wifi_cb> on_disconnected_handlers;
    std::vector<wifi_cb> on_connection_failed_handlers;
    std::vector<wifi_cb> on_got_ip_handlers;
    std::vector<wifi_cb> on_lost_ip_handlers;

    static void ref_base_event_handler(void *arg, const esp_event_base_t event_base, const int32_t event_id,
                                       void *event_data)
    {
        auto *instance = static_cast<Wifi *>(arg);
        instance->base_event_handler(event_base, event_id, event_data);
    }

    void base_event_handler(const esp_event_base_t event_base, const int32_t event_id, void *event_data)
    {
        if (event_base == WIFI_EVENT)
        {
            switch (event_id)
            {
            case WIFI_EVENT_STA_START:
                prop_is_connected = false;
                prop_is_conn_failed = false;
                for (const auto &fn : this->on_start_handlers)
                    if (fn)
                        fn(this);
                break;
            case WIFI_EVENT_STA_STOP:
                prop_is_connected = false;
                prop_is_conn_failed = false;
                for (const auto &fn : this->on_stop_handlers)
                    if (fn)
                        fn(this);
                break;
            case WIFI_EVENT_STA_CONNECTED:
                prop_is_connected = true;
                prop_is_conn_failed = false;

                for (const auto &fn : this->on_connected_handlers)
                    if (fn)
                        fn(this);
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                if (prop_is_connected && !prop_is_conn_failed)
                {
                    prop_is_connected = false;
                    prop_is_conn_failed = false;

                    for (const auto &fn : this->on_disconnected_handlers)
                        if (fn)
                            fn(this);
                }
                else
                {
                    prop_is_connected = false;
                    prop_is_conn_failed = true;

                    for (const auto &fn : this->on_connection_failed_handlers)
                        if (fn)
                            fn(this);
                }
                break;
            default:;
            }

            return;
        }

        if (event_base == IP_EVENT)
        {
            const auto e = static_cast<ip_event_got_ip_t *>(event_data);

            switch (event_id)
            {
            case IP_EVENT_STA_GOT_IP:
                ip_info = IpInfo(e->ip_info);
                for (const auto &fn : this->on_got_ip_handlers)
                    if (fn)
                        fn(this);
                break;
            case IP_EVENT_STA_LOST_IP:
                ip_info.reset();
                for (const auto &fn : this->on_lost_ip_handlers)
                    if (fn)
                        fn(this);
                break;
            default:;
            }
        }
    }

public:
    Wifi()
    {
        this->wifi_obj = nullptr;
        this->wifi_config = {};
        this->wifi_event_grp = xEventGroupCreate();
    }

    Result<Wifi *, Error> init()
    {
        if (prop_is_wifi_initialized)
            return Result<Wifi *, Error>(ALREADY_INITIALIZED);

        this->wifi_obj = esp_netif_create_default_wifi_sta();
        const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

        if (esp_wifi_init(&cfg) != ESP_OK)
        {
            esp_wifi_deinit();
            esp_netif_destroy_default_wifi(this->wifi_obj);
            this->wifi_obj = nullptr;
            return Result<Wifi *, Error>(FAILED_INITIALIZATION);
        }

        std::strncpy(reinterpret_cast<char *>(this->wifi_config.sta.ssid), this->ssid.value().c_str(),
                     sizeof(this->wifi_config.sta.ssid));
        std::strncpy(reinterpret_cast<char *>(this->wifi_config.sta.password), this->password.value().c_str(),
                     sizeof(this->wifi_config.sta.password));

        if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
        {
            esp_wifi_deinit();
            esp_netif_destroy_default_wifi(this->wifi_obj);
            this->wifi_obj = nullptr;
            return Result<Wifi *, Error>(SET_WIFI_MODE_STA);
        }

        if (esp_wifi_set_config(WIFI_IF_STA, &this->wifi_config) != ESP_OK)
        {
            esp_wifi_deinit();
            esp_netif_destroy_default_wifi(this->wifi_obj);
            this->wifi_obj = nullptr;
            return Result<Wifi *, Error>(SET_WIFI_CONFIG);
        }

        if (esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &Wifi::ref_base_event_handler, this) != ESP_OK)
        {
            esp_wifi_deinit();
            esp_netif_destroy_default_wifi(this->wifi_obj);
            this->wifi_obj = nullptr;
            return Result<Wifi *, Error>(REGISTER_EVENT_HANDLER);
        }

        if (esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &Wifi::ref_base_event_handler, this) != ESP_OK)
        {
            esp_wifi_deinit();
            esp_netif_destroy_default_wifi(this->wifi_obj);
            this->wifi_obj = nullptr;
            return Result<Wifi *, Error>(REGISTER_EVENT_HANDLER);
        }

        if (esp_wifi_start() != ESP_OK)
        {
            esp_wifi_deinit();
            esp_netif_destroy_default_wifi(this->wifi_obj);
            this->wifi_obj = nullptr;
            return Result<Wifi *, Error>(WIFI_START);
        }

        prop_is_wifi_initialized = true;

        return Result<Wifi *, Error>(this);
    }

    Result<bool, Error> set_ssid(std::string ssid)
    {
        if (ssid.length() < 1)
            return Result<bool, Error>(SSID_TOO_SHORT);

        if (ssid.length() > 32)
            return Result<bool, Error>(SSID_TOO_LARGE);

        this->ssid = ssid;
        return Result<bool, Error>(true);
    }

    Result<bool, Error> set_password(std::string password)
    {
        if (password.length() < 6)
            return Result<bool, Error>(PASSWORD_TOO_SHORT);

        if (password.length() > 64)
            return Result<bool, Error>(PASSWORD_TOO_LONG);

        this->password = password;
        return Result<bool, Error>(true);
    }

    Result<bool, Error> connect()
    {
        if (!prop_is_wifi_initialized)
            return Result<bool, Error>(WIFI_NOT_INITIALIZED);

        if (esp_wifi_connect() != ESP_OK)
            return Result<bool, Error>(FAILED_CONNECT);

        return Result<bool, Error>(true);
    }

    Result<bool, Error> disconnect()
    {
        if (!prop_is_wifi_initialized)
            return Result<bool, Error>(WIFI_NOT_INITIALIZED);

        if (esp_wifi_disconnect() != ESP_OK)
            return Result<bool, Error>(FAILED_DISCONNECT);

        return Result<bool, Error>(true);
    }

    std::optional<IpInfo> get_ipv4_info() const
    {
        return this->ip_info;
    }

    Result<bool, Error> add_on_start_listener(const wifi_cb listener)
    {
        const bool listener_present = std::ranges::find(this->on_start_handlers, listener) !=
                                      this->on_start_handlers.end();

        if (listener_present)
            return Result<bool, Error>(LISTENER_ALREADY_PRESENT);

        this->on_start_handlers.push_back(listener);
        return Result<bool, Error>(true);
    }

    Result<bool, Error> add_on_stop_listener(const wifi_cb listener)
    {
        const bool listener_present = std::ranges::find(this->on_stop_handlers, listener) !=
                                      this->on_stop_handlers.end();

        if (listener_present)
            return Result<bool, Error>(LISTENER_ALREADY_PRESENT);

        this->on_stop_handlers.push_back(listener);
        return Result<bool, Error>(true);
    }

    Result<bool, Error> add_on_connected_listener(const wifi_cb listener)
    {
        const bool listener_present = std::ranges::find(this->on_connected_handlers,
                                                        listener) != this->on_connected_handlers.end();

        if (listener_present)
            return Result<bool, Error>(LISTENER_ALREADY_PRESENT);

        this->on_connected_handlers.push_back(listener);
        return Result<bool, Error>(true);
    }

    Result<bool, Error> add_on_disconnected_listener(const wifi_cb listener)
    {
        const bool listener_present = std::ranges::find(this->on_disconnected_handlers,
                                                        listener) != this->on_disconnected_handlers.end();

        if (listener_present)
            return Result<bool, Error>(LISTENER_ALREADY_PRESENT);

        this->on_disconnected_handlers.push_back(listener);
        return Result<bool, Error>(true);
    }

    Result<bool, Error> add_on_connection_failed_listener(const wifi_cb listener)
    {
        const bool listener_present = std::ranges::find(this->on_connection_failed_handlers,
                                                        listener) != this->on_connection_failed_handlers.end();

        if (listener_present)
            return Result<bool, Error>(LISTENER_ALREADY_PRESENT);

        this->on_connection_failed_handlers.push_back(listener);
        return Result<bool, Error>(true);
    }

    Result<bool, Error> add_on_got_ip_listener(const wifi_cb listener)
    {
        const bool listener_present = std::ranges::find(this->on_got_ip_handlers,
                                                        listener) != this->on_got_ip_handlers.end();

        if (listener_present)
            return Result<bool, Error>(LISTENER_ALREADY_PRESENT);

        this->on_got_ip_handlers.push_back(listener);
        return Result<bool, Error>(true);
    }

    Result<bool, Error> add_on_lost_ip_listener(const wifi_cb listener)
    {
        const bool listener_present = std::ranges::find(this->on_lost_ip_handlers,
                                                        listener) != this->on_lost_ip_handlers.end();

        if (listener_present)
            return Result<bool, Error>(LISTENER_ALREADY_PRESENT);

        this->on_lost_ip_handlers.push_back(listener);
        return Result<bool, Error>(true);
    }

    ~Wifi()
    {
        esp_wifi_stop();
        esp_wifi_deinit();
        esp_netif_destroy_default_wifi(this->wifi_obj);
        this->wifi_obj = nullptr;
        prop_is_wifi_initialized = false;
        prop_is_connected = false;
        prop_is_conn_failed = false;
    }
};

#endif
