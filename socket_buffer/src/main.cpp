#include <vector>
#include <socket_buffer.h>
#include <limits>
#include <lz4.h>

using namespace std;
using namespace socket_buffer;

auto main() -> int
{
    vector<byte_t> buf;
    for (int i = 0; i < 1 << 14; i++)
    {
        buf.push_back(i & 0xFF);
    }

    static WriteBuffer<1 << 24> testWrite;

    for (int i = 0; i < 100; i++)
    {
        testWrite.addVar<type_16>(16);
        testWrite.addVar<type_32>(32);
        testWrite.addVar<type_64>(64);
        testWrite.addVar<type_8>(8);
        testWrite.addVar<type_array>(buf.data(),
                                     static_cast<safesize_t>(buf.size()));
        testWrite.addVar<type_array>(buf.data(),
                                     static_cast<safesize_t>(buf.size()));

        testWrite.addVar<type_64>(numeric_limits<gvt<type_64>>::max());

        testWrite.addVar<type_array>(buf.data(),
                                     static_cast<safesize_t>(buf.size()));
        testWrite.addVar<type_8>(8);
    }

    // testWrite.debug();

    auto boundedSize = LZ4_compressBound(testWrite.m_writeSize);
    auto compressed = reinterpret_cast<char*>(alloca(boundedSize));
    auto newSize = LZ4_compress_fast(testWrite.shift<const char*>(),
                                     compressed,
                                     testWrite.m_writeSize,
                                     boundedSize,
                                     1);

    cout << "original=" << testWrite.m_writeSize << " "
         << "compressed=" << newSize << endl;

    cin.get();

    static ReadBuffer<1 << 24> testRead;
    auto realSize = LZ4_decompress_safe(
        compressed, testRead.shift<char*>(), newSize, testRead.m_maxSize);

    cout << "decompressed=" << realSize << endl;
    cin.get();

    for (int i = 0; i < 100; i++)
    {
        cout << hex << testRead.readVar<type_16>() << endl;
        cout << hex << testRead.readVar<type_32>() << endl;
        cout << hex << testRead.readVar<type_64>() << endl;
        cout << hex << static_cast<int>(testRead.readVar<type_8>()) << endl;

        safesize_t size = 0;
        auto bytesPtr = testRead.readVar<type_array>(&size);
        cout << bstr(bytesPtr, size) << endl;

        bytesPtr = testRead.readVar<type_array>(&size);
        cout << bstr(bytesPtr, size) << endl;

        cout << testRead.readVar<type_64>() << endl;

        bytesPtr = testRead.readVar<type_array>(&size);
        cout << bstr(bytesPtr, size) << endl;

        cout << hex << static_cast<int>(testRead.readVar<type_8>()) << endl;
    }

    return 0;
}
