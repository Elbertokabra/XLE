// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Core/Types.h"
#include "../Core/Prefix.h"
#include "../Utility/StringUtils.h"
#include "../Utility/IntrusivePtr.h"

namespace RenderCore
{
	enum class Format;

    /// Container for CPUAccess::Enum
    namespace CPUAccess
    {
        /// <summary>Determines CPU access privileges</summary>
        /// Determines what access (if any) the CPU will have to the
        /// buffer. This can control how the object is stored in memory
        /// in into what memory it is stored.
        ///
        /// It is always a good idea to use as few flags as possible. Many
        /// buffers should be fine with a CPUAccess flag of "0".
        ///
        /// Note that this flag can change how the system performs upload
        /// operations. For example, in D3D11, when the CPUAccess::WriteDynamic flag
        /// is set, UpdateSubresource() can't be used to some types of buffers.
        /// In these cases, Map() is used instead.
        ///
        /// Try to avoid "WriteDynamic" unless the buffer will be Locked
        /// multiple times per frame. If a buffer only needs to be updated
        /// once per frame (or less), use CPUAccess::Write
        enum Enum
        {
            Read                = 1<<0,             ///< CPU can read from this resource (using IManager::Resource_Readback)
            Write               = 1<<1,             ///< CPU can write to this resource, but will only do so once (or less) per frame
            WriteDynamic        = (1<<2)|Write      ///< CPU can write to this resource, and will lock it multiple times during a single per frame
        };
        typedef unsigned BitField;
    }

    /// Container for GPUAccess::Enum
    namespace GPUAccess
    {
        /// <summary>Determines GPU access privileges</summary>
        /// Determines whether the GPU will read from or write to a resource (or both).
        /// As usual, try to limit the privileges were possible.
        enum Enum
        {
            Read                = 1<<0,     ///< GPU can read from a resource (eg, shader resource, texture, input structured buffer)
            Write               = 1<<1      ///< GPU can write to the resource (eg, render target, RWTexture, RWStructuredBuffer)
        };
        typedef unsigned BitField;
    }

    /// Container for BindFlag::Enum
    namespace BindFlag
    {
        /// <summary>Determines how the buffer will be bound to the pipeline</summary>
        /// Most buffers are just blocks of data on the GPU. They can be bound to the
        /// pipeline in multiple ways, for different purposes. 
        /// 
        /// This flag controls how the buffer can be used. Most buffer only have a single
        /// bind flag. But sometimes we need to combine input and output binding modes
        /// eg: 
        ///     <list>
        ///         <item> BindFlag::RenderTarget | BindFlag::ShaderResource
        ///         <item> BindFlag::DepthStencil | BindFlag::ShaderResource
        ///         <item> BindFlag::StructuredBuffer | BindFlag::VertexBuffer
        ///     </list>
        enum Enum
        {
            VertexBuffer        = 1<<0,     ///< Used as an vertex buffer (ie, IASetVertexBuffers)
            IndexBuffer         = 1<<1,     ///< Used as an index buffer (ie, IASetIndexBuffer)
            ShaderResource      = 1<<2,     ///< Used as a shader resource (ie, PSSetShaderResources)
            RenderTarget        = 1<<3,     ///< Used as a render target (ie, OMSetRenderTargets)
            DepthStencil        = 1<<4,     ///< Used as a depth buffer (ie, OMSetRenderTargets)
            UnorderedAccess     = 1<<5,     ///< Used as a unordered access buffer (ie, CSSetUnorderedAccessViews)
            StructuredBuffer    = 1<<6,     ///< Used as a structured buffer (ie, CSSetShaderResources)
            ConstantBuffer      = 1<<7,     ///< Used as a constant buffer (ie, VSSetConstantBuffers)
            StreamOutput        = 1<<8,     ///< Used as a stream-output buffer from the geomtry shader (ie, SOSetTargets)
            DrawIndirectArgs    = 1<<9,     ///< Used with DrawInstancedIndirect or DrawIndexedInstancedIndirect
            RawViews            = 1<<10     ///< Enables use of raw shader resource views
        };
        typedef unsigned BitField;
    }

    /// Container for AllocationRules::Enum
    namespace AllocationRules
    {
        /// <summary>Determines how BufferUploads will allocate a resource</summary>
        /// Special flags that determine how the system will allocate a resource.
        enum Enum
        {
            Pooled              = 1<<0,     ///< If a compatible resource has been recently released, reuse it
            Batched             = 1<<1,     ///< Batch together similar uploads, so they become a single low level operation per frame
            Staging             = 1<<2,     ///< Staging memory only (ie, don't send to GPU)
            NonVolatile         = 1<<3      ///< Allow the underlying API to manage memory so that it will survive device resets (ie, create a managed resource in D3D9)
        };
        typedef unsigned BitField;
    }
        
        /////////////////////////////////////////////////

    class LinearBufferDesc
    {
	public:
        unsigned _sizeInBytes;
        unsigned _structureByteSize;

        static LinearBufferDesc Create(unsigned sizeInBytes, unsigned structureByteSize=0);
    };

