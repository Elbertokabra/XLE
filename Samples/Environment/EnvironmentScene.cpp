// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "EnvironmentScene.h"
#include "../Shared/CharactersScene.h"
#include "../Shared/SampleGlobals.h"

#include "../../RenderCore/RenderUtils.h"
#include "../../RenderCore/Metal/State.h"
#include "../../RenderCore/Assets/TerrainFormat.h"

#include "../../SceneEngine/LightDesc.h"
#include "../../SceneEngine/LightingParserContext.h"
#include "../../SceneEngine/Terrain.h"
#include "../../SceneEngine/SceneEngineUtility.h"

#include "../../PlatformRig/PlatformRigUtil.h"

#include "../../ConsoleRig/Console.h"
#include "../../Math/Transformations.h"
#include "../../Utility/Streams/PathUtils.h"

namespace SceneEngine { extern float SunDirectionAngle; }

namespace Sample
{
    static const char* WorldDirectory = "game/demworld";

    std::shared_ptr<SceneEngine::ITerrainFormat>     MainTerrainFormat;
    SceneEngine::TerrainCoordinateSystem             MainTerrainCoords;
    SceneEngine::TerrainConfig                       MainTerrainConfig;

    class EnvironmentSceneParser::Pimpl
    {
    public:
        std::unique_ptr<CharactersScene>                _characters;
        std::shared_ptr<SceneEngine::TerrainManager>    _terrainManager;
        std::shared_ptr<RenderCore::CameraDesc>         _cameraDesc;

        float _time;
    };

///////////////////////////////////////////////////////////////////////////////////////////////////

    void EnvironmentSceneParser::PrepareFrame(RenderCore::Metal::DeviceContext* context) 
    {
        RenderCore::Metal::ViewportDesc viewport(*context);
        auto sceneCamera = GetCameraDesc();
        auto projectionMatrix = RenderCore::PerspectiveProjection(
            sceneCamera._verticalFieldOfView, viewport.Width / float(viewport.Height),
            sceneCamera._nearClip, sceneCamera._farClip, RenderCore::GeometricCoordinateSpace::RightHanded, 
            #if (GFXAPI_ACTIVE == GFXAPI_DX11) || (GFXAPI_ACTIVE == GFXAPI_DX9)
                RenderCore::ClipSpaceType::Positive);
            #else
                RenderCore::ClipSpaceType::StraddlingZero);
            #endif
        auto worldToProjection = Combine(InvertOrthonormalTransform(sceneCamera._cameraToWorld), projectionMatrix);

