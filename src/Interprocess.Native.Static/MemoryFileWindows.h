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

            auto handle = CreateFileMappingW(
                INVALID_HANDLE_VALUE,
                nullptr,
                PAGE_READWRITE,
                capacityHigh,
                capacityLow,
                name
            );

            if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
            {
                return handle;
            }
            auto error = GetLastError();
            if (error != ERROR_ACCESS_DENIED)
            {
                throw std::system_error(static_cast<int>(error), std::system_category());
            }

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
