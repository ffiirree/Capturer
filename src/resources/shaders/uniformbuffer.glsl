layout(std140, binding = 0) uniform buf {
    mat4 mvp;   // Model View Projection Matrix
    mat4 M;     // Color Convert Matrix
    vec4 color; // libass color
} ubuf;