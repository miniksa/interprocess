namespace Cloudtoid.Interprocess.Tests;

public unsafe class CircularBufferTests
{
    private static readonly byte[] ByteArray = [100, 110, 120];

    [Theory]
    [InlineData(new byte[] { 100 }, 0, 0)]
    [InlineData(new byte[] { 100 }, 1, 0)]
    [InlineData(new byte[] { 100 }, 2, 0)]
    [InlineData(new byte[] { 100 }, 3, 0)]
    [InlineData(new byte[] { 100, 110 }, 0, 0)]
    [InlineData(new byte[] { 100, 110 }, 1, 1)]
    [InlineData(new byte[] { 100, 110 }, 2, 0)]
    [InlineData(new byte[] { 100, 110 }, 3, 1)]
    public void CanAdjustOffset(byte[] bytes, long offset, long adjustedOffset)
    {
        fixed (byte* bytesPtr = &bytes[0])
        {
            var buffer = new CircularBuffer(bytesPtr, bytes.Length);
            buffer.Capacity.Should().Be(bytes.Length);
            buffer.AdjustedOffset(ref offset);
            offset.Should().Be(adjustedOffset);
        }
    }

    [Theory]
    [InlineData(new byte[] { 100 }, 0, 100)]
    [InlineData(new byte[] { 100 }, 1, 100)]
    [InlineData(new byte[] { 100 }, 2, 100)]
    [InlineData(new byte[] { 100 }, 3, 100)]
    [InlineData(new byte[] { 100, 110 }, 0, 100)]
    [InlineData(new byte[] { 100, 110 }, 1, 110)]
    [InlineData(new byte[] { 100, 110 }, 2, 100)]
    [InlineData(new byte[] { 100, 110 }, 3, 110)]
    public void CanGetPointer(byte[] bytes, long offset, byte expectedValue)
    {
        fixed (byte* bytesPtr = &bytes[0])
        {
            var buffer = new CircularBuffer(bytesPtr, bytes.Length);
            buffer.Capacity.Should().Be(bytes.Length);
            var b = *buffer.GetPointer(offset);
            b.Should().Be(expectedValue);
        }
    }

    [Theory]
    [InlineData(0, 0, new byte[] { })]
    [InlineData(0, 1, new byte[] { 100 })]
    [InlineData(1, 1, new byte[] { 110 })]
    [InlineData(2, 1, new byte[] { 120 })]
    [InlineData(3, 1, new byte[] { 100 })]
    [InlineData(0, 2, new byte[] { 100, 110 })]
    [InlineData(1, 2, new byte[] { 110, 120 })]
    [InlineData(2, 2, new byte[] { 120, 100 })]
    [InlineData(3, 2, new byte[] { 100, 110 })]
    [InlineData(0, 3, new byte[] { 100, 110, 120 })]
    [InlineData(1, 3, new byte[] { 110, 120, 100 })]
    [InlineData(2, 3, new byte[] { 120, 100, 110 })]
    [InlineData(3, 3, new byte[] { 100, 110, 120 })]
    [InlineData(0, 4, new byte[] { 100, 110, 120, 100 })]
    [InlineData(1, 4, new byte[] { 110, 120, 100, 110 })]
    [InlineData(0, 0, new byte[] { }, 1)]
    [InlineData(1, 4, new byte[] { 110 }, 1)]
    [InlineData(1, 2, new byte[] { 110, 120 }, 6)]
    public void CanRead(long offset, int length, byte[] expectedResult, int? bufferLength = null)
    {
        fixed (byte* bytesPtr = &ByteArray[0])
        {
            var buffer = new CircularBuffer(bytesPtr, ByteArray.Length);
            if (bufferLength is null)
                buffer.Read(offset, length).ToArray().Should().BeEquivalentTo(expectedResult);

            var resultBuffer = new byte[bufferLength ?? length];
            buffer.Read(offset, length, resultBuffer).ToArray().Should().BeEquivalentTo(expectedResult);
        }
    }

