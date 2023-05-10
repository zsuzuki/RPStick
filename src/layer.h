#include <Arduino.h>
#include <vector>

struct Layer
{
    struct Key
    {
        uint8_t code;
        uint8_t mod;
        uint8_t str;
    };
    Key A_;
    Key B_;
    Key C_;
    //
    uint8_t r_;
    uint8_t g_;
    uint8_t b_;
    //
    const char *caption0_;
    const char *caption1_;
};

using LayerList = std::vector<Layer>;
