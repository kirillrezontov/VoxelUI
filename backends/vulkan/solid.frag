#version 450
layout(location=0) in vec4 tint;
layout(location=1) flat in vec3 localX;
layout(location=5) flat in vec3 localY;
layout(location=2) flat in vec4 box;
layout(location=3) flat in vec2 rounded;
layout(location=4) flat in vec4 borderTint;
layout(location=0) out vec4 color;
bool inside(vec2 p, vec4 b, float radius) {
    if (any(lessThanEqual(b.zw,vec2(0))) || any(lessThan(p,b.xy)) || any(greaterThanEqual(p,b.xy+b.zw))) return false;
    float r=min(radius,min(b.z,b.w)*0.5);
    return distance(p,clamp(p,b.xy+vec2(r),b.xy+b.zw-vec2(r)))<=r;
}
void main() {
    color=tint;
    if(rounded.x>=0) {
        precise vec2 localPoint=vec2(
            localX.x*gl_FragCoord.x+localX.y*gl_FragCoord.y+localX.z,
            localY.x*gl_FragCoord.x+localY.y*gl_FragCoord.y+localY.z);
        if(!inside(localPoint,box,rounded.x)) discard;
        vec4 inner=vec4(box.xy+vec2(rounded.y),max(vec2(0),box.zw-vec2(2*rounded.y)));
        if(rounded.y>0 && !inside(localPoint,inner,max(0,rounded.x-rounded.y))) color=borderTint;
    }
}
