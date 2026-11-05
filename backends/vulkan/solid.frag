#version 450
layout(location=0) in vec4 tint;
layout(location=1) flat in vec3 localX;
layout(location=5) flat in vec3 localY;
layout(location=6) flat in vec4 texels;
layout(set=0,binding=0) uniform sampler2D atlas;
layout(location=2) flat in vec4 box;
layout(location=3) flat in vec2 rounded;
layout(location=4) flat in vec4 borderTint;
layout(location=0) out vec4 color;
bool inside(vec2 p, vec4 b, float radius) {
    if (any(lessThanEqual(b.zw,vec2(0))) || any(lessThan(p,b.xy)) || any(greaterThanEqual(p,b.xy+b.zw))) return false;
    float r=min(radius,min(b.z,b.w)*0.5);
    return distance(p,clamp(p,b.xy+vec2(r),b.xy+b.zw-vec2(r)))<=r;
}
layout(push_constant) uniform View { vec2 extent; float srgb; float pad; } view;
void shade() {
    color=tint;
    if(rounded.x==-3) {
        vec2 d=texels.zw-texels.xy;
        float t=clamp(dot(gl_FragCoord.xy-texels.xy,d)/max(1e-12,dot(d,d)),0,1);
        color=floor(mix(tint,borderTint,t)*255.0+0.0001)/255.0;
        color.a=floor(color.a*rounded.y*255.0+0.0001)/255.0;return;
    }
    if(rounded.x==-4) {
        vec2 d=texels.zw-texels.xy;
        float t=clamp(dot(gl_FragCoord.xy-texels.xy,d)/max(1e-12,dot(d,d)),0,1);
        if(distance(gl_FragCoord.xy,texels.xy+t*d)>rounded.y) discard;
        return;
    }
    if(rounded.x>=0 || rounded.x==-2) {
        precise vec2 localPoint=vec2(
            localX.x*gl_FragCoord.x+localX.y*gl_FragCoord.y+localX.z,
            localY.x*gl_FragCoord.x+localY.y*gl_FragCoord.y+localY.z);
        if(rounded.x==-2) {
            ivec2 pixel=ivec2(clamp(floor((localPoint-box.xy)/box.zw*texels.zw),vec2(0),texels.zw-1));
            vec4 sampled=texelFetch(atlas,ivec2(texels.xy)+pixel,0);
            color=vec4(tint.rgb*sampled.rgb,floor(tint.a*sampled.a*255.0+0.0001)/255.0);
            return;
        }
        if(!inside(localPoint,box,rounded.x)) discard;
        vec4 inner=vec4(box.xy+vec2(rounded.y),max(vec2(0),box.zw-vec2(2*rounded.y)));
        if(rounded.y>0 && !inside(localPoint,inner,max(0,rounded.x-rounded.y))) color=borderTint;
    }
}

void main() {
    shade();
    if(view.srgb>0.5) color.rgb=mix(color.rgb/12.92,pow((color.rgb+0.055)/1.055,vec3(2.4)),greaterThan(color.rgb,vec3(0.04045)));
}
