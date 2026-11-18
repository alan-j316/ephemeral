/**
 * @file RenderPrimitives.cpp
 * @author Alan Abraham P Kochumon
 * @date Created on: November 18, 2026
 *
 * @brief Implementation of member functions declared in RenderPrimitives.h
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include "RenderPrimitives.h"

#include "VkLoader.h"

namespace ephemeral
{
    void MeshNode::draw(const flcn::Mat4F& topMatrix, DrawContext& ctx) noexcept
    {
        const flcn::Mat4F nodeMat = topMatrix * worldTransform;

        // Construct a render object for each surface and push it to the draw context
        for (auto& [startIndex, count, material] : mesh->surfaces)
        {
            RenderObject obj;
            obj.firstIndex  = startIndex;
            obj.indexCount  = count;
            obj.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
            obj.material    = &material->material;

            obj.transform           = nodeMat;
            obj.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

            ctx.renderables.push_back(obj);
        }

        Node::draw(topMatrix, ctx);
    }

} // namespace ephemeral
