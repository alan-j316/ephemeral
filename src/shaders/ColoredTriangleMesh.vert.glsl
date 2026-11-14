#version 460
#extension GL_EXT_buffer_reference: require

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outUV;

struct Vertex
{
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


// Push constants
layout(push_constant) uniform constants
{
    mat4 renderMatrix;
    VertexBuffer vertexBuffer;
} PushConstants;


void main()
{
    // Load the vertex data form device address
    Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];

    gl_Position = PushConstants.renderMatrix * vec4(v.position, 1.0f);
        outColor = v.color.xyz;
//    outColor = vec3(1.0f, 0.0f, 0.5f);
    outUV.x = v.uvX;
    outUV.y = v.uvY;
}