        _pimpl->_characters->Cull(worldToProjection);
        _pimpl->_characters->Prepare(context);
    }

    void EnvironmentSceneParser::ExecuteScene(   
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned techniqueIndex) const
    {
        if (    parseSettings._batchFilter == SceneParseSettings::BatchFilter::General
            ||  parseSettings._batchFilter == SceneParseSettings::BatchFilter::Depth) {

            #if defined(ENABLE_TERRAIN)
                if (parseSettings._toggles & SceneParseSettings::Toggles::Terrain) {
                    if (Tweakable("DoTerrain", true)) {
                        _pimpl->_terrainManager->Render(context, parserContext, techniqueIndex);
                    }
                }
            #endif
            
            if (parseSettings._toggles & SceneParseSettings::Toggles::NonTerrain) {
                _pimpl->_characters->Render(context, parserContext, techniqueIndex);
            }

        }
    }

    void EnvironmentSceneParser::ExecuteShadowScene( 
        RenderCore::Metal::DeviceContext* context, 
        LightingParserContext& parserContext, 
        const SceneParseSettings& parseSettings,
        unsigned frustumIndex, unsigned techniqueIndex) const 
    {
        SceneParseSettings settings = parseSettings;
        settings._toggles &= ~SceneParseSettings::Toggles::Terrain;
        ExecuteScene(context, parserContext, settings, techniqueIndex);
    }

    RenderCore::CameraDesc EnvironmentSceneParser::GetCameraDesc() const 
    { 
        return *_pimpl->_cameraDesc;
    }

    unsigned EnvironmentSceneParser::GetLightCount() const 
    {
        return 1; 
    }

    auto EnvironmentSceneParser::GetLightDesc(unsigned index) const -> const LightDesc&
    { 
            //  This method just returns a properties for the lights in the scene. 
            //
            //  The lighting parser will take care of the actual lighting calculations 
            //  required. All we have to do is return the properties of the lights
            //  we want.
        static LightDesc dummy;
        dummy._radius = 10000.f;
        dummy._isDynamicLight = false;
        dummy._isPointLight = false;
        dummy._shadowFrustumIndex = 0;
        dummy._lightColour = Float3(1.f, 1.f, 1.f);

            // sun direction based on angle in the sky
        Float2 sunDirectionOfMovement = Normalize(Float2(1.f, 0.33f));
        Float2 sunRotationAxis(-sunDirectionOfMovement[1], sunDirectionOfMovement[0]);
        dummy._negativeLightDirection = 
            Normalize(TransformDirectionVector(
                MakeRotationMatrix(Expand(sunRotationAxis, 0.f), SceneEngine::SunDirectionAngle), Float3(0.f, 0.f, 1.f)));

        return dummy;
    }

    auto EnvironmentSceneParser::GetGlobalLightingDesc() const -> GlobalLightingDesc
    { 
            //  There are some "global" lighting parameters that apply to
            //  the entire rendered scene 
            //      (or, at least, to one area of the scene -- eg, indoors/outdoors)
            //  Here, we can fill in these properties.
            //
            //  Note that the scene parser "desc" functions can be called multiple
            //  times in a single frame. Generally the properties can be animated in
            //  any way, but they should stay constant over the course of a single frame.
        GlobalLightingDesc result;
        auto ambientScale = Tweakable("AmbientScale", 0.075f);
        result._ambientLight = Float3(.65f * ambientScale, .7f * ambientScale, 1.f * ambientScale);
        result._skyTexture = "game/xleres/DefaultResources/sky/desertsky.jpg";
        result._doToneMap = true;
        return result;
    }

    unsigned EnvironmentSceneParser::GetShadowFrustumCount() const
    { 
        return 1; 
    }

    auto EnvironmentSceneParser::GetShadowFrustumDesc(unsigned index) const -> const ShadowFrustumDesc&
    { 
            //  Shadowing lights can have a ShadowFrustumDesc object associated.
            //  This object determines the shadow "projections" or "cascades" we use 
            //  for calculating shadows.
            //
            //  Normally, we want multiple shadow cascades per light. There are a few
            //  different methods for deciding on the cascades for a scene.
            //
            //  In this case, we're just using a default implementation -- this
            //  implementation is very basic. The results are ok, but not optimal.
            //  Specialised scenes may some specialised algorithm for calculating shadow
            //  cascades.
        static ShadowFrustumDesc result[1];
        if (index >= dimof(result)) {
            throw Exceptions::BasicLabel("Bad shadow frustum index");
        }

        result[index] = PlatformRig::CalculateDefaultShadowFrustums(
            GetLightDesc(index), GetCameraDesc());
        return result[index];
    }

    float EnvironmentSceneParser::GetTimeValue() const      
    { 
            //  The scene parser can also provide a time value, in seconds.
            //  This is used to control rendering effects, such as wind
            //  and waves.
        return _pimpl->_time; 
    }

    void EnvironmentSceneParser::Update(float deltaTime)    
    {
        _pimpl->_characters->Update(deltaTime);
        _pimpl->_time += deltaTime; 
    }

    std::shared_ptr<PlayerCharacter>  EnvironmentSceneParser::GetPlayerCharacter()
    {
        return _pimpl->_characters->GetPlayerCharacter();
    }

    std::shared_ptr<SceneEngine::TerrainManager> EnvironmentSceneParser::GetTerrainManager()
    {
        return _pimpl->_terrainManager;
    }

    std::shared_ptr<RenderCore::CameraDesc> EnvironmentSceneParser::GetCameraPtr()
    {
        return _pimpl->_cameraDesc;
    }

    EnvironmentSceneParser::EnvironmentSceneParser()
    {
        auto pimpl = std::make_unique<Pimpl>();
        pimpl->_time = 0.f;
        pimpl->_characters = std::make_unique<CharactersScene>();

        #if defined(ENABLE_TERRAIN)
            MainTerrainFormat = std::make_shared<RenderCore::Assets::TerrainFormat>();
            MainTerrainConfig = SceneEngine::TerrainConfig(WorldDirectory);
            pimpl->_terrainManager = std::make_shared<SceneEngine::TerrainManager>(
                MainTerrainConfig, MainTerrainFormat, 
                SceneEngine::GetBufferUploads(), Int2(0, 0), MainTerrainConfig._cellCount,
                Float2(-11200.f - 7000.f, -11200.f + 700.f));
            MainTerrainCoords = pimpl->_terrainManager->GetCoords();
        #endif

        pimpl->_cameraDesc = std::make_shared<RenderCore::CameraDesc>();
        pimpl->_cameraDesc->_cameraToWorld = pimpl->_characters->DefaultCameraToWorld();
        pimpl->_cameraDesc->_nearClip = 0.5f;
        pimpl->_cameraDesc->_farClip = 4000.f; // 32000.f;

        _pimpl = std::move(pimpl);
    }

    EnvironmentSceneParser::~EnvironmentSceneParser()
    {}


}
