//====== Copyright (c) 2014, Valve Corporation, All rights reserved. ========//
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//===========================================================================//

#ifndef PACKETBITBUF_H
#define PACKETBITBUF_H

#include <packet.h>

#define LittleShort(val) (val)
#define LittleLong(val) (val)
// OVERALL Coordinate Size Limits used in COMMON.C MSG_*BitCoord() Routines (and
// someday the HUD)
#define COORD_INTEGER_BITS 14
#define COORD_FRACTIONAL_BITS 5
#define COORD_DENOMINATOR (1 << (COORD_FRACTIONAL_BITS))
#define COORD_RESOLUTION (1.0f / (COORD_DENOMINATOR))

// Special threshold for networking multiplayer origins
#define COORD_INTEGER_BITS_MP 11
#define COORD_FRACTIONAL_BITS_MP_LOWPRECISION 3
#define COORD_DENOMINATOR_LOWPRECISION \
    (1 << (COORD_FRACTIONAL_BITS_MP_LOWPRECISION))
#define COORD_RESOLUTION_LOWPRECISION (1.0f / (COORD_DENOMINATOR_LOWPRECISION))

#define NORMAL_FRACTIONAL_BITS 11
#define NORMAL_DENOMINATOR ((1 << (NORMAL_FRACTIONAL_BITS)) - 1)
#define NORMAL_RESOLUTION (1.0f / (NORMAL_DENOMINATOR))

enum EBitCoordType
{
    kCW_None,
    kCW_LowPrecision,
    kCW_Integral
};

//-----------------------------------------------------------------------------
// namespaced helpers
//-----------------------------------------------------------------------------
namespace bitbuf
{
    // ZigZag Transform:  Encodes signed integers so that they can be
    // effectively used with varint encoding.
    //
    // varint operates on unsigned integers, encoding smaller numbers into
    // fewer bytes.  If you try to use it on a signed integer, it will treat
    // this number as a very large unsigned integer, which means that even
    // small signed numbers like -1 will take the maximum number of bytes
    // (10) to encode.  ZigZagEncode() maps signed integers to unsigned
    // in such a way that those with a small absolute value will have smaller
    // encoded values, making them appropriate for encoding using varint.
    //
    //       int32 ->     uint32
    // -------------------------
    //           0 ->          0
    //          -1 ->          1
    //           1 ->          2
    //          -2 ->          3
    //         ... ->        ...
    //  2147483647 -> 4294967294
    // -2147483648 -> 4294967295
    //
    //        >> encode >>
    //        << decode <<

    inline uint32 ZigZagEncode32(int32 n)
    {
        // Note:  the right-shift must be arithmetic
        return (n << 1) ^ (n >> 31);
    }

    inline int32 ZigZagDecode32(uint32 n)
    {
        return (n >> 1) ^ -static_cast<int32>(n & 1);
    }

    inline int64 ZigZagDecode64(uint64 n)
    {
        return (n >> 1) ^ -static_cast<int64>(n & 1);
    }

    const int kMaxVarintBytes = 10;
    const int kMaxVarint32Bytes = 5;
}

class CBitRead
{
 public:
    uint32 m_nInBufWord;
    int m_nBitsAvail;
    uint32* m_pDataIn;
    uint32* m_pBufferEnd;
    uint32* m_pData;

    bool m_bOverflow;
    int m_nDataBits;
    size_t m_nDataBytes;

    static const uint32 s_nMaskTable[33]; // 0 1 3 7 15 ..

 public:
    CBitRead(const void* pData, int nBytes, int nBits = -1)
    {
        Init();
        StartReading(pData, nBytes, 0, nBits);
    }

    CBitRead(void)
    {
        Init();
    }

    void Init()
    {
        m_bOverflow = false;
        m_nDataBits = -1;
        m_nDataBytes = 0;
        m_pBufferEnd = nullptr;
        m_pData = nullptr;
        m_pDataIn = nullptr;
        m_nBitsAvail = 0;
        m_nInBufWord = 0;
    }

    void SetOverflowFlag(void)
    {
        m_bOverflow = true;
    }

    bool IsOverflowed(void) const
    {
        return m_bOverflow;
    }

    int Tell(void) const
    {
        return GetNumBitsRead();
    }

    size_t TotalBytesAvailable(void) const
    {
        return m_nDataBytes;
    }

    int GetNumBitsLeft(void) const
    {
        return m_nDataBits - Tell();
    }

