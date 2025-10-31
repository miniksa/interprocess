#pragma once

#include "CircularBuffer.h"
#include "MemoryView.h"
#include "MessageHeader.h"
#include "QueueHeader.h"
#include "QueueOptions.h"

namespace Cloudtoid::Interprocess
{
    class Queue
    {
        MemoryView* _view;

    public:
        explicit Queue(const QueueOptions& options)
        {
            _view = new MemoryView(options);
            _buffer = new CircularBuffer(sizeof(QueueHeader) + _view->Pointer(), options.GetCapacity());
        }

        virtual ~Queue()
        {
            if (_view != nullptr)
            {
                delete _view;
                _view = nullptr;
            }
        }

        Queue(const Queue&) = default;
        Queue& operator=(const Queue&) = default;
        Queue(Queue&&) = default;
        Queue& operator=(Queue&&) = default;

        [[nodiscard]]
        QueueHeader* GetHeader() const
        {
            return reinterpret_cast<QueueHeader*>(_view->Pointer());
        }

    protected:
        CircularBuffer* _buffer;

        static unsigned long long GetMessageBodyOffset(const unsigned long long startOffset)
        {
            return startOffset + sizeof(MessageHeader);
        }

        static unsigned long long GetPaddedMessageLength(const unsigned long long bodyLength)
        {
            const auto length = sizeof(MessageHeader) + bodyLength;

            // Round up to the closest integer divisible by 8. This will add the [padding] if one is needed.
            return 8 * static_cast<unsigned long long>(std::ceil(static_cast<double>(length) / 8.0));
        }

        [[nodiscard]]
        unsigned long long SafeIncrementMessageOffset(const unsigned long long offset,
                                                      const unsigned long long increment) const
        {
            return (offset + increment) % (_buffer->GetCapacity() * 2);
        }
    };
} // namespace Cloudtoid::Interprocess
