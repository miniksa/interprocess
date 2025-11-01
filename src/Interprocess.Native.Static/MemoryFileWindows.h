#pragma once
#include <system_error>

#include "IMemoryFile.h"
#include "QueueOptions.h"

#define NOMINMAX
#include <windows.h>

namespace Cloudtoid::Interprocess::Memory::Windows
{
    class MemoryFileWindows final : public IMemoryFile
    {
        const wchar_t* _mapNamePrefix = L"CT_IP_";

        [[nodiscard]]
        static HANDLE CreateOrOpenCore(unsigned long long capacity, const wchar_t* name)
        {
            const auto capacityHigh = static_cast<unsigned long>(capacity >> 32);
            const auto capacityLow = static_cast<unsigned long>(capacity & 0xFFFFFFFF);

            // Try to create a new memory-mapped file
            auto handle = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                capacityHigh,
                capacityLow,
                name
            );

            // CRITICAL: Save GetLastError() immediately after CreateFileMappingW
            // because subsequent operations may overwrite it
            auto lastError = GetLastError();

            if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
            {
                // Check if this is a newly created file vs existing one
                bool isNewFile = (lastError != ERROR_ALREADY_EXISTS);
                
                if (isNewFile)
                {
                    // For newly created memory-mapped files, ensure they are zero-initialized
                    // Map the entire file to zero it out
                    void* view = MapViewOfFile(handle, FILE_MAP_WRITE, 0, 0, 0);
                    if (view != nullptr)
                    {
                        // Zero out the entire memory-mapped file
                        ZeroMemory(view, static_cast<SIZE_T>(capacity));
                        UnmapViewOfFile(view);
                    }
                }
                
                return handle;
            }
            
            auto error = GetLastError();
            if (error != ERROR_ACCESS_DENIED)
            {
                throw std::system_error(static_cast<int>(error), std::system_category());
            }

            // Try to open existing file mapping
            handle = OpenFileMappingW(PAGE_READWRITE, FALSE, name);

            if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
            {
                return handle;
            }

            error = GetLastError();
            throw std::system_error(static_cast<int>(error), std::system_category());
        }

    public:
        ~MemoryFileWindows() override
        {
            if (_mappedFile != nullptr && _mappedFile != INVALID_HANDLE_VALUE)
            {
                CloseHandle(_mappedFile);
                _mappedFile = nullptr;
            }
        }

        explicit MemoryFileWindows(const QueueOptions& options)
        {
            const auto queueName = std::format(L"{}{}", _mapNamePrefix, options.GetQueueName());
            _mappedFile = CreateOrOpenCore(options.GetQueueStorageSize(), queueName.data());
        }

        MemoryFileWindows(const MemoryFileWindows&) = delete;
        MemoryFileWindows& operator=(const MemoryFileWindows&) = delete;
        MemoryFileWindows(MemoryFileWindows&&) = default;
        MemoryFileWindows& operator=(MemoryFileWindows&&) = default;
    };
} // namespace Cloudtoid::Interprocess