    int GetNumBytesLeft(void) const
    {
        return GetNumBitsLeft() >> 3;
    }

    bool Seek(int nPosition);

    bool SeekRelative(int nOffset)
    {
        return Seek(GetNumBitsRead() + nOffset);
    }

    unsigned char const* GetBasePointer()
    {
        return reinterpret_cast<unsigned char const*>(m_pData);
    }

    void StartReading(const void* pData,
                      int nBytes,
                      int iStartBit = 0,
                      int nBits = -1);

    int GetNumBitsRead(void) const;
    int GetNumBytesRead(void) const;

    void GrabNextDWord(bool bOverFlowImmediately = false);
    void FetchNext(void);
    unsigned int ReadUBitLong(int numbits);
    int ReadSBitLong(int numbits);
    unsigned int ReadUBitVar(void);
    unsigned int PeekUBitLong(int numbits);
    bool ReadBytes(void* pOut, int nBytes);

    // Returns 0 or 1.
    int ReadOneBit(void);
    int ReadLong(void)
    {
        return static_cast<int>(ReadUBitLong(sizeof(long) << 3));
    }

    int ReadChar(void);
    int ReadByte(void);
    int ReadShort(void);
    int ReadWord(void);
    float ReadFloat(void);
    void ReadBits(void* pOut, int nBits);

    float ReadBitCoord();
    float ReadBitCoordMP(EBitCoordType coordType);
    float ReadBitCellCoord(int bits, EBitCoordType coordType);
    float ReadBitNormal();
    void ReadBitVec3Coord(Vector& fa);
    void ReadBitVec3Normal(Vector& fa);
    void ReadBitAngles(QAngle& fa);
    float ReadBitAngle(int numbits);
    float ReadBitFloat(void);

    // Returns false if bufLen isn't large enough to hold the
    // string in the buffer.
    //
    // Always reads to the end of the string (so you can read the
    // next piece of data waiting).
    //
    // If bLine is true, it stops when it reaches a '\n' or a null-terminator.
    //
    // pStr is always null-terminated (unless bufLen is 0).
    //
    // pOutNumChars is set to the number of characters left in pStr when the
    // routine is complete (this will never exceed bufLen-1).
    //
    bool ReadString(char* pStr,
                    int bufLen,
                    bool bLine = false,
                    int* pOutNumChars = nullptr);

    // reads a varint encoded integer
    uint32 ReadVarInt32();
    uint64 ReadVarInt64();
    int32 ReadSignedVarInt32()
    {
        return bitbuf::ZigZagDecode32(ReadVarInt32());
    }
    int64 ReadSignedVarInt64()
    {
        return bitbuf::ZigZagDecode64(ReadVarInt64());
    }
};

#ifndef MIN
    #define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#if !defined(_X360)
    #define LZSS_ID (('S' << 24) | ('S' << 16) | ('Z' << 8) | ('L'))
#else
    #define LZSS_ID (('L' << 24) | ('Z' << 16) | ('S' << 8) | ('S'))
#endif

// bind the buffer for correct identification
struct lzss_header_t
{
    unsigned int id;
    unsigned int actualSize; // always little endian
};

#define DEFAULT_LZSS_WINDOW_SIZE 4096

class CLZSS
{
 public:
    unsigned char* Compress(unsigned char* pInput,
                            int inputlen,
                            unsigned int* pOutputSize);
    unsigned char* CompressNoAlloc(unsigned char* pInput,
                                   int inputlen,
                                   unsigned char* pOutput,
                                   unsigned int* pOutputSize);
    unsigned int Uncompress(unsigned char* pInput, unsigned char* pOutput);
    bool IsCompressed(unsigned char* pInput);
    unsigned int GetActualSize(unsigned char* pInput);

    // windowsize must be a power of two.
    CLZSS(int nWindowSize = DEFAULT_LZSS_WINDOW_SIZE);

 private:
    // expected to be sixteen bytes
    struct lzss_node_t
    {
        unsigned char* pData;
        lzss_node_t* pPrev;
        lzss_node_t* pNext;
        char empty[4];
    };

    struct lzss_list_t
    {
        lzss_node_t* pStart;
        lzss_node_t* pEnd;
    };

    void BuildHash(unsigned char* pData);
    lzss_list_t* m_pHashTable;
    lzss_node_t* m_pHashTarget;
    int m_nWindowSize;
};

#endif
