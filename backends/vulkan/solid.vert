#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec4 color;
layout(location=2) in vec3 inverseX;
layout(location=6) in vec3 inverseY;
layout(location=3) in vec4 bounds;
layout(location=4) in vec2 rounding;
layout(location=5) in vec4 border;
layout(location=0) out vec4 tint;
layout(location=1) flat out vec3 localX;
layout(location=5) flat out vec3 localY;
layout(location=2) flat out vec4 box;
layout(location=3) flat out vec2 rounded;
layout(location=4) flat out vec4 borderTint;
layout(push_constant) uniform View { vec2 extent; } view;
void main() {
    gl_Position = vec4(position / view.extent * 2.0 - 1.0, 0.0, 1.0);
    tint = color; localX=inverseX; localY=inverseY; box=bounds; rounded=rounding; borderTint=border;
}
