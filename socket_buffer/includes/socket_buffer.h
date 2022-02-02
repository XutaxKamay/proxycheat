#ifndef SOCKET_BUFFER_H
#define SOCKET_BUFFER_H

#include <iostream>
#include <cstdint>
#include <cassert>
#include <cstring>

namespace socket_buffer
{
    typedef void* ptr_t;
    typedef unsigned char byte_t;
    typedef byte_t* array_t;

    // A buffer for sockets shoulnd't be > than 2GB anyway.
    typedef int32_t safesize_t;

    std::string bstr(array_t data, int len)
    {
        constexpr char hexmap[] = {'0',
                                   '1',
                                   '2',
                                   '3',
                                   '4',
                                   '5',
                                   '6',
                                   '7',
                                   '8',
                                   '9',
                                   'A',
                                   'B',
                                   'C',
                                   'D',
                                   'E',
                                   'F'};
        std::string s(len * 2, ' ');
        for (int i = 0; i < len; ++i)
        {
            s[2 * i] = hexmap[(data[i] & 0xF0) >> 4];
            s[2 * i + 1] = hexmap[data[i] & 0x0F];
        }
        return s;
    }

    constexpr auto UDPSize = 508;

    template <typename T>
    constexpr inline auto alloc(safesize_t size)
    {
        return reinterpret_cast<T>(::operator new(size));
    }

    template <typename T>
    constexpr inline void free(T& pBuf)
    {
        ::operator delete(reinterpret_cast<ptr_t>(pBuf));
    }

#pragma pack(1)
    typedef enum __attribute__((packed))
    {
        type_pointer,
        type_float,
        type_double,
        type_size,
        type_32,
        type_64,
        type_16,
        type_8,
        type_array,
        type_unknown
    } typesize_t;
#pragma pack()

    template <typename T>
    struct type_wrapper_t
    {
        using type = T;
    };

    template <typename T>
    inline constexpr type_wrapper_t<T> _type {};

    template <typesize_t type>
    inline constexpr auto _gvt()
    {
        if constexpr (type == type_size)
            return _type<safesize_t>;
        else if constexpr (type == type_8)
            return _type<byte_t>;
        else if constexpr (type == type_16)
            return _type<uint16_t>;
        else if constexpr (type == type_32)
            return _type<uint32_t>;
        else if constexpr (type == type_64)
            return _type<uint64_t>;
        else if constexpr (type == type_pointer)
            return _type<ptr_t>;
        else if constexpr (type == type_array)
            return _type<array_t>;
        else if constexpr (type == type_float)
            return _type<float>;
        else if constexpr (type == type_double)
            return _type<double>;
        else
            return _type<void>;
    };

    template <typesize_t _type>
    using gvt = typename decltype(_gvt<_type>())::type;
    template <typesize_t _type>
    using gvt = gvt<_type>;

    template <safesize_t max_size = 0>
    class Buffer
    {
     public:
        Buffer() :
            m_pData(nullptr), m_maxSize(max_size), m_bAllocated(max_size != 0)
        {
            // Allocate with the maximum size for performance.
            if constexpr (max_size != 0)
                m_pData = alloc<decltype(m_pData)>(max_size);
        }

        Buffer(array_t pData, bool bAllocated, safesize_t maxSize = 0) :
            m_pData(pData), m_maxSize(maxSize), m_bAllocated(bAllocated)
        {}

        ~Buffer()
        {
            // Free data.
            if (m_bAllocated)
                free(m_pData);
        }

        template <typename cast_t = ptr_t>
        constexpr inline auto shift(safesize_t size = 0)
        {
            if (size == 0)
            {
                return reinterpret_cast<cast_t>(m_pData);
            }
            else
            {
                return reinterpret_cast<cast_t>(
                    reinterpret_cast<uintptr_t>(m_pData) + size);
            }
        }

        constexpr inline auto& operator[](safesize_t size)
        {
            return *shift<byte_t*>(size);
        }

        array_t m_pData;
        safesize_t m_maxSize;
        bool m_bAllocated;
    };

    template <safesize_t max_size = 0>
    class WriteBuffer : public Buffer<max_size>
    {
     public:
        WriteBuffer() : Buffer<max_size>(), m_writeSize(0)
        {}

