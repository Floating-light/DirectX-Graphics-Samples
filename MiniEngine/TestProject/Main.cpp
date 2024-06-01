#include "pch.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "SystemTime.h"
#include "TextRenderer.h"
#include "GameInput.h"
#include "CommandContext.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "BufferManager.h"

// from project Core
#include "Math/BoundingBox.h"
#include "Camera.h"
#include "CameraController.h"
#include "SSAO.h"
#include "MotionBlur.h"
#include "TemporalEffects.h"
#include "FXAA.h"
#include "PostEffects.h"
#include "ShadowCamera.h"
#include "ParticleEffectManager.h"
#include "DepthOfField.h"
#include "Display.h"

// from peoject Model
#include "Renderer.h"
#include "ModelLoader.h"

// 
#include "ModelAssimp.h"
#include "MyModel.h"


using namespace GameCore;
using namespace Graphics;

class TestProject : public GameCore::IGameApp
{
public:

    TestProject()
    {
    }

    virtual void Startup( void ) override;
    virtual void Cleanup( void ) override;

    virtual void Update( float deltaT ) override;
    virtual void RenderScene( void ) override;

private:
    ModelInstance m_ModelInst;
    Math::Camera m_Camera;
    std::unique_ptr<CameraController> m_CameraController;
    ShadowCamera m_SunShadowCamera;

    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

};

//CREATE_APPLICATION( TestProject )
int __stdcall wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    return GameCore::RunApplication(TestProject(), L"TestProject", hInstance, nCmdShow);
}
#include <direct.h> // for _getcwd() to check data root path
void ChangeIBLSet(EngineVar::ActionType);
void ChangeIBLBias(EngineVar::ActionType);

DynamicEnumVar g_IBLSet("Viewer/Lighting/Environment", ChangeIBLSet);

std::vector<std::pair<TextureRef, TextureRef>> g_IBLTextures;
NumVar g_IBLBias("Viewer/Lighting/Gloss Reduction", 2.0f, 0.0f, 10.0f, 1.0f, ChangeIBLBias);
void ChangeIBLSet(EngineVar::ActionType)
{
    int setIdx = g_IBLSet - 1;
    if (setIdx < 0)
    {
        Renderer::SetIBLTextures(nullptr, nullptr);
    }
    else
    {
        auto texturePair = g_IBLTextures[setIdx];
        Renderer::SetIBLTextures(texturePair.first, texturePair.second);
    }
}