    class TextureSamples
    {
	public:
        uint8 _sampleCount;
        uint8 _samplingQuality;
        static TextureSamples Create(uint8 sampleCount=1, uint8 samplingQuality=0)
        {
            TextureSamples result;
            result._sampleCount = sampleCount;
            result._samplingQuality = samplingQuality;
            return result;
        }
    };

    class TextureDesc
    {
	public:
        uint32 _width, _height, _depth;
        Format _format;
        struct Dimensionality { enum Enum { T1D, T2D, T3D, CubeMap }; };
        Dimensionality::Enum _dimensionality;
        uint8 _mipCount;
        uint16 _arrayCount;
        TextureSamples _samples;

        static TextureDesc Plain1D(
            uint32 width, Format format, 
            uint8 mipCount=1, uint16 arrayCount=0);
        static TextureDesc Plain2D(
            uint32 width, uint32 height, Format format, 
            uint8 mipCount=1, uint16 arrayCount=0, const TextureSamples& samples = TextureSamples::Create());
        static TextureDesc Plain3D(
            uint32 width, uint32 height, uint32 depth, Format format, uint8 mipCount=1);
        static TextureDesc Empty();
    };

    /// <summary>Description of a buffer</summary>
    /// Description of a buffer, used during creation operations.
    /// Usually, BufferDesc is filled with a description of a new buffer to create,
    /// and passed to IManager::Transaction_Begin.
    class ResourceDesc
    {
    public:
            // following the D3D11 style; let's use a "type" member, with a union
        struct Type { enum Enum { LinearBuffer, Texture, Unknown, Max }; };
        Type::Enum _type;
        BindFlag::BitField _bindFlags;
        CPUAccess::BitField _cpuAccess; 
        GPUAccess::BitField _gpuAccess;
        AllocationRules::BitField _allocationRules;
        union {
            LinearBufferDesc _linearBufferDesc;
            TextureDesc _textureDesc;
        };
        char _name[48];

		ResourceDesc();
    };

    inline ResourceDesc CreateDesc(
        BindFlag::BitField bindFlags,
        CPUAccess::BitField cpuAccess, 
        GPUAccess::BitField gpuAccess,
        const TextureDesc& textureDesc,
        const char name[])
    {
		ResourceDesc desc;
        desc._type = ResourceDesc::Type::Texture;
        desc._bindFlags = bindFlags;
        desc._cpuAccess = cpuAccess;
        desc._gpuAccess = gpuAccess;
        desc._allocationRules = 0;
        desc._textureDesc = textureDesc;
        XlCopyString(desc._name, dimof(desc._name), name);
        return desc;
    }

    inline ResourceDesc CreateDesc(
        BindFlag::BitField bindFlags,
        CPUAccess::BitField cpuAccess, 
        GPUAccess::BitField gpuAccess,
        const LinearBufferDesc& linearBufferDesc,
        const char name[])
    {
		ResourceDesc desc;
        desc._type = ResourceDesc::Type::LinearBuffer;
        desc._bindFlags = bindFlags;
        desc._cpuAccess = cpuAccess;
        desc._gpuAccess = gpuAccess;
        desc._allocationRules = 0;
        desc._linearBufferDesc = linearBufferDesc;
        XlCopyString(desc._name, dimof(desc._name), name);
        return desc;
    }

	inline TextureDesc TextureDesc::Plain1D(
		uint32 width, Format format,
		uint8 mipCount, uint16 arrayCount)
	{
		TextureDesc result;
		result._width = width;
		result._height = 1;
		result._depth = 1;
		result._format = format;
		result._dimensionality = Dimensionality::T1D;
		result._mipCount = mipCount;
		result._arrayCount = arrayCount;
		result._samples = TextureSamples::Create();
		return result;
	}

	inline TextureDesc TextureDesc::Plain2D(
		uint32 width, uint32 height, Format format,
		uint8 mipCount, uint16 arrayCount,
		const TextureSamples& samples)
	{
		TextureDesc result;
		result._width = width;
		result._height = height;
		result._depth = 1;
		result._format = format;
		result._dimensionality = Dimensionality::T2D;
		result._mipCount = mipCount;
		result._arrayCount = arrayCount;
		result._samples = samples;
		return result;
	}

	inline TextureDesc TextureDesc::Plain3D(
		uint32 width, uint32 height, uint32 depth,
		Format format, uint8 mipCount)
	{
		TextureDesc result;
		result._width = width;
		result._height = height;
		result._depth = depth;
		result._format = format;
		result._dimensionality = Dimensionality::T3D;
		result._mipCount = mipCount;
		result._arrayCount = 0;
		result._samples = TextureSamples::Create();
		return result;
	}

	inline TextureDesc TextureDesc::Empty()
	{
		TextureDesc result;
		result._width = 0;
		result._height = 0;
		result._depth = 0;
		result._format = (Format)0;
		result._dimensionality = Dimensionality::T1D;
		result._mipCount = 0;
		result._arrayCount = 0;
		result._samples = TextureSamples::Create();
		return result;
	}

	inline LinearBufferDesc LinearBufferDesc::Create(unsigned sizeInBytes, unsigned structureByteSize)
	{
		return LinearBufferDesc{ sizeInBytes, structureByteSize };
	}

	class SubResourceInitData
	{
	public:
		const void* _data;
		size_t _size;
		size_t _rowPitch;
		size_t _slicePitch;
	};
}