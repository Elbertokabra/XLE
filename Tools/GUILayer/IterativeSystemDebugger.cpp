// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#include "IterativeSystemDebugger.h"
#include "IOverlaySystem.h"
#include "ManipulatorUtils.h"       // for IGetAndSetProperties
#include "MarshalString.h"
#include "EngineDevice.h"
#include "NativeEngineDevice.h"
#include "../../SceneEngine/Erosion.h"
#include "../../SceneEngine/LightingParser.h"
#include "../../SceneEngine/TerrainUberSurface.h"
#include "../../RenderCore/Metal/DeviceContext.h"
#include "../../RenderCore/IDevice.h"
#include "../../BufferUploads/ResourceLocator.h"
#include "../../Math/Transformations.h"
#include "../../Assets/AssetsCore.h"
#include "../../Utility/Meta/ClassAccessors.h"
#include "../../Utility/MemoryUtils.h"
#include <memory>

namespace GUILayer
{
    using ErosionSettings = SceneEngine::ErosionSimulation::Settings;

////////////////////////////////////////////////////////////////////////////////////////////////////

    ref class ErosionOverlay : public IOverlaySystem
    {
    public:
        virtual void RenderToScene(
            RenderCore::IThreadContext* device, 
            SceneEngine::LightingParserContext& parserContext) override;
        virtual void RenderWidgets(
            RenderCore::IThreadContext* device, 
            const RenderCore::Techniques::ProjectionDesc& projectionDesc) override;
        virtual void SetActivationState(bool newState) override {}

        ErosionOverlay(
            std::shared_ptr<SceneEngine::ErosionSimulation> sim,
            ErosionIterativeSystem::Settings^ previewSettings);
        !ErosionOverlay();
        ~ErosionOverlay();
    private:
        clix::shared_ptr<SceneEngine::ErosionSimulation> _sim;
        ErosionIterativeSystem::Settings^ _previewSettings;
    };

    static SceneEngine::ErosionSimulation::RenderDebugMode AsDebugMode(ErosionIterativeSystem::Settings::Preview input)
    {
        using P = ErosionIterativeSystem::Settings::Preview;
        switch (input) {
        default:
        case P::WaterVelocity: return SceneEngine::ErosionSimulation::RenderDebugMode::WaterVelocity3D;
        case P::HardMaterials: return SceneEngine::ErosionSimulation::RenderDebugMode::HardMaterials;
        case P::SoftMaterials: return SceneEngine::ErosionSimulation::RenderDebugMode::SoftMaterials;
        }
    }

    void ErosionOverlay::RenderToScene(
        RenderCore::IThreadContext* device,
        SceneEngine::LightingParserContext& parserContext)
    {
        auto metalContext = RenderCore::Metal::DeviceContext::Get(*device);
        Float2 worldDims = _sim->GetDimensions() * _sim->GetWorldSpaceSpacing();

        auto camToWorld = MakeCameraToWorld(
            Float3(0.f, 0.f, -1.f),
            Float3(0.f, 1.f, 0.f),
            Float3(0.f, 0.f, 0.f));
        SceneEngine::LightingParser_SetGlobalTransform(
            metalContext.get(), parserContext, camToWorld, 
            0.f, 0.f, worldDims[0], worldDims[1], 
            -4096.f, 4096.f);

        _sim->RenderDebugging(*metalContext, parserContext, AsDebugMode(_previewSettings->ActivePreview));
    }

    void ErosionOverlay::RenderWidgets(
        RenderCore::IThreadContext* device,
        const RenderCore::Techniques::ProjectionDesc& projectionDesc)
    {}

    ErosionOverlay::ErosionOverlay(
        std::shared_ptr<SceneEngine::ErosionSimulation> sim,
        ErosionIterativeSystem::Settings^ previewSettings)
    : _sim(sim), _previewSettings(previewSettings)
    {}

    ErosionOverlay::!ErosionOverlay() { _sim.reset(); }
    ErosionOverlay::~ErosionOverlay() { _sim.reset(); }

////////////////////////////////////////////////////////////////////////////////////////////////////

    template<typename Type>
        public ref class ClassAccessors_GetAndSet : public IGetAndSetProperties
    {
    public:
        virtual bool TryGetMember(System::String^ name, bool caseInsensitive, System::Type^ type, Object^% result);
        virtual bool TrySetMember(System::String^ name, bool caseInsensitive, Object^ value);

        explicit ClassAccessors_GetAndSet(std::shared_ptr<Type> type);
        !ClassAccessors_GetAndSet();
        ~ClassAccessors_GetAndSet();

    protected:
        clix::shared_ptr<Type> _type;
    };

