#ifndef CANVAS_H
#define CANVAS_H

#include <cstdint>

class Canvas
{
public:
    enum EditStatus: std::uint32_t {
        NONE            = 0x00000000,
        READY           = 0x00010000,
        GRAPH_PAINTING  = 0x00100000,
        GRAPH_MOVING    = 0x00200000,
        GRAPH_RESIZING  = 0x00400000,
        GRAPH_ROTATING  = 0x00800000,

        READY_MASK      = 0x000f0000,
        OPERATION_MASK  = 0xfff00000,
        GRAPH_MASK      = 0x0000ffff
    };
public:
    Canvas();
};

#endif // CANVAS_H