void ChangeIBLBias(EngineVar::ActionType)
{
    Renderer::SetIBLBias(g_IBLBias);
}
void LoadIBLTextures()
{
    char CWD[256];
    _getcwd(CWD, 256);

    Utility::Printf("Loading IBL environment maps\n");

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(L"Textures/*_diffuseIBL.dds", &ffd);

    g_IBLSet.AddEnum(L"None");

    if (hFind != INVALID_HANDLE_VALUE) do
    {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        std::wstring diffuseFile = ffd.cFileName;
        std::wstring baseFile = diffuseFile;
        baseFile.resize(baseFile.rfind(L"_diffuseIBL.dds"));
        std::wstring specularFile = baseFile + L"_specularIBL.dds";

        TextureRef diffuseTex = TextureManager::LoadDDSFromFile(L"Textures/" + diffuseFile);
        if (diffuseTex.IsValid())
        {
            TextureRef specularTex = TextureManager::LoadDDSFromFile(L"Textures/" + specularFile);
            if (specularTex.IsValid())
            {
                g_IBLSet.AddEnum(baseFile);
                g_IBLTextures.push_back(std::make_pair(diffuseTex, specularTex));
            }
        }
    } while (FindNextFile(hFind, &ffd) != 0);

    FindClose(hFind);

    Utility::Printf("Found %u IBL environment map sets\n", g_IBLTextures.size());

    if (g_IBLTextures.size() > 0)
        g_IBLSet.Increment();
}
void TestProject::Startup( void )
{
    if(1)
    {
        //std::string filePath = "HuangQuan/星穹铁道—黄泉（轴修复）.pmx";
        //std::wstring filePathW = L"HuangQuan/星穹铁道—黄泉（轴修复）.pmx";

        std::string filePath = "HeiTianE/星穹铁道—黑天鹅（优化）.pmx"; 
        std::wstring filePathW = L"HeiTianE/星穹铁道—黑天鹅（优化）.pmx"; 

        
        Renderer::ModelData offData;
        const std::wstring miniFileName = Utility::RemoveExtension(filePathW) + L".mini"; 
        AssimpModel AM;
        if (AM.Load(filePath))
        {
            const std::wstring basePathW = Utility::GetBasePath(filePathW);
            if (AM.BuildModel(offData, basePathW))
            {
                bool Succ = Renderer::SaveModel(miniFileName, offData);
                Utility::Printf("success : %d", false);
            }
        }

        std::shared_ptr<Renderer::ModelData> NewModel = FMyModel::LoadModel(filePath); 
        for (int i = 0; i < NewModel->m_Meshes.size(); ++i)
        {
            Mesh* MyMesh = NewModel->m_Meshes[i];
            Mesh* OldMesh = offData.m_Meshes[i];
            memcpy(MyMesh->bounds, OldMesh->bounds, 4 * sizeof(float));
            assert(MyMesh->draw[0].primCount == OldMesh->draw[0].primCount);
            assert(MyMesh->draw[0].baseVertex == OldMesh->draw[0].baseVertex);
            assert(MyMesh->draw[0].startIndex == OldMesh->draw[0].startIndex);
            assert(MyMesh->vbOffset == OldMesh->vbOffset);
            assert(MyMesh->vbSize == OldMesh->vbSize);
            //int Equi = memcmp(NewModel->m_GeometryData.data() + MyMesh->vbOffset, offData.m_GeometryData.data() + OldMesh->vbOffset, MyMesh->vbSize);
            //assert(Equi == 0);

            assert(MyMesh->vbDepthOffset== OldMesh->vbDepthOffset);
            assert(MyMesh->vbDepthSize == OldMesh->vbDepthSize);
            assert(MyMesh->ibOffset== OldMesh->ibOffset);
            assert(MyMesh->ibSize== OldMesh->ibSize);
            std::vector<uint16_t> Ind1(MyMesh->ibSize/ sizeof(uint16_t)); 
            std::vector<uint16_t> Ind2(MyMesh->ibSize/ sizeof(uint16_t)); 
            memcpy(Ind1.data(), NewModel->m_GeometryData.data() + MyMesh->ibOffset, MyMesh->ibSize);  
            memcpy(Ind2.data(), offData.m_GeometryData.data() + OldMesh->ibOffset + MyMesh->ibOffset, MyMesh->ibSize); 

            int Equi = memcmp(NewModel->m_GeometryData.data() + MyMesh->ibOffset, offData.m_GeometryData.data() + OldMesh->ibOffset, MyMesh->ibSize);
            assert(Equi == 0);
            assert(MyMesh->vbStride == OldMesh->vbStride);
            assert(MyMesh->ibFormat== OldMesh->ibFormat);

        }
        
        //bool Succ = Renderer::SaveModel(miniFileName, *NewModel);
        //Utility::Printf("success : %d", Succ);
    }
    MotionBlur::Enable = false;
    TemporalEffects::EnableTAA = false;
    FXAA::Enable = false;
    PostEffects::EnableHDR = false;
    PostEffects::EnableAdaptation = false;
    SSAO::Enable = true; 
    Renderer::Initialize(); 
    LoadIBLTextures();
    // Setup your data
    //m_ModelInst = Renderer::LoadModel(L"HuangQuan/exported/untitled.gltf", true); 
    
    //Math::OrientedBox obb = m_ModelInst.GetBoundingBox(); 
    //float modelRadius = Length(obb.GetDimensions()) * 0.5f;
    //const Math::Vector3 eye = obb.GetCenter() + Math::Vector3(modelRadius * 0.5f, 0.0f, 0.0f); 
    //m_Camera.SetEyeAtUp(eye, Math::Vector3(Math::kZero), Math::Vector3(Math::kYUnitVector));

    //m_Camera.SetZRange(1.0f, 10000.0f);
    //m_CameraController.reset(new FlyingFPSCamera(m_Camera, Math::Vector3(Math::kYUnitVector)));

    //m_ModelInst = Renderer::LoadModel(L"HuangQuan/exported/untitled22222.gltf", true);
    //m_ModelInst = Renderer::LoadModel(L"HuangQuan/星穹铁道—黄泉（轴修复）.gl", true);
    m_ModelInst = Renderer::LoadModel(L"HeiTianE/星穹铁道—黑天鹅（优化）.gl", true);
    m_ModelInst.LoopAllAnimations();
    m_ModelInst.Resize(10.0f);

    MotionBlur::Enable = false;
    m_Camera.SetZRange(1.0f, 10000.0f);
    m_CameraController.reset(new OrbitCamera(m_Camera, m_ModelInst.GetBoundingSphere(), Vector3(kYUnitVector)));


}

void TestProject::Cleanup( void )
{
    // Free up resources in an orderly fashion
    m_ModelInst = nullptr;
    Renderer::Shutdown(); 
}

