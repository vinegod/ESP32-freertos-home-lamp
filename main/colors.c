#include "colors.h"

rgb_color_t color_to_rgb_struct(color_e color)
{
    switch (color)
    {
    case color_RED: {
        rgb_color_t _color = {.color_rgb.red = 255, .color_rgb.green = 0, .color_rgb.blue = 0};
        return _color;
    }

    case color_GREEN: {
        rgb_color_t _color = {.color_rgb.red = 0, .color_rgb.green = 255, .color_rgb.blue = 0};
        return _color;
    }

    case color_BLUE: {
        rgb_color_t _color = {.color_rgb.red = 0, .color_rgb.green = 0, .color_rgb.blue = 255};
        return _color;
    }

    case color_WARM_WHITE: {
        rgb_color_t _color = {.color_rgb.red = 253, .color_rgb.green = 227, .color_rgb.blue = 108};
        return _color;
    }

    case color_WHITE: {
        rgb_color_t _color = {.color_rgb.red = 255, .color_rgb.green = 255, .color_rgb.blue = 255};
        return _color;
    }

    default: {
        rgb_color_t _color = {.color_rgb.red = 255, .color_rgb.green = 255, .color_rgb.blue = 255};
        return _color;
    }
    }
}