    [Theory]
    [InlineData(0, 0, new byte[] { })]
    [InlineData(0, 1, new byte[] { 100 })]
    [InlineData(1, 1, new byte[] { 110 })]
    [InlineData(2, 1, new byte[] { 120 })]
    [InlineData(3, 1, new byte[] { 100 })]
    [InlineData(0, 2, new byte[] { 100, 110 })]
    [InlineData(1, 2, new byte[] { 110, 120 })]
    [InlineData(2, 2, new byte[] { 120, 100 })]
    [InlineData(3, 2, new byte[] { 100, 110 })]
    [InlineData(0, 3, new byte[] { 100, 110, 120 })]
    [InlineData(1, 3, new byte[] { 110, 120, 100 })]
    [InlineData(2, 3, new byte[] { 120, 100, 110 })]
    [InlineData(3, 3, new byte[] { 100, 110, 120 })]
    public void CanWrite(long offset, long length, byte[] bytes)
    {
        var b = new byte[3];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, b.Length);
            buffer.Write(bytes, offset);
            buffer.Read(offset, length).ToArray().Should().BeEquivalentTo(bytes);
        }
    }

    [Fact]
    public void CanWriteStruct()
    {
        var b = new byte[sizeof(QueueHeader)];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, b.Length);
            var value = new QueueHeader
            {
                ReadOffset = 1,
                WriteOffset = 2,
                ReadLockTimestamp = long.MaxValue,
                Reserved = long.MinValue
            };
            buffer.Write(value, 0);
            value.Should().BeEquivalentTo(*(QueueHeader*)ptr);
        }
    }

    [Theory]
    [InlineData(0, 0)]
    [InlineData(0, 1)]
    [InlineData(1, 1)]
    [InlineData(2, 1)]
    [InlineData(3, 1)]
    [InlineData(0, 2)]
    [InlineData(1, 2)]
    [InlineData(2, 2)]
    [InlineData(3, 2)]
    [InlineData(0, 3)]
    [InlineData(1, 3)]
    [InlineData(2, 3)]
    [InlineData(3, 3)]
    public void CanZeroBlock(long offset, long length)
    {
        var b = new byte[3] { 1, 1, 1 };
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, b.Length);
            buffer.Read(offset, length).ToArray().All(i => i == 1).Should().BeTrue();
            buffer.Clear(offset, length);
            buffer.Read(offset, length).ToArray().All(i => i == 0).Should().BeTrue();
        }
    }

    [Fact]
    public void WriteSpanWrapping()
    {
        // Test that spans wrap correctly across buffer boundary
        var b = new byte[10];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 10);
            var data = new byte[] { 1, 2, 3, 4, 5 };

            buffer.Write(data, 8); // Start near end, should wrap

            var result = buffer.Read(8, 5);
            result.ToArray().Should().BeEquivalentTo(data, options => options.WithStrictOrdering());
        }
    }

    [Fact]
    public void WriteAtExactBoundary()
    {
        // Test writing at exact buffer boundary
        var b = new byte[10];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 10);
            var data = new byte[] { 1, 2, 3 };

            buffer.Write(data, 10); // Exactly at boundary, should wrap to 0

            var result = buffer.Read(0, 3);
            result.ToArray().Should().BeEquivalentTo(data, options => options.WithStrictOrdering());
        }
    }

    [Fact]
    public void ReadWrapping()
    {
        // Test that reads wrap correctly across buffer boundary
        var b = new byte[10];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 10);

            // Fill buffer with known values
            for (int i = 0; i < 10; i++)
                b[i] = (byte)(i + 1);

            // Read across boundary
            var result = buffer.Read(8, 4);
            result.ToArray().Should().BeEquivalentTo(new byte[] { 9, 10, 1, 2 });
        }
    }

    [Fact]
    public void ClearWrapping()
    {
        // Test that clear operations wrap correctly
        var b = new byte[10];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 10);

            // Fill with non-zero values
            for (int i = 0; i < 10; i++)
                b[i] = 0xFF;

            // Clear across boundary
            buffer.Clear(8, 4);

            var result = buffer.Read(0, 10);
            result.Span[0].Should().Be(0); // Wrapped from clear
            result.Span[1].Should().Be(0); // Wrapped from clear
            result.Span[2].Should().Be(0xFF); // Not cleared
            result.Span[8].Should().Be(0); // Cleared
            result.Span[9].Should().Be(0); // Cleared
        }
    }

    [Fact]
    public void WriteAndReadRoundTripWrapping()
    {
        // Test write/read cycle with wrapping
        var b = new byte[20];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 20);
            var data = new byte[] { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE };

            buffer.Write(data, 18); // Wraps at offset 20
            var result = buffer.Read(18, 5);

            result.ToArray().Should().BeEquivalentTo(data, options => options.WithStrictOrdering());
        }
    }

    [Fact]
    public void MultipleSequentialWrites()
    {
        // Test multiple sequential writes to ensure consistency
        var b = new byte[30];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 30);

            var data1 = new byte[] { 1, 2, 3 };
            var data2 = new byte[] { 4, 5, 6 };
            var data3 = new byte[] { 7, 8, 9 };

            buffer.Write(data1, 0);
            buffer.Write(data2, 3);
            buffer.Write(data3, 6);

            var result = buffer.Read(0, 9);
            result.ToArray().Should().BeEquivalentTo(new byte[] { 1, 2, 3, 4, 5, 6, 7, 8, 9 });
        }
    }

    [Fact]
    public void SingleByteBuffer()
    {
        // Edge case: buffer with capacity of 1
        var b = new byte[1];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 1);

            byte[] data = [42];
            buffer.Write(data, 0);

            var result = buffer.Read(0, 1);
            result.Span[0].Should().Be(42);
        }
    }

    [Fact]
    public void LargeOffsetWrapping()
    {
        // Test very large offsets that wrap multiple times
        var b = new byte[10];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 10);

            // Fill with known pattern
            for (int i = 0; i < 10; i++)
                b[i] = (byte)i;

            // Very large offset: 1000005 % 10 = 5
            var result = buffer.Read(1000005, 3);
            result.ToArray().Should().BeEquivalentTo(new byte[] { 5, 6, 7 });
        }
    }

    [Fact]
    public void FullBufferWriteAndRead()
    {
        // Test writing and reading entire buffer capacity
        const int bufferSize = 256;
        var b = new byte[bufferSize];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, bufferSize);

            var writeData = Enumerable.Range(0, bufferSize).Select(i => (byte)i).ToArray();
            buffer.Write(writeData, 0);

            var result = buffer.Read(0, bufferSize);
            result.ToArray().Should().BeEquivalentTo(writeData, options => options.WithStrictOrdering());
        }
    }

    [Fact]
    public void OverwritePreviousData()
    {
        // Test that new writes correctly overwrite old data
        var b = new byte[10];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 10);

            var data1 = new byte[] { 1, 2, 3, 4, 5 };
            buffer.Write(data1, 0);

            var data2 = new byte[] { 10, 20, 30 };
            buffer.Write(data2, 0);

            var result = buffer.Read(0, 5);
            result.Span[0].Should().Be(10);
            result.Span[1].Should().Be(20);
            result.Span[2].Should().Be(30);
            result.Span[3].Should().Be(4); // Original data
            result.Span[4].Should().Be(5); // Original data
        }
    }

    [Fact]
    public void AlternatingWriteAndClear()
    {
        // Test alternating write and clear operations
        var b = new byte[20];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 20);

            var data = new byte[] { 1, 2, 3, 4, 5 };

            buffer.Write(data, 0);
            buffer.Clear(0, 5);

            buffer.Write(data, 5);
            buffer.Clear(5, 5);

            buffer.Write(data, 10);

            var result = buffer.Read(0, 20);

            // First 10 bytes should be zero
            for (int i = 0; i < 10; i++)
                result.Span[i].Should().Be(0);

            // Last 5 bytes should have the data
            for (int i = 10; i < 15; i++)
                result.Span[i].Should().Be(data[i - 10]);
        }
    }

    [Fact]
    public void DataIntegrityWithAllByteValues()
    {
        // Test that all 256 byte values are preserved correctly
        const int bufferSize = 512;
        var b = new byte[bufferSize];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, bufferSize);

            // Write all 256 byte values twice
            var allBytes = Enumerable.Range(0, 256).Select(i => (byte)i).ToArray();
            var doubleBytes = allBytes.Concat(allBytes).ToArray();

            buffer.Write(doubleBytes, 0);
            var result = buffer.Read(0, 512);

            result.ToArray().Should().BeEquivalentTo(doubleBytes, options => options.WithStrictOrdering());
        }
    }

    [Fact]
    public void OffsetHandlingConsistency()
    {
        // Regression test: Ensure offset handling is consistent across operations
        var b = new byte[10];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 10);

            var data = new byte[] { 1, 2, 3 };

            // Write at offsets that should all wrap to 0
            buffer.Write(data, 0);
            buffer.Write(data, 10);
            buffer.Write(data, 20);

            // All writes should have gone to offset 0 due to wrapping
            var result = buffer.Read(0, 3);
            result.ToArray().Should().BeEquivalentTo(data);
        }
    }

    [Fact]
    public void ClearEntireBuffer()
    {
        // Test clearing the entire buffer
        var b = new byte[50];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 50);

            // Fill with non-zero values
            for (int i = 0; i < 50; i++)
                b[i] = 0xFF;

            buffer.Clear(0, 50);

            var result = buffer.Read(0, 50);
            result.ToArray().All(x => x == 0).Should().BeTrue();
        }
    }

    [Fact]
    public void WriteEmptySpan()
    {
        // Test writing empty span doesn't corrupt state
        var b = new byte[10];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 10);

            var emptyData = Array.Empty<byte>();
            buffer.Write(emptyData, 0);

            // Buffer should still be all zeros
            var result = buffer.Read(0, 10);
            result.ToArray().All(x => x == 0).Should().BeTrue();
        }
    }

    [Fact]
    public void ReadEmptyLength()
    {
        // Test reading zero bytes returns empty result
        var b = new byte[10];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, 10);

            var result = buffer.Read(0, 0);
            result.Length.Should().Be(0);
        }
    }

    [Theory]
    [InlineData(100)]
    [InlineData(500)]
    [InlineData(1000)]
    public void StressTestManyOperations(int iterations)
    {
        // Stress test with many sequential operations
        const int bufferSize = 128;
        var b = new byte[bufferSize];
        fixed (byte* ptr = &b[0])
        {
            var buffer = new CircularBuffer(ptr, bufferSize);

            for (int i = 0; i < iterations; i++)
            {
                int offset = i % bufferSize;
#pragma warning disable IDE0047 // Remove unnecessary parentheses
                int length = ((i * 7) % 20) + 1; // Varying lengths 1-20
#pragma warning restore IDE0047

                var data = Enumerable.Range(0, length).Select(x => (byte)((i + x) % 256)).ToArray();
                buffer.Write(data, offset);

                var result = buffer.Read(offset, length);
                result.ToArray().Should().BeEquivalentTo(
                    data,
                    options => options.WithStrictOrdering(),
                    $"Failed on iteration {i}");
            }
        }
    }
}