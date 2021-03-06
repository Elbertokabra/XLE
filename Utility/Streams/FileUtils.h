// Copyright 2015 XLGAMES Inc.
//
// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "../Detail/API.h"
#include "../../Core/Exceptions.h"
#include "../../Core/Types.h"
#include "../StringUtils.h" // for StringSection
#include "../Mixins.h"
#include <memory>       // for std::unique_ptr

#include <vector>
#include <string>

typedef struct _iobuf FILE;

namespace Utility 
{
    namespace Exceptions
    {
        class IOException : public ::Exceptions::BasicLabel
        {
        public:
            /// <summary>A incomplete list of a few common file related errors</summary>
            /// Openning a file can result in a wide variety of possible errors. However, there
            /// are a few particularly common ones (like file not found, etc). This enum provides
            /// a way to quickly identify some of the common error types.
            enum class Reason
            {
                Success, FileNotFound,
                AccessDenied, WriteProtect,
                Complex
            };

            IOException(Reason reason, const char format[], ...) never_throws
            : _reason(reason)
            {
                va_list args;
                va_start(args, format);
                _vsnprintf_s(_buffer, _TRUNCATE, format, args);
                va_end(args);
            }

            Reason GetReason() const { return _reason; }

        private:
            Reason _reason;
        };
    }

        //
        //  "BasicFile" --  C++ wrapper for file interactions.
        //                  Prefer using BasicFile, instead of C-style interface functions 
        //                  (XlFileOpen, etc)
        //
        //      Cannot be copied, but can be moved.
        //      Constructor will throw on file-system errors
        //      But other methods should not throw on errors
        //
    class BasicFile : noncopyable
    {
    public:
        size_t      Read(void *buffer, size_t size, size_t count) const never_throws;
        size_t      Write(const void *buffer, size_t size, size_t count) never_throws;
        size_t      Seek(size_t offset, int origin) never_throws;
        size_t      TellP() const never_throws;
        void        Flush() const never_throws;

        uint64      GetSize() never_throws;

        struct ShareMode
        {
            enum { Read = 1<<0, Write = 1<<1 };
            typedef unsigned BitField;
        };

        using Reason = Exceptions::IOException::Reason;
        Reason TryOpen(const char filename[], const char openMode[], ShareMode::BitField shareMode = ShareMode::Read) never_throws;

        BasicFile(const char filename[], const char openMode[], ShareMode::BitField shareMode = ShareMode::Read);
        BasicFile(BasicFile&& moveFrom) never_throws;
        BasicFile& operator=(BasicFile&& moveFrom) never_throws;
        BasicFile(const BasicFile& copyFrom) never_throws;
        BasicFile& operator=(const BasicFile& copyFrom) never_throws;
        BasicFile();
        ~BasicFile();

    private:
        void* _file;
    };

    class MemoryMappedFile
    {
    public:
        struct Access
        {
            enum Enum { Read = 1<<0, Write = 1<<1, OpenAlways = 1<<2 };
            typedef unsigned BitField;
        };

        void*           GetData()           { return _mappedData; }
        const void*     GetData() const     { return _mappedData; }
        bool            IsValid()           { return _mappedData != 0; }
        size_t          GetSize() const;

        MemoryMappedFile(
            const char filename[], uint64 size, 
            Access::BitField access,
            BasicFile::ShareMode::BitField shareMode = 0);
        MemoryMappedFile();
        MemoryMappedFile(MemoryMappedFile&& moveFrom) never_throws;
        MemoryMappedFile& operator=(MemoryMappedFile&& moveFrom) never_throws;
        ~MemoryMappedFile();

    private:
        void* _mapping;
        void* _fileHandle;
        void* _mappedData;
    };

    XL_UTILITY_API bool DoesFileExist(const char filename[]);
    XL_UTILITY_API bool DoesDirectoryExist(const char filename[]);
    XL_UTILITY_API std::unique_ptr<uint8[]> LoadFileAsMemoryBlock(const char sourceFileName[], size_t* sizeResult = nullptr);
    XL_UTILITY_API void CreateDirectoryRecursive(const StringSection<char> filename);
    XL_UTILITY_API uint64 GetFileModificationTime(const char filename[]);
    XL_UTILITY_API uint64 GetFileSize(const char filename[]);

    namespace FindFilesFilter
    {
        enum Enum 
        {
            File = 1<<0,
            Directory = 1<<1,
            All =  0xfffffffful
        };
        typedef unsigned BitField;
    }
    XL_UTILITY_API std::vector<std::string> FindFiles(const std::string& searchPath, FindFilesFilter::BitField filter = FindFilesFilter::All);
    XL_UTILITY_API std::vector<std::string> FindFilesHierarchical(const std::string& rootDirectory, const std::string& filePattern, FindFilesFilter::BitField filter = FindFilesFilter::All);
}

using namespace Utility;
