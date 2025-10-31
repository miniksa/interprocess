#pragma once

#include <cstddef>

namespace Cloudtoid::Interprocess
{
    // We rely on this structure to fit in 64 bits.
    // If you change the size of this, no longer many of the assumptions
    // taken in this code are going to be valid.
    class MessageHeader
    {
    public:
        int State;
        int BodyLength;

        static constexpr int LockedToBeConsumedState = 1;
        static constexpr int ReadyToBeConsumedState = 2;

        MessageHeader(const int state, const int bodyLength) :
            State{state},
            BodyLength{bodyLength}
        {
        }
    };

    // Assert exact same layout as the C# version
    static_assert(sizeof(MessageHeader) == 8, "The MessageHeader must be 8-bytes");
    static_assert(offsetof(MessageHeader, State) == 0, "State must be at offset 0");
    static_assert(offsetof(MessageHeader, BodyLength) == 4, "BodyLength must be at offset 4");
} // namespace Cloudtoid::Interprocess
