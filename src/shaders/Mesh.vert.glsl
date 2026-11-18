#version 460

#extension GL_GOOGLE_include_directive: require
#extension GL_EXT_buffer_reference: require

#include "InputStructures.glsl"


layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outColor;
layout(location = 2) out vec2 outUV;

struct Vertex {
    vec4 color;
    vec3 position;
    float uvX;
    vec3 normal;
    float uvY;
};


layout(buffer_reference, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(push_constant) uniform PushConstant
{
    mat4 renderMat;
    VertexBuffer vertexBuffer;
} pushConstant;


void main()
{
    Vertex vert = pushConstant.vertexBuffer.vertices[gl_VertexIndex];
    vec4 position = vec4(vert.position, 1.0f);

    gl_Position = sceneData.viewProj * pushConstant.renderMat * position;

    outNormal = (pushConstant.renderMat * vec4(vert.normal, 0.0f)).xyz;
    outColor = vert.color.xyz * materialData.colorFactors.xyz;
    outUV.x = vert.uvX;
    outUV.y = vert.uvY;

}
