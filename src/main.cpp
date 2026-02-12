#include "drak/udp.hpp"
#include "drak/wifi.hpp"
#include "drak/color.hpp"
#include "drak/light_lang.hpp"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "stdint.h"
#include "driver/gpio.h"
#include "led_strip.h"

#define LED_COUNT 60
#define STRIP_GPIO 12

led_strip_handle_t led_strip;
LightLangCompiler llc;

void on_start(Wifi *w) { w->connect(); }

void on_connected(Wifi *_) { printf("Connected to Wi-Fi!\n"); }

void on_connection_failed(Wifi *w)
{
    printf("Failed to connect to Wi-Fi!, Retrying in 2(s)\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    w->connect();
}

void on_disconnected(Wifi *w)
{
    printf("Disconnected from Wi-Fi, Retrying in 2(s)!\n");
    vTaskDelay(pdMS_TO_TICKS(2000));
    w->connect();
}

void on_socket_message(UDP::Server *_, const std::string &data, const std::string &sender_ip, uint16_t sender_port)
{
    llc.terminate();
    llc.execute(data);
}

void on_got_ip(Wifi *w)
{
    const auto ipv4_addr = w->get_ipv4_info().value().get_ipv4_addr();
    printf("Got IP addr: %" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 "!\n",
           ipv4_addr[0], ipv4_addr[1], ipv4_addr[2], ipv4_addr[3]);

    auto ws = UDP::Server(3000);

    ws.add_on_message_listener(&on_socket_message);

    auto is_err = ws.start();

    if (is_err.is_err())
    {
        printf("Error while starting ws\n");
    }

    while (true)
        vTaskDelay(pdMS_TO_TICKS(2000));
}

void on_lost_ip(Wifi *_) { printf("Lost IP address!\n"); }

void configure_led(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = STRIP_GPIO,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_SK6812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }};

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        }};

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
}

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    configure_led();
    led_strip_set_pixel(led_strip, 1, 255, 0, 0);
    led_strip_refresh(led_strip);

    auto wifi = new Wifi();

    wifi->set_ssid("WIFI_SSID_HERE");
    wifi->set_password("WIFI_PASSWORD_HERE");

    wifi->add_on_start_listener(&on_start);
    wifi->add_on_connected_listener(&on_connected);
    wifi->add_on_disconnected_listener(&on_disconnected);
    wifi->add_on_connection_failed_listener(&on_connection_failed);

    wifi->add_on_got_ip_listener(&on_got_ip);
    wifi->add_on_lost_ip_listener(&on_lost_ip);

    wifi->init();
}
