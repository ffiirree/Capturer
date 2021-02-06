#ifndef CANVAS_H
#define CANVAS_H

#include <cstdint>

class Canvas
{
public:
    enum EditStatus: std::uint32_t {
        NONE            = 0x0000'0000,
        READY           = 0x0001'0000,
        GRAPH_PAINTING  = 0x0010'0000,
        GRAPH_MOVING    = 0x0020'0000,
        GRAPH_RESIZING  = 0x0040'0000,
        GRAPH_ROTATING  = 0x0080'0000,

        READY_MASK      = 0x000f'0000,
        OPERATION_MASK  = 0xfff0'0000,
        GRAPH_MASK      = 0x0000'ffff
    };
public:
    Canvas();
};

#endif // CANVAS_H
