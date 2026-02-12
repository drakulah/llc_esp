#ifndef LIGHT_LANG_HPP
#define LIGHT_LANG_HPP

#define LED_COUNT 60

#include <string>
#include <cstdlib>
#include "led_strip.h"

extern led_strip_handle_t led_strip;

struct TaskArgs
{
    std::string code;
};

class LightLangCompiler
{
public:
    ~LightLangCompiler()
    {
        this->terminate();
    }

    void terminate()
    {
    }

    void execute(std::string code)
    {
        bool loop = false;

        do
        {
            loop = code[0] == '1';

            for (int i = 1; i + 16 <= code.length(); i += 16)
            {
                int led_index = strtol(code.substr(i, 3).c_str(), nullptr, 16);

                if (led_index < 0 || led_index >= LED_COUNT)
                    continue;

                int r = strtol(code.substr(i + 3, 2).c_str(), nullptr, 16);
                int g = strtol(code.substr(i + 5, 2).c_str(), nullptr, 16);
                int b = strtol(code.substr(i + 7, 2).c_str(), nullptr, 16);
                int delay = strtol(code.substr(i + 9, 7).c_str(), nullptr, 16);

                if (delay > 0)
                    vTaskDelay(pdMS_TO_TICKS(delay));

                led_strip_set_pixel(led_strip, led_index, r, g, b);
            }

            led_strip_refresh(led_strip);

        } while (loop);
    }
};

#endif