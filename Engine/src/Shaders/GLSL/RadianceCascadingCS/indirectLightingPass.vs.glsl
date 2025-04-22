#version 450 core

#extension GL_ARB_bindless_texture : require
#extension GL_ARB_gpu_shader_int64 : require // Needed for uint64_t
#extension GL_ARB_shader_storage_buffer_object : require


layout(location = 0) in vec3 a_Position;
layout(location = 2) in vec2 a_TexCoord;

precision highp float;


out vec2 TexCoord;

uniform mat4 u_model;



void main() {
    TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
    gl_Position =  u_model * vec4(a_Position, 1.0);
}
