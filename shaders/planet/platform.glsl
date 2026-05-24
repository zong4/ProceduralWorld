#define SHADERTOY

#ifdef __cplusplus
#define _in(T) const T &
#define _inout(T) T &
#define _out(T) T &
#define _begin(type) type {
#define _end }
#define _mutable(T) T
#define _constant(T) const T
#define mul(a, b) (a) * (b)
#endif

#if defined(GL_ES) || defined(GL_SHADING_LANGUAGE_VERSION)
#define _in(T) const in T
#define _inout(T) inout T
#define _out(T) out T
#define _begin(type) type (
#define _end )
#define _mutable(T) T
#define _constant(T) const T
#define mul(a, b) (a) * (b)
#endif

#ifdef HLSL
#define _in(T) const in T
#define _inout(T) inout T
#define _out(T) out T
#define _begin(type) {
#define _end }
#define _mutable(T) static T
#define _constant(T) static const T
#define vec2 float2
#define vec3 float3
#define vec4 float4
#define mat2 float2x2
#define mat3 float3x3
#define mat4 float4x4
#define mix lerp
#define fract frac
#define mod fmod
#pragma pack_matrix(row_major)
#endif

#ifdef HLSLTOY
cbuffer uniforms : register(b0) {
	float2 u_res;
	float u_time;
	float2 u_mouse;
};
void mainImage(_out(float4) fragColor, _in(float2) fragCoord);
float4 main(float4 uv : SV_Position) : SV_Target{ float4 col; mainImage(col, uv.xy); return col; }
#endif

#if defined(__cplusplus) || defined(SHADERTOY)
#define u_res iResolution
#define u_time iTime
#define u_mouse iMouse
#endif

#ifdef GLSLSANDBOX
uniform float time;
uniform vec2 mouse;
uniform vec2 resolution;
#define u_res resolution
#define u_time time
#define u_mouse mouse
void mainImage(_out(vec4) fragColor, _in(vec2) fragCoord);
void main() { mainImage(gl_FragColor, gl_FragCoord.xy); }
#endif

#ifdef UE4
_constant(vec2) u_res = vec2(0, 0);
_constant(vec2) u_mouse = vec2(0, 0);
_mutable(float) u_time = 0;
#endif
