//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

#pragma once

#include "Model.h"
#include "Animation.h"
#include "ConstantBuffers.h"
#include "../Core/Math/BoundingSphere.h"
#include "../Core/Math/BoundingBox.h"

#include <cstdint>
#include <vector>

namespace glTF { class Asset; struct Mesh; }

#define CURRENT_MINI_FILE_VERSION 13

namespace Renderer
{
    using namespace Math;

    // Unaligned mirror of MaterialConstants
    struct MaterialConstantData
    {
        float baseColorFactor[4]; // default=[1,1,1,1]
        float emissiveFactor[3]; // default=[0,0,0]
        float normalTextureScale; // default=1
        float metallicFactor; // default=1
        float roughnessFactor; // default=1
        uint32_t flags;
    };

    // Used at load time to construct descriptor tables
    struct MaterialTextureData
    {
        uint16_t stringIdx[kNumTextures];
        uint32_t addressModes;
    };

    // All of the information that needs to be written to a .mini data file
    struct ModelData
    {
        BoundingSphere m_BoundingSphere;
        AxisAlignedBox m_BoundingBox;// 这些Bound, 在遍历输入文件的所有Node的时候累计
        std::vector<byte> m_GeometryData;//用这一个Buffer存所有vertex，index Buffer.
        std::vector<byte> m_AnimationKeyFrameData;
        std::vector<AnimationCurve> m_AnimationCurves;
        std::vector<AnimationSet> m_Animations;
        std::vector<uint16_t> m_JointIndices;
        std::vector<Matrix4> m_JointIBMs;
                                                             
        std::vector<Mesh*> m_Meshes;
        std::vector<GraphNode> m_SceneGraph;// 存Transform(Local Transform), 材质id等信息，与Mesh一一对应,

        // 材质constants参数, baseColorFactor ...
        std::vector<MaterialConstantData> m_MaterialConstants; 
        // 对应材质的textures , 一个TextureData里面多个texture, 对应texture的值是下面"m_TextureNames"的index，没有就是 0xFFFF
        // addressModes 是采样器的flag， 用一个uint32记下了所有对应texture的采样参数
        std::vector<MaterialTextureData> m_MaterialTextures; 
        std::vector<std::string> m_TextureNames; // texture 的相对path(文件名)
        std::vector<uint8_t> m_TextureOptions; // 每个texture对应的纹理选项:TexConversionFlags， 没有就是0xFF
    };

    struct FileHeader
    {
        char     id[4];   // "MINI"
        uint32_t version; // CURRENT_MINI_FILE_VERSION
        uint32_t numNodes;
        uint32_t numMeshes;
        uint32_t numMaterials;
        uint32_t meshDataSize;
        uint32_t numTextures;
        uint32_t stringTableSize;
        uint32_t geometrySize;
        uint32_t keyFrameDataSize;      // Animation data
        uint32_t numAnimationCurves;
        uint32_t numAnimations;
        uint32_t numJoints;     // All joints for all skins
        float    boundingSphere[4];
        float    minPos[3];
        float    maxPos[3];
    };

    void CompileMesh(
        std::vector<Mesh*>& meshList,
        std::vector<byte>& bufferMemory,
        glTF::Mesh& srcMesh,
        uint32_t matrixIdx,
        const Matrix4& localToObject,
        Math::BoundingSphere& boundingSphere,
        Math::AxisAlignedBox& boundingBox
    );

    bool BuildModel( ModelData& model, const glTF::Asset& asset, int sceneIdx = -1 );
    bool SaveModel( const std::wstring& filePath, const ModelData& model );
    
    std::shared_ptr<Model> LoadModel( const std::wstring& filePath, bool forceRebuild = false );
}