#include "MyModel.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <utility>
#include <unordered_map>
#include <array>

#include "ModelLoader.h"
#include "TextureConvert.h"
#include "Math/BoundingSphere.h"

static void PrintMaterialProperty(aiMaterialProperty* InProperty)
{
    switch (InProperty->mType)
    {
    case aiPropertyTypeInfo::aiPTI_String:
    {
        assert(InProperty->mDataLength >= 5);
        // 前四个byte是字符串长度，中间是字符串，最后一个byte是\0
        unsigned int length = static_cast<unsigned int>(*reinterpret_cast<uint32_t*>(InProperty->mData)); 
        std::string res(InProperty->mData + 4, length); // 这里的length不包括\0 
        
        Utility::Printf("key: %s : %s, type %d, index %d\n", InProperty->mKey.C_Str(), res.c_str(), InProperty->mSemantic, InProperty->mIndex); 
        break;
    }
    case aiPropertyTypeInfo::aiPTI_Float : 
    case aiPropertyTypeInfo::aiPTI_Double :
    {
        const bool isFloat = InProperty->mType == aiPropertyTypeInfo::aiPTI_Float;
        uint32_t num = InProperty->mDataLength / (isFloat ? sizeof(float) : sizeof(double));
        Utility::Printf("key: %s : ", InProperty->mKey.C_Str());

        for (uint32_t i = 0; i < num; ++i)
        {
            double value = isFloat ? static_cast<double>(reinterpret_cast<float*>(InProperty->mData)[i]):
                static_cast<double>(reinterpret_cast<double*>(InProperty->mData)[i]);
            Utility::Printf("%f, ", value);
        }
        Utility::Printf("\n"); 
        break;
    }
    case aiPropertyTypeInfo::aiPTI_Integer:
    {
        uint32_t num = max(static_cast<uint32_t>(InProperty->mDataLength / sizeof(int32_t)), 1u);  
        
        Utility::Printf("key: %s : type %d, index %d", InProperty->mKey.C_Str(), InProperty->mSemantic, InProperty->mIndex);

        if (InProperty->mDataLength == 1)
        {
            int32_t Value = static_cast<int>(*InProperty->mData); 
            Utility::Printf("bool %d, ", Value); 
        }
        else
        {
            for (int i = 0; i < num; i++)
            {
                int32_t Value = static_cast<int>(reinterpret_cast<int32_t*>(InProperty->mData)[i]); 
                Utility::Printf("%d, ", Value);
            }
        }
        Utility::Printf("\n");
        break;
    }
    default:
        Utility::Printf("unknow key: %s : %d\n", InProperty->mKey.C_Str(), InProperty->mType);
        break;
    }
}

static void BuildMaterials(Renderer::ModelData& model, const aiScene* InScene, const std::string& basePath)
{
    assert(InScene->mNumMaterials > 0);
    model.m_MaterialConstants.reserve(InScene->mNumMaterials);
    for (int i = 0; i < InScene->mNumMaterials; ++i)
    {
        aiMaterial* inMat = InScene->mMaterials[i];
        Utility::Printf("========>> material %s\n", inMat->GetName().C_Str());
        
        Renderer::MaterialConstantData MatData;
        Renderer::MaterialTextureData TexData;
        aiColor3D diffuse(1.0f, 1.0f, 1.0f);
        aiColor3D ambient(1.0f, 1.0f, 1.0f);
        aiColor3D Spec(0,0,0);

        float metallic_factor = 1.f;
        float roughness_factor = 1.f;
        float opacity = 1.f; 
        float shipercent = 1.f;

        inMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse); 
        inMat->Get(AI_MATKEY_COLOR_SPECULAR, Spec); 
        inMat->Get(AI_MATKEY_COLOR_AMBIENT, ambient); 

        
        inMat->Get(AI_MATKEY_OPACITY, opacity);  
        inMat->Get(AI_MATKEY_SHININESS_STRENGTH, shipercent);
        
        inMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic_factor); 
        inMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness_factor); 

        int diffuse_uvwsrc = 0;
        inMat->Get(AI_MATKEY_UVWSRC_DIFFUSE(0), diffuse_uvwsrc); 

        MatData.baseColorFactor[0] = diffuse[0]; 
        MatData.baseColorFactor[1] = diffuse[1];
        MatData.baseColorFactor[2] = diffuse[2];
        MatData.baseColorFactor[3] = diffuse[3];
        
        MatData.emissiveFactor[0] = 0;
        MatData.emissiveFactor[1] = 0;
        MatData.emissiveFactor[2] = 0;

        MatData.normalTextureScale = 1.0f;
        MatData.metallicFactor = metallic_factor;
        MatData.roughnessFactor = roughness_factor;
        MatData.flags = 0x3C000000;
        // todo : alphatest : MatData.flags |= 0x60;

        TexData.stringIdx[kBaseColor] = 0xFFFF;
        TexData.stringIdx[kMetallicRoughness] = 0xFFFF;
        TexData.stringIdx[kOcclusion] = 0xFFFF;
        TexData.stringIdx[kEmissive] = 0xFFFF;
        TexData.stringIdx[kNormal] = 0xFFFF;
        TexData.addressModes = 0x55555;
        std::array<aiTextureType, 5> aiTexRemap = {aiTextureType_DIFFUSE, aiTextureType_METALNESS, aiTextureType_AMBIENT_OCCLUSION ,aiTextureType_EMISSIVE, aiTextureType_NORMALS};
        for (uint32_t i = kBaseColor; i < kNumTextures; ++i)
        {
            aiString texDiffuse; 
            aiReturn Ret = inMat->Get(AI_MATKEY_TEXTURE(aiTexRemap[i], 0), texDiffuse);
            if (Ret == aiReturn_SUCCESS)
            {
                std::string texName = texDiffuse.C_Str(); 
                auto it = std::find(model.m_TextureNames.begin(), model.m_TextureNames.end(), texName); 
                int index = std::distance(model.m_TextureNames.begin(), it);
                if ( it == model.m_TextureNames.end()) 
                {
                    model.m_TextureNames.push_back(texName);
                    // TODO: opacity ！= 1 开启alpha混合? 
                    model.m_TextureOptions.push_back(TextureOptions(false, false, true));
                }
                TexData.stringIdx[i] = index; 
            }
            else
            {
                TexData.stringIdx[i] = 0xFFFF;
            }
        }

        model.m_MaterialConstants.push_back(MatData);
        model.m_MaterialTextures.push_back(TexData);
        // debug 
        for (int i = 0; i < inMat->mNumProperties; ++i)
        {
            PrintMaterialProperty(inMat->mProperties[i]);
        }
    }

    assert(model.m_TextureOptions.size() == model.m_TextureNames.size());
    for (size_t i = 0; i  < model.m_TextureNames.size(); ++i)
    {
        std::string localPath = basePath + model.m_TextureNames[i];
        CompileTextureOnDemand(Utility::UTF8ToWideString(localPath), model.m_TextureOptions[i]); 
    }
}

void TraverseScene(aiNode* node, 
    std::vector<GraphNode>& sceneGraph, 
    Math::BoundingSphere& modelBSphere, 
    Math::AxisAlignedBox& modelBBox, 
    std::vector<Mesh*>& meshList,
    std::vector<byte>& bufferMemory, 
    uint32_t curPos, 
    const Math::Matrix4& xform)
{
    if (!node)
    {
        return;
    }
    GraphNode& NewNode = sceneGraph.emplace_back(); 
    NewNode.hasChildren = node->mNumChildren;
    NewNode.hasSibling= node->mParent && 
        (node->mParent->mNumChildren > 1) && 
        (node != node->mParent->mChildren[node->mParent->mNumChildren-1]); 
    NewNode.matrixIdx = curPos;
    NewNode.skeletonRoot = false; // TODO: ???

    aiVector3t<ai_real> scaling; 
    aiVector3t<ai_real> rotation;
    aiVector3t<ai_real> position;
    node->mTransformation.Decompose(scaling, rotation, position);

    NewNode.scale = Math::XMFLOAT3(scaling.x,scaling.y,scaling.z);
    NewNode.rotation = Math::Quaternion(rotation.y, rotation.z, rotation.x);
    assert(sizeof(node->mTransformation) == 16 * sizeof(float));
    std::memcpy((float*)&NewNode.xform, &(node->mTransformation), sizeof(node->mTransformation)); 

    const Math::Matrix4 WorldTransform = xform * NewNode.xform;

    // ProcessMesh
    for (int i = 0; i < node->mNumMeshes; ++i)
    {
        node->mMeshes[i];
    }

    for (int i = 0; i < node->mNumChildren; ++i)
    {
        TraverseScene(node->mChildren[i], sceneGraph, modelBSphere, modelBBox, meshList, bufferMemory, ++curPos, WorldTransform);
    }
}
static void PopulateMesh(const aiScene* Scene, Renderer::ModelData& model, std::vector<byte>& GeometryData) 
{
    for (int i = 0; i < Scene->mNumMeshes; ++i)
    {
        aiMesh* CurMesh = Scene->mMeshes[i];
        Mesh* NewMesh = new Mesh(); 
        model.m_Meshes.push_back(NewMesh);
        NewMesh->meshCBV = -1; // 
        NewMesh->materialCBV = CurMesh->mMaterialIndex;
        NewMesh->psoFlags = 0x00; 

        assert(CurMesh->mPrimitiveTypes & aiPrimitiveType::aiPrimitiveType_TRIANGLE); 
        assert(CurMesh->HasPositions());
        assert(CurMesh->HasNormals());
        
        NewMesh->psoFlags |= PSOFlags::kHasPosition | PSOFlags::kHasNormal;
        std::vector<byte> byteStream; 
        for (int vi = 0; vi = CurMesh->mNumVertices; vi++)
        {
            
        }
        if(NewMesh)
        CurMesh->GetNumUVChannels();
        std::vector<> Vdata;
        CurMesh->mNumVertices;
        CurMesh->mNumFaces;
        CurMesh->mFaces;
    }
}
static void BuildMesh(Renderer::ModelData& model, const aiScene* InScene, const std::string& basePath)
{
    aiNode* node = InScene->mRootNode;
    model.m_BoundingSphere = Math::BoundingSphere(Math::kZero);
    model.m_BoundingBox = Math::AxisAlignedBox(Math::kZero);
    TraverseScene(node, model.m_SceneGraph, model.m_BoundingSphere, model.m_BoundingBox, model.m_Meshes, model.m_GeometryData, 0, Math::Matrix4(Math::kIdentity));
    
}

std::shared_ptr<FMyModel> FMyModel::LoadModel(const std::string& InPath)
{
    Renderer::ModelData MyModel;
    Assimp::Importer importer;
    std::string basePath = Utility::GetBasePath(InPath);
    // 移除不要的数据
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, aiComponent_COLORS | aiComponent_LIGHTS | aiComponent_CAMERAS);
    
    // 单个Mesh最大triangle和vertices，超过这个可以分离, "SplitLargeMeshes"
    importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, INT_MAX);
    importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, 0xfffe);

    // 移除点、线
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

    const aiScene* scene = importer.ReadFile(InPath, 
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices |
        aiProcess_Triangulate |
        aiProcess_RemoveComponent |
        aiProcess_GenSmoothNormals |
        aiProcess_SplitLargeMeshes |
        aiProcess_ValidateDataStructure |
        aiProcess_ImproveCacheLocality | //we do not use  optimizePostTransform()
        aiProcess_RemoveRedundantMaterials |
        aiProcess_SortByPType |
        aiProcess_FindInvalidData |
        aiProcess_GenUVCoords |
        aiProcess_TransformUVCoords |
        aiProcess_OptimizeMeshes |
        aiProcess_OptimizeGraph);

    if (scene == nullptr)
    {
        return  nullptr;
    }
    if (scene->HasTextures())
    {
        assert(0 && "Has texture in scene");
    }
    if (scene->HasAnimations())
    {
        assert(0 && "TODO: animations");
    }
    BuildMaterials(MyModel, scene, basePath);

    BuildMesh(MyModel, scene, basePath); 


    return std::shared_ptr<FMyModel>(); 
}