    template<typename Type>
        bool ClassAccessors_GetAndSet<Type>::TryGetMember(System::String^ name, bool caseInsensitive, System::Type^ type, Object^% result)
    {
        auto& accessors = GetAccessors<Type>();
        auto nativeString = clix::marshalString<clix::E_UTF8>(name);
        if (type == System::Single::typeid) {
            float f = 0.f;
            bool success = accessors.TryGet(
                f, *_type.get(), Hash64(nativeString.c_str()));
            if (success)
                result = gcnew System::Single(f);
            return success;
        } 
        else if (type == System::UInt32::typeid) {
            uint32 f = 0;
            bool success = accessors.TryGet(
                f, *_type.get(), Hash64(nativeString.c_str()));
            if (success)
                result = gcnew System::UInt32(f);
            return success;
        }
        return false;
    }

    template<typename Type>
        bool ClassAccessors_GetAndSet<Type>::TrySetMember(System::String^ name, bool caseInsensitive, Object^ value)
    {
        auto& accessors = GetAccessors<Type>();
        auto nativeString = clix::marshalString<clix::E_UTF8>(name);
        if (value->GetType() == System::Single::typeid) {
            auto v = *dynamic_cast<System::Single^>(value);
            return accessors.TrySet(v, *_type.get(), Hash64(nativeString.c_str()));
        } else if (value->GetType() == System::UInt32::typeid) {
            auto v = *dynamic_cast<System::UInt32^>(value);
            return accessors.TryGet(v, *_type.get(), Hash64(nativeString.c_str()));
        }
        return false;
    }

    template<typename Type>
        ClassAccessors_GetAndSet<Type>::ClassAccessors_GetAndSet(std::shared_ptr<Type> type)
        : _type(type)
    {}

    template<typename Type>
        ClassAccessors_GetAndSet<Type>::!ClassAccessors_GetAndSet() { _type.reset(); }

    template<typename Type>
        ClassAccessors_GetAndSet<Type>::~ClassAccessors_GetAndSet() {}

////////////////////////////////////////////////////////////////////////////////////////////////////

    class ErosionIterativeSystemPimpl
    {
    public:
        std::shared_ptr<SceneEngine::ErosionSimulation> _sim;
        std::shared_ptr<ErosionSettings> _settings;
    };

    static std::shared_ptr<RenderCore::Metal::DeviceContext> GetImmediateContext()
    {
        auto immContext = EngineDevice::GetInstance()->GetNative().GetRenderDevice()->GetImmediateContext();
        return RenderCore::Metal::DeviceContext::Get(*immContext);
    }

    void ErosionIterativeSystem::Tick()
    {
        TRY {
            _pimpl->_sim->Tick(*GetImmediateContext(), *_pimpl->_settings);
        } CATCH (const ::Assets::Exceptions::PendingAsset&) {
        } CATCH_END
    }

    ErosionIterativeSystem::ErosionIterativeSystem(String^ sourceHeights)
    {
        using namespace SceneEngine;
        _pimpl.reset(new ErosionIterativeSystemPimpl);
        _pimpl->_settings = std::make_shared<ErosionSettings>();
        _settings = gcnew ErosionIterativeSystem::Settings();

        _getAndSetProperties = gcnew ClassAccessors_GetAndSet<ErosionSettings>(_pimpl->_settings);

        {
            TerrainUberSurfaceGeneric uberSurface(
                clix::marshalString<clix::E_UTF8>(sourceHeights).c_str());

            auto maxSize = 4096u;
            UInt2 dims(
                std::min(uberSurface.GetWidth(), maxSize),
                std::min(uberSurface.GetHeight(), maxSize));
            _pimpl->_sim = std::make_shared<ErosionSimulation>(dims, 1.f);

                // We can use the an ubersurface interface to get the 
                // heights data onto the GPU (in the form of a resource locator)
                // Note that we're limited by the maximum texture size supported
                // by the GPU here. If we want to deal with a very large area, we
                // have to split it up into multiple related simulations.
            intrusive_ptr<BufferUploads::ResourceLocator> resLoc;
            {
                GenericUberSurfaceInterface interf(uberSurface);
                resLoc = interf.CopyToGPU(UInt2(0,0), dims);
            }

            RenderCore::Metal::ShaderResourceView srv(resLoc->GetUnderlying());
            _pimpl->_sim->InitHeights(
                *GetImmediateContext(), srv,
                UInt2(0,0), dims);
        }

        _overlay = gcnew ErosionOverlay(_pimpl->_sim, _settings);
    }

    ErosionIterativeSystem::!ErosionIterativeSystem()
    {
        _pimpl.reset();
        delete _overlay; _overlay = nullptr;
    }

    ErosionIterativeSystem::~ErosionIterativeSystem()
    {
        _pimpl.reset();
        delete _overlay; _overlay = nullptr;
    }

    ErosionIterativeSystem::Settings::Settings()
    {
        ActivePreview = Preview::HardMaterials;
    }
}