        WriteBuffer(array_t pData,
                    bool bAllocated = false,
                    safesize_t writeSize = 0,
                    safesize_t maxSize = 0) :
            Buffer<max_size>(pData, bAllocated, maxSize)
        {
            m_writeSize = writeSize;
        }

        ~WriteBuffer()
        {}

        // Add the type of variable
        constexpr inline auto addType(typesize_t typeSize)
        {
            addData(&typeSize, static_cast<safesize_t>(sizeof(typeSize)));
        }

        template <typesize_t typeSize = type_32>
        constexpr inline auto addVar(gvt<typeSize> value, safesize_t size = 0)
        {
            // Add first the type of variable
            addType(typeSize);

            if constexpr (typeSize == type_array)
            {
                // Add the size of the array
                addData(&size, static_cast<safesize_t>(sizeof(size)));
                addData(value, static_cast<safesize_t>(size));
            }
            else
            {
                addData(&value, static_cast<safesize_t>(sizeof(value)));
            }
        }

        constexpr inline auto reset()
        {
            m_writeSize = 0;
        }

        constexpr inline auto debug()
        {
            std::cout << bstr(this->m_pData, m_writeSize) << std::endl;
        }

        template <typename cast_t = ptr_t>
        constexpr inline auto shift(safesize_t size = 0)
        {
            if (size == 0)
            {
                return reinterpret_cast<cast_t>(this->m_pData);
            }
            else
            {
                return reinterpret_cast<cast_t>(
                    reinterpret_cast<uintptr_t>(this->m_pData) +
                    static_cast<uintptr_t>(size));
            }
        }

        constexpr inline auto addData(ptr_t pData, safesize_t size)
        {
            memcpy(shift(m_writeSize), pData, static_cast<size_t>(size));
            advance(size);
        }

        constexpr inline auto advance(safesize_t size)
        {
            m_writeSize += size;
        }

     public:
        safesize_t m_writeSize;
    };

    // Read buffer for udp packets.
    // UDPSize is the size to be sure that fragment won't happen.
    // WriteBuffer<508> buffer;
    template <safesize_t max_size = 0>
    class ReadBuffer : public Buffer<max_size>
    {
     public:
        ReadBuffer() : Buffer<max_size>(), m_readSize(0)
        {}

        ReadBuffer(array_t pData,
                   bool bAllocated = false,
                   safesize_t readSize = 0,
                   safesize_t maxSize = 0) :
            Buffer<max_size>(pData, bAllocated, maxSize)
        {
            m_readSize = readSize;
        }

        ~ReadBuffer()
        {}

        template <typesize_t typeSize = type_32>
        constexpr inline auto readVar(safesize_t* pSize = nullptr)
        {
            auto type = *this->shift<typesize_t*>(m_readSize);
            advance(sizeof(typesize_t));

            // Read type first
            assert(type == typeSize);

            using varType = gvt<typeSize>;

            varType data = {};

            if constexpr (typeSize == type_array)
            {
                auto dataSize = *this->shift<safesize_t*>(m_readSize);
                advance(sizeof(safesize_t));

                data = this->shift<varType>(m_readSize);
                advance(dataSize);

                if (pSize != nullptr)
                {
                    *pSize = dataSize;
                }
            }
            else
            {
                data = *this->shift<varType*>(m_readSize);
                advance(sizeof(varType));

                if (pSize != nullptr)
                {
                    *pSize = sizeof(varType);
                }
            }

            return data;
        }

        constexpr inline auto reset()
        {
            m_readSize = 0;
        }

        constexpr inline auto debug()
        {
            std::cout << bstr(this->m_pData, m_readSize) << std::endl;
        }

        template <typename cast_t = ptr_t>
        constexpr inline auto shift(safesize_t size = 0)
        {
            if (size == 0)
            {
                return reinterpret_cast<cast_t>(this->m_pData);
            }
            else
            {
                return reinterpret_cast<cast_t>(
                    reinterpret_cast<uintptr_t>(this->m_pData) +
                    static_cast<uintptr_t>(size));
            }
        }

        constexpr inline auto advance(safesize_t size)
        {
            m_readSize += size;
        }

     public:
        safesize_t m_readSize;
    };
};

#endif