void TestProject::Update( float deltaT )
{
    ScopedTimer _prof(L"Update State");

    m_CameraController->Update(deltaT);

    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Update");

    m_ModelInst.Update(gfxContext, deltaT);
    gfxContext.Finish();

    TemporalEffects::GetJitterOffset(m_MainViewport.TopLeftX, m_MainViewport.TopLeftY); 
    m_MainViewport.Width = (float)g_SceneColorBuffer.GetWidth(); 
    m_MainViewport.Height = (float)g_SceneColorBuffer.GetHeight(); 
    m_MainViewport.MinDepth = 0.0f;
    m_MainViewport.MaxDepth = 1.0f;

    m_MainScissor.left = 0; 
    m_MainScissor.top = 0; 
    m_MainScissor.right = (LONG)g_SceneColorBuffer.GetWidth(); 
    m_MainScissor.bottom = (LONG)g_SceneColorBuffer.GetHeight(); 
}

void TestProject::RenderScene( void )
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

    uint32_t FrameIndex = TemporalEffects::GetFrameIndexMod2(); 
    const D3D12_VIEWPORT& viewport = m_MainViewport;
    const D3D12_RECT& scissor = m_MainScissor;

    ParticleEffectManager::Update(gfxContext.GetComputeContext(), Graphics::GetFrameTime());
    
    Math::Vector3 SunDirection = Math::Vector3(0, 0, 1);
    m_SunShadowCamera.UpdateMatrix(-SunDirection, Vector3(0, -500.0f, 0), Vector3(5000, 3000, 3000),
        (uint32_t)g_ShadowBuffer.GetWidth(), (uint32_t)g_ShadowBuffer.GetHeight(), 16); 


    GlobalConstants globals; 
    globals.ViewProjMatrix = m_Camera.GetViewProjMatrix(); 
    globals.SunShadowMatrix = m_SunShadowCamera.GetShadowMatrix(); 
    globals.CameraPos = m_Camera.GetPosition();
    globals.SunDirection = SunDirection;
    globals.SunIntensity = Vector3(4.f);

    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);  
    gfxContext.ClearDepth(g_SceneDepthBuffer);

    Renderer::MeshSorter sorter(Renderer::MeshSorter::kDefault);
    sorter.SetCamera(m_Camera);
    sorter.SetViewport(viewport);
    sorter.SetScissor(scissor);
    sorter.SetDepthStencilTarget(g_SceneDepthBuffer);
    sorter.AddRenderTarget(g_SceneColorBuffer);

    m_ModelInst.Render(sorter);
    
    sorter.Sort();
    
    {
        ScopedTimer _prof(L"Depth Pre-Pass", gfxContext);
        sorter.RenderMeshes(Renderer::MeshSorter::kZPass, gfxContext, globals);
    }

    SSAO::Render(gfxContext, m_Camera);

    {
        ScopedTimer _outerprof(L"Main Render", gfxContext);
        {
            ScopedTimer _prof(L"Sun Shadow Map", gfxContext);

            Renderer::MeshSorter shadowSorter(Renderer::MeshSorter::kShadows); 
            shadowSorter.SetCamera(m_SunShadowCamera); 
            shadowSorter.SetDepthStencilTarget(g_ShadowBuffer); 

            m_ModelInst.Render(shadowSorter); 

            shadowSorter.Sort(); 
            shadowSorter.RenderMeshes(Renderer::MeshSorter::kZPass, gfxContext, globals); 
        }
        gfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        gfxContext.ClearColor(g_SceneColorBuffer);
        {
            ScopedTimer _prof(L"Render Color", gfxContext);

            gfxContext.TransitionResource(g_SSAOFullScreen, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ);
            gfxContext.SetRenderTarget(g_SceneColorBuffer.GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());
            gfxContext.SetViewportAndScissor(viewport, scissor);

            sorter.RenderMeshes(Renderer::MeshSorter::kOpaque, gfxContext, globals);

        }
        Renderer::DrawSkybox(gfxContext, m_Camera, viewport, scissor);
        sorter.RenderMeshes(Renderer::MeshSorter::kTransparent, gfxContext, globals);

    }
    MotionBlur::GenerateCameraVelocityBuffer(gfxContext, m_Camera, true);

    TemporalEffects::ResolveImage(gfxContext);

    ParticleEffectManager::Render(gfxContext, m_Camera, g_SceneColorBuffer, g_SceneDepthBuffer, g_LinearDepth[FrameIndex]);

    // Until I work out how to couple these two, it's "either-or".
    if (DepthOfField::Enable) 
        DepthOfField::Render(gfxContext, m_Camera.GetNearClip(), m_Camera.GetFarClip()); 
    else
        MotionBlur::RenderObjectBlur(gfxContext, g_VelocityBuffer); 
    gfxContext.Finish();
}
