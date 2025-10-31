#pragma once

#include <format>
#include <limits>
#include <string>

#include "IInterprocessSemaphoreReleaser.h"
#include "MemoryFileWindows.h"

namespace Cloudtoid::Interprocess::Semaphore::Windows
{
    class SemaphoreWindows final : public IInterprocessSemaphoreReleaser, public IInterprocessSemaphoreWaiter
    {
        const wchar_t* _handleNamePrefix = L"Global\\CT.IP.";
        HANDLE _handle;

    public:
        explicit SemaphoreWindows(const std::wstring& name)
        {
            auto full_name = std::format(L"{}{}", _handleNamePrefix, name);

            _handle = CreateSemaphoreW(
                nullptr,
                0,
                std::numeric_limits<long>::max(),
                full_name.data());

            if (_handle == nullptr || _handle == INVALID_HANDLE_VALUE)
            {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
            }
        }

        ~SemaphoreWindows() override
        {
            if (_handle != nullptr)
            {
                CloseHandle(_handle);
                _handle = nullptr;
            }
        }

        SemaphoreWindows(const SemaphoreWindows&) = default;
        SemaphoreWindows& operator=(const SemaphoreWindows&) = default;
        SemaphoreWindows(SemaphoreWindows&&) = default;
        SemaphoreWindows& operator=(SemaphoreWindows&&) = default;

        void Release() override
        {
            if (FALSE == ReleaseSemaphore(_handle, 1, nullptr))
            {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
            }
        }

        bool Wait(const int millisecondTimeout) override
        {
            return WaitForSingleObject(_handle, millisecondTimeout) == WAIT_OBJECT_0;
        }
    };
} // namespace Cloudtoid::Interprocess
