#pragma once

namespace Cloudtoid::Interprocess
{
    class IMemoryFile
    {
    public:
        virtual ~IMemoryFile() = 0;
        IMemoryFile(const IMemoryFile&) = delete;
        IMemoryFile& operator=(const IMemoryFile&) = delete;
        IMemoryFile(IMemoryFile&&) = default;
        IMemoryFile& operator=(IMemoryFile&&) = default;

        [[nodiscard]]
        void* GetMappedFile() const noexcept
        {
            return _mappedFile;
        }

    protected:
        IMemoryFile() = default;
        void* _mappedFile = nullptr;
    };

    // Even pure virtual destructors need an implementation
    inline IMemoryFile::~IMemoryFile() = default;
} // namespace Cloudtoid::Interprocess
