#pragma once
#include "IMemoryFile.h"
#include "MemoryFileWindows.h"

namespace Cloudtoid::Interprocess
{
    // This class manages the underlying Memory Mapped File
    class MemoryView
    {
        IMemoryFile* _file;
        void* _view;

    public:
        ~MemoryView()
        {
            if (_view != nullptr)
            {
                UnmapViewOfFile(_view);
                _view = nullptr;
            }

            if (_file != nullptr)
            {
                delete _file;
                _file = nullptr;
            }
        }

        explicit MemoryView(const QueueOptions& options)
        {
            _file = new Memory::Windows::MemoryFileWindows(options);

            _view = MapViewOfFile(
                _file.GetMappedFile(),
                FILE_MAP_READ | FILE_MAP_WRITE,
                0,
                0,
                0);

            if (_view == nullptr)
            {
                const auto error = GetLastError();
                throw std::system_error(static_cast<int>(error), std::system_category());
            }
        }

        MemoryView(MemoryView& other) = delete;
        MemoryView& operator=(MemoryView& other) = delete;
        MemoryView(MemoryView&& other) = default;
        MemoryView& operator=(MemoryView&& other) = default;

        unsigned char* Pointer() const noexcept
        {
            return static_cast<unsigned char*>(_view);
        }
    };
} // namespace Cloudtoid::Interprocess
