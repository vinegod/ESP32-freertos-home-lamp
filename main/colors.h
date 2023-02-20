#ifndef COLORS_H_
#define COLORS_H_

#include <stdint.h>

/**
 * @brief struct with RGB configuration
 *
 * @note can be accessed via color array or color_rgb struct
 */
typedef struct
{
    union {
        uint16_t color[3];
        struct
        {
            uint16_t red, green, blue;
        } color_rgb;
    };
} rgb_color_t;

/**
 * @brief enum that represents colors
 */
typedef enum
{
    color_RED,
    color_BLUE,
    color_GREEN,
    color_WHITE,
    color_WARM_WHITE
} color_e;

/**
 * @brief convert colors from enum_e to their rgb_color_t representation
 *
 * @param color color that is available in enum
 * @return rgb_color_t color representation
 */
rgb_color_t color_to_rgb_struct(color_e color);

#endif /* COLORS_H_ */
