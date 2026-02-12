#ifndef UDP_HPP
#define UDP_HPP

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "result.hpp"
#include <algorithm>
#include <list>
#include <string>
#include <cstring>

namespace UDP
{
  class Server;

  constexpr int MAX_EVENT_HANDLER_COUNT = 20;
  constexpr int RX_BUFFER_SIZE = 1024;

  enum Error
  {
    FAILED_CREATE_SOCKET,
    FAILED_BIND_SOCKET,
    FAILED_START_TASK,
    LISTENER_ALREADY_PRESENT,
    TOO_MANY_LISTENERS,
    EVENT_SOCKET_ERROR
  };

  using namespace std;

  using handler_error = void (*)(const Server *, Error);
  using handler_message = void (*)(Server *, const string &data, const string &sender_ip, uint16_t sender_port);

  class Server
  {
  private:
    int sock = -1;
    int port;
    TaskHandle_t thread_handle = nullptr;
    volatile bool is_running = false;

    SemaphoreHandle_t handler_mutex;

    std::list<handler_error> on_error_handlers;
    std::list<handler_message> on_message_handlers;

    static void udp_task(void *arg) { static_cast<Server *>(arg)->receiver_loop(); }

    [[noreturn]] void receiver_loop()
    {
      char rx_buffer[RX_BUFFER_SIZE];
      struct sockaddr_in dest_addr;

      dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
      dest_addr.sin_family = AF_INET;
      dest_addr.sin_port = htons(this->port);

      while (true)
      {
        sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock < 0)
        {
          this->emit_error_event(FAILED_CREATE_SOCKET);
          vTaskDelay(pdMS_TO_TICKS(1000));
          continue;
        }

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0)
        {
          this->emit_error_event(FAILED_BIND_SOCKET);
          close(sock);
          vTaskDelay(pdMS_TO_TICKS(1000));
          continue;
        }

        is_running = true;

        while (is_running)
        {
          struct sockaddr_storage source_addr;
          socklen_t socklen = sizeof(source_addr);

          int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

          if (len < 0)
          {
            this->emit_error_event(EVENT_SOCKET_ERROR);
            break;
          }
          else
          {
            rx_buffer[len] = 0;
            string msg(rx_buffer);
            char addr_str[128];
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
            uint16_t port = ntohs(((struct sockaddr_in *)&source_addr)->sin_port);

            this->emit_message_event(msg, string(addr_str), port);
          }
        }

        if (sock != -1)
        {
          shutdown(sock, 0);
          close(sock);
          sock = -1;
        }
      }
    }

    void emit_error_event(const Error e)
    {
      xSemaphoreTake(handler_mutex, portMAX_DELAY);
      for (const auto &fn : this->on_error_handlers)
        if (fn)
          fn(this, e);
      xSemaphoreGive(handler_mutex);
    }

    void emit_message_event(const string &msg, const string &ip, uint16_t port)
    {
      xSemaphoreTake(handler_mutex, portMAX_DELAY);
      for (const auto &fn : this->on_message_handlers)
        if (fn)
          fn(this, msg, ip, port);
      xSemaphoreGive(handler_mutex);
    }

  public:
    explicit Server(int port_num) : port(port_num)
    {
      handler_mutex = xSemaphoreCreateMutex();
    }

    Result<bool, Error> start()
    {
      BaseType_t res = xTaskCreate(udp_task, "udp_server", 4096, this, 5, &thread_handle);

      if (res != pdPASS)
      {
        return Result<bool, Error>(FAILED_START_TASK);
      }

      return Result<bool, Error>(true);
    }

    bool send_to(const string &ip, uint16_t port, const uint8_t *data, size_t len)
    {
      if (sock < 0)
        return false;

      struct sockaddr_in dest_addr;
      dest_addr.sin_addr.s_addr = inet_addr(ip.c_str());
      dest_addr.sin_family = AF_INET;
      dest_addr.sin_port = htons(port);

      int err = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
      return err >= 0;
    }

    Result<bool, Error> add_on_error_listener(const handler_error listener)
    {
      xSemaphoreTake(handler_mutex, portMAX_DELAY);
      if (this->on_error_handlers.size() >= MAX_EVENT_HANDLER_COUNT)
      {
        xSemaphoreGive(handler_mutex);
        return Result<bool, Error>(TOO_MANY_LISTENERS);
      }

      const bool listener_present =
          ranges::find(this->on_error_handlers, listener) !=
          this->on_error_handlers.end();

      if (listener_present)
      {
        xSemaphoreGive(handler_mutex);
        return Result<bool, Error>(LISTENER_ALREADY_PRESENT);
      }

      this->on_error_handlers.push_back(listener);
      xSemaphoreGive(handler_mutex);
      return Result<bool, Error>(true);
    }

    Result<bool, Error> add_on_message_listener(const handler_message listener)
    {
      xSemaphoreTake(handler_mutex, portMAX_DELAY);
      if (this->on_message_handlers.size() >= MAX_EVENT_HANDLER_COUNT)
      {
        xSemaphoreGive(handler_mutex);
        return Result<bool, Error>(TOO_MANY_LISTENERS);
      }

      const bool listener_present =
          ranges::find(this->on_message_handlers, listener) !=
          this->on_message_handlers.end();

      if (listener_present)
      {
        xSemaphoreGive(handler_mutex);
        return Result<bool, Error>(LISTENER_ALREADY_PRESENT);
      }

      this->on_message_handlers.push_back(listener);
      xSemaphoreGive(handler_mutex);
      return Result<bool, Error>(true);
    }

    ~Server()
    {
      is_running = false;
      if (sock != -1)
      {
        shutdown(sock, 0);
        close(sock);
      }
      if (thread_handle != nullptr)
      {
        vTaskDelete(thread_handle);
      }
      vSemaphoreDelete(handler_mutex);
      on_error_handlers.clear();
      on_message_handlers.clear();
    }
  };
}

#endif // UDP_HPP