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

#include <bitread.h>
#include <assert.h>

const uint32 CBitRead::s_nMaskTable[33] = {
    0,
    (1 << 1) - 1,
    (1 << 2) - 1,
    (1 << 3) - 1,
    (1 << 4) - 1,
    (1 << 5) - 1,
    (1 << 6) - 1,
    (1 << 7) - 1,
    (1 << 8) - 1,
    (1 << 9) - 1,
    (1 << 10) - 1,
    (1 << 11) - 1,
    (1 << 12) - 1,
    (1 << 13) - 1,
    (1 << 14) - 1,
    (1 << 15) - 1,
    (1 << 16) - 1,
    (1 << 17) - 1,
    (1 << 18) - 1,
    (1 << 19) - 1,
    (1 << 20) - 1,
    (1 << 21) - 1,
    (1 << 22) - 1,
    (1 << 23) - 1,
    (1 << 24) - 1,
    (1 << 25) - 1,
    (1 << 26) - 1,
    (1 << 27) - 1,
    (1 << 28) - 1,
    (1 << 29) - 1,
    (1 << 30) - 1,
    0x7fffffff,
    0xffffffff,
};

int CBitRead::GetNumBitsRead(void) const
{
    if (!m_pData) // pesky null ptr bitbufs. these happen.
        return 0;

    int nCurOfs = ((uintptr_t(m_pDataIn) - uintptr_t(m_pData)) / 4) - 1;
    nCurOfs *= 32;
    nCurOfs += (32 - m_nBitsAvail);
    int nAdjust = 8 * (m_nDataBytes & 3);
    return MIN(nCurOfs + nAdjust, m_nDataBits);
}

int CBitRead::GetNumBytesRead(void) const
{
    return ((GetNumBitsRead() + 7) >> 3);
}

void CBitRead::GrabNextDWord(bool bOverFlowImmediately)
{
    if (m_pDataIn == m_pBufferEnd)
    {
        m_nBitsAvail = 1; // so that next read will run out of words
        m_nInBufWord = 0;
        m_pDataIn++; // so seek count increments like old
        if (bOverFlowImmediately)
        {
            SetOverflowFlag();
        }
    }
    else if (m_pDataIn > m_pBufferEnd)
    {
        SetOverflowFlag();
        m_nInBufWord = 0;
    }
    else
    {
        assert(reinterpret_cast<uintptr_t>(m_pDataIn) + 3 <
               reinterpret_cast<uintptr_t>(m_pBufferEnd));
        m_nInBufWord = *(m_pDataIn++);
    }
}

void CBitRead::FetchNext(void)
{
    m_nBitsAvail = 32;
    GrabNextDWord(false);
}

int CBitRead::ReadOneBit(void)
{
    int nRet = m_nInBufWord & 1;
    if (--m_nBitsAvail == 0)
    {
        FetchNext();
    }
    else
    {
        m_nInBufWord >>= 1;
    }
    return nRet;
}

unsigned int CBitRead::ReadUBitLong(int numbits)
{
    if (m_nBitsAvail >= numbits)
    {
        unsigned int nRet = m_nInBufWord & s_nMaskTable[numbits];
        m_nBitsAvail -= numbits;
        if (m_nBitsAvail)
        {
            m_nInBufWord >>= numbits;
        }
        else
        {
            FetchNext();
        }
        return nRet;
    }
    else
    {
        // need to merge words
        unsigned int nRet = m_nInBufWord;
        numbits -= m_nBitsAvail;
        GrabNextDWord(true);
        if (m_bOverflow)
            return 0;
        nRet |= ((m_nInBufWord & s_nMaskTable[numbits]) << m_nBitsAvail);
        m_nBitsAvail = 32 - numbits;
        m_nInBufWord >>= numbits;
        return nRet;
    }
}

int CBitRead::ReadSBitLong(int numbits)
{
    int nRet = ReadUBitLong(numbits);
    // sign extend
    return (nRet << (32 - numbits)) >> (32 - numbits);
}

#ifdef _WIN32
    #pragma warning(push)
    #pragma warning(disable : 4715) // disable warning on not all cases
                                    // returning a value. throwing default:
                                    // in measurably reduces perf in bit
                                    // packing benchmark
#endif
unsigned int CBitRead::ReadUBitVar(void)
{
    unsigned int ret = ReadUBitLong(6);
    switch (ret & (16 | 32))
    {
        case 16:
            ret = (ret & 15) | (ReadUBitLong(4) << 4);
            assert(ret >= 16);
            break;

        case 32:
            ret = (ret & 15) | (ReadUBitLong(8) << 4);
            assert(ret >= 256);
            break;
        case 48:
            ret = (ret & 15) | (ReadUBitLong(32 - 4) << 4);
            assert(ret >= 4096);
            break;
    }
    return ret;
}
#ifdef _WIN32
    #pragma warning(pop)
#endif

int CBitRead::ReadChar(void)
{
    return ReadSBitLong(sizeof(char) << 3);
}

int CBitRead::ReadByte(void)
{
    return ReadUBitLong(sizeof(unsigned char) << 3);
}

int CBitRead::ReadShort(void)
{
    return ReadSBitLong(sizeof(short) << 3);
}

int CBitRead::ReadWord(void)
{
    return ReadUBitLong(sizeof(unsigned short) << 3);
}

bool CBitRead::Seek(int nPosition)
{
    bool bSucc = true;
    if (nPosition < 0 || nPosition > m_nDataBits)
    {
        SetOverflowFlag();
        bSucc = false;
        nPosition = m_nDataBits;
    }
    int nHead = m_nDataBytes & 3; // non-multiple-of-4 bytes at head of buffer.
                                  // We put the "round off" at the head to make
                                  // reading and detecting the end efficient.

    int nByteOfs = nPosition / 8;
    if ((m_nDataBytes < 4) || (nHead && (nByteOfs < nHead)))
    {
        // partial first dword
        unsigned char* pPartial = (unsigned char*)m_pData;
        if (m_pData)
        {
            m_nInBufWord = *(pPartial++);
            if (nHead > 1)
            {
                m_nInBufWord |= (*pPartial++) << 8;
            }
            if (nHead > 2)
            {
                m_nInBufWord |= (*pPartial++) << 16;
            }
        }
        m_pDataIn = (uint32*)pPartial;
        m_nInBufWord >>= (nPosition & 31);
        m_nBitsAvail = (nHead << 3) - (nPosition & 31);
    }
    else
    {
        int nAdjPosition = nPosition - (nHead << 3);
        m_pDataIn = reinterpret_cast<uint32*>(
            reinterpret_cast<unsigned char*>(m_pData) +
            ((nAdjPosition / 32) << 2) + nHead);
        if (m_pData)
        {
            m_nBitsAvail = 32;
            GrabNextDWord();
        }
        else
        {
            m_nInBufWord = 0;
            m_nBitsAvail = 1;
        }
        m_nInBufWord >>= (nAdjPosition & 31);
        m_nBitsAvail = MIN(m_nBitsAvail,
                           32 - (nAdjPosition & 31)); // in case grabnextdword
                                                      // overflowed
    }
    return bSucc;
}

void CBitRead::StartReading(const void* pData,
                            int nBytes,
                            int iStartBit,
                            int nBits)
{
    // Make sure it's dword aligned and padded.
    assert(((unsigned long)pData & 3) == 0);
    m_pData = (uint32*)pData;
    m_pDataIn = m_pData;
    m_nDataBytes = nBytes;

    if (nBits == -1)
    {
        m_nDataBits = nBytes << 3;
    }
    else
    {
        assert(nBits <= nBytes * 8);
        m_nDataBits = nBits;
    }
    m_bOverflow = false;
    m_pBufferEnd = reinterpret_cast<uint32*>(
        reinterpret_cast<unsigned char*>(m_pData) + nBytes);
    if (m_pData)
    {
        Seek(iStartBit);
    }
}

bool CBitRead::ReadString(char* pStr, int maxLen, bool bLine, int* pOutNumChars)
{
    assert(maxLen != 0);

    bool bTooSmall = false;
    int iChar = 0;
    while (1)
    {
        char val = ReadChar();
        if (val == 0)
            break;
        else if (bLine && val == '\n')
            break;

        if (iChar < (maxLen - 1))
        {
            pStr[iChar] = val;
            ++iChar;
        }
        else
        {
            bTooSmall = true;
        }
    }

    // Make sure it's null-terminated.
    assert(iChar < maxLen);
    pStr[iChar] = 0;

    if (pOutNumChars)
    {
        *pOutNumChars = iChar;
    }

    return !IsOverflowed() && !bTooSmall;
}

// Read 1-5 bytes in order to extract a 32-bit unsigned value from the
// stream. 7 data bits are extracted from each byte with the 8th bit used
// to indicate whether the loop should continue.
// This allows variable size numbers to be stored with tolerable
// efficiency. Numbers sizes that can be stored for various numbers of
// encoded bits are:
//  8-bits: 0-127
// 16-bits: 128-16383
// 24-bits: 16384-2097151
// 32-bits: 2097152-268435455
// 40-bits: 268435456-0xFFFFFFFF
uint32 CBitRead::ReadVarInt32()
{
    uint32 result = 0;
    int count = 0;
    uint32 b;

    do
    {
        if (count == bitbuf::kMaxVarint32Bytes)
        {
            return result;
        }
        b = ReadUBitLong(8);
        result |= (b & 0x7F) << (7 * count);
        ++count;
    } while (b & 0x80);

    return result;
}

uint64 CBitRead::ReadVarInt64()
{
    uint64 result = 0;
    int count = 0;
    uint64 b;

    do
    {
        if (count == bitbuf::kMaxVarintBytes)
        {
            return result;
        }
        b = ReadUBitLong(8);
        result |= static_cast<uint64>(b & 0x7F) << (7 * count);
        ++count;
    } while (b & 0x80);

    return result;
}

void CBitRead::ReadBits(void* pOutData, int nBits)
{
    unsigned char* pOut = (unsigned char*)pOutData;
    int nBitsLeft = nBits;

    // align output to dword boundary
    while (((unsigned long)pOut & 3) != 0 && nBitsLeft >= 8)
    {
        *pOut = (unsigned char)ReadUBitLong(8);
        ++pOut;
        nBitsLeft -= 8;
    }

    // read dwords
    while (nBitsLeft >= 32)
    {
        auto ret = ReadUBitLong(32);
        memcpy(pOut, &ret, sizeof(ret));
        pOut += sizeof(ret);
        nBitsLeft -= 32;
    }

    // read remaining bytes
    while (nBitsLeft >= 8)
    {
        auto ret = ReadUBitLong(8);
        memcpy(pOut, &ret, 1);
        ++pOut;
        nBitsLeft -= 8;
    }

    // read remaining bits
    if (nBitsLeft)
    {
        *pOut = ReadUBitLong(nBitsLeft);
    }
}

bool CBitRead::ReadBytes(void* pOut, int nBytes)
{
    ReadBits(pOut, nBytes << 3);
    return !IsOverflowed();
}

#define BITS_PER_INT 32
inline int GetBitForBitnum(int bitNum)
{
    static int bitsForBitnum[] = {
        (1 << 0),  (1 << 1),  (1 << 2),  (1 << 3),  (1 << 4),  (1 << 5),
        (1 << 6),  (1 << 7),  (1 << 8),  (1 << 9),  (1 << 10), (1 << 11),
        (1 << 12), (1 << 13), (1 << 14), (1 << 15), (1 << 16), (1 << 17),
        (1 << 18), (1 << 19), (1 << 20), (1 << 21), (1 << 22), (1 << 23),
        (1 << 24), (1 << 25), (1 << 26), (1 << 27), (1 << 28), (1 << 29),
        (1 << 30), (1 << 31),
    };

    return bitsForBitnum[(bitNum) & (BITS_PER_INT - 1)];
}

float CBitRead::ReadBitAngle(int numbits)
{
    float shift = (float)(GetBitForBitnum(numbits));

    int i = ReadUBitLong(numbits);
    float fReturn = (float)i * (360.0f / shift);

    return fReturn;
}

// Basic Coordinate Routines (these contain bit-field size AND fixed point
// scaling constants)
float CBitRead::ReadBitCoord(void)
{
    int intval = 0, fractval = 0, signbit = 0;
    float value = 0.0;

    // Read the required integer and fraction flags
    intval = ReadOneBit();
    fractval = ReadOneBit();

    // If we got either parse them, otherwise it's a zero.
    if (intval || fractval)
    {
        // Read the sign bit
        signbit = ReadOneBit();

        // If there's an integer, read it in
        if (intval)
        {
            // Adjust the integers from [0..MAX_COORD_VALUE-1] to
            // [1..MAX_COORD_VALUE]
            intval = ReadUBitLong(COORD_INTEGER_BITS) + 1;
        }

        // If there's a fraction, read it in
        if (fractval)
        {
            fractval = ReadUBitLong(COORD_FRACTIONAL_BITS);
        }

        // Calculate the correct floating point value
        value = intval + ((float)fractval * COORD_RESOLUTION);

        // Fixup the sign if negative.
        if (signbit)
            value = -value;
    }

    return value;
}

float CBitRead::ReadBitCoordMP(EBitCoordType coordType)
{
    bool bIntegral = (coordType == kCW_Integral);
    bool bLowPrecision = (coordType == kCW_LowPrecision);

    int intval = 0, fractval = 0, signbit = 0;
    float value = 0.0;

    bool bInBounds = ReadOneBit() ? true : false;

    if (bIntegral)
    {
        // Read the required integer and fraction flags
        intval = ReadOneBit();
        // If we got either parse them, otherwise it's a zero.
        if (intval)
        {
            // Read the sign bit
            signbit = ReadOneBit();

            // If there's an integer, read it in
            // Adjust the integers from [0..MAX_COORD_VALUE-1] to
            // [1..MAX_COORD_VALUE]
            if (bInBounds)
            {
                value = (float)(ReadUBitLong(COORD_INTEGER_BITS_MP) + 1);
            }
            else
            {
                value = (float)(ReadUBitLong(COORD_INTEGER_BITS) + 1);
            }
        }
    }
    else
    {
        // Read the required integer and fraction flags
        intval = ReadOneBit();

        // Read the sign bit
        signbit = ReadOneBit();

        // If we got either parse them, otherwise it's a zero.
        if (intval)
        {
            if (bInBounds)
            {
                intval = ReadUBitLong(COORD_INTEGER_BITS_MP) + 1;
            }
            else
            {
                intval = ReadUBitLong(COORD_INTEGER_BITS) + 1;
            }
        }

        // If there's a fraction, read it in
        fractval = ReadUBitLong(bLowPrecision ?
                                    COORD_FRACTIONAL_BITS_MP_LOWPRECISION :
                                    COORD_FRACTIONAL_BITS);

        // Calculate the correct floating point value
        value = intval + ((float)fractval * (bLowPrecision ?
                                                 COORD_RESOLUTION_LOWPRECISION :
                                                 COORD_RESOLUTION));
    }

    // Fixup the sign if negative.
    if (signbit)
        value = -value;

    return value;
}

float CBitRead::ReadBitCellCoord(int bits, EBitCoordType coordType)
{
    bool bIntegral = (coordType == kCW_Integral);
    bool bLowPrecision = (coordType == kCW_LowPrecision);

    int intval = 0, fractval = 0;
    float value = 0.0;

    if (bIntegral)
    {
        value = (float)(ReadUBitLong(bits));
    }
    else
    {
        intval = ReadUBitLong(bits);

        // If there's a fraction, read it in
        fractval = ReadUBitLong(bLowPrecision ?
                                    COORD_FRACTIONAL_BITS_MP_LOWPRECISION :
                                    COORD_FRACTIONAL_BITS);

        // Calculate the correct floating point value
        value = intval + ((float)fractval * (bLowPrecision ?
                                                 COORD_RESOLUTION_LOWPRECISION :
                                                 COORD_RESOLUTION));
    }

    return value;
}

void CBitRead::ReadBitVec3Coord(Vector& fa)
{
    int xflag, yflag, zflag;

    // This vector must be initialized! Otherwise, If any of the flags aren't
    // set, the corresponding component will not be read and will be stack
    // garbage.
    fa.Init(0, 0, 0);

    xflag = ReadOneBit();
    yflag = ReadOneBit();
    zflag = ReadOneBit();

    if (xflag)
        fa.x = ReadBitCoord();
    if (yflag)
        fa.y = ReadBitCoord();
    if (zflag)
        fa.z = ReadBitCoord();
}

float CBitRead::ReadBitNormal(void)
{
    // Read the sign bit
    int signbit = ReadOneBit();

    // Read the fractional part
    unsigned int fractval = ReadUBitLong(NORMAL_FRACTIONAL_BITS);

    // Calculate the correct floating point value
    float value = (float)fractval * NORMAL_RESOLUTION;

    // Fixup the sign if negative.
    if (signbit)
        value = -value;

    return value;
}

void CBitRead::ReadBitVec3Normal(Vector& fa)
{
    int xflag = ReadOneBit();
    int yflag = ReadOneBit();

    if (xflag)
        fa.x = ReadBitNormal();
    else
        fa.x = 0.0f;

    if (yflag)
        fa.y = ReadBitNormal();
    else
        fa.y = 0.0f;

    // The first two imply the third (but not its sign)
    int znegative = ReadOneBit();

    float fafafbfb = fa.x * fa.x + fa.y * fa.y;
    if (fafafbfb < 1.0f)
        fa.z = sqrt(1.0f - fafafbfb);
    else
        fa.z = 0.0f;

    if (znegative)
        fa.z = -fa.z;
}

void CBitRead::ReadBitAngles(QAngle& fa)
{
    Vector tmp;
    ReadBitVec3Coord(tmp);
    fa.Init(tmp.x, tmp.y, tmp.z);
}

float CBitRead::ReadBitFloat(void)
{
    uint32 nvalue = ReadUBitLong(32);

    float ret;
    memcpy(&ret, &nvalue, sizeof(float));

    return ret;
}

#define LZSS_LOOKSHIFT 4
#define LZSS_LOOKAHEAD (1 << LZSS_LOOKSHIFT)

//-----------------------------------------------------------------------------
// Returns true if buffer is compressed.
//-----------------------------------------------------------------------------
bool CLZSS::IsCompressed(unsigned char* pInput)
{
    lzss_header_t* pHeader = (lzss_header_t*)pInput;
    if (pHeader && pHeader->id == LZSS_ID)
    {
        return true;
    }

    // unrecognized
    return false;
}

//-----------------------------------------------------------------------------
// Returns uncompressed size of compressed input buffer. Used for allocating
// output buffer for decompression. Returns 0 if input buffer is not compressed.
//-----------------------------------------------------------------------------
unsigned int CLZSS::GetActualSize(unsigned char* pInput)
{
    lzss_header_t* pHeader = (lzss_header_t*)pInput;
    if (pHeader && pHeader->id == LZSS_ID)
    {
        return LittleLong(pHeader->actualSize);
    }

    // unrecognized
    return 0;
}

void CLZSS::BuildHash(unsigned char* pData)
{
    lzss_list_t* pList;
    lzss_node_t* pTarget;

    unsigned int i;
    memcpy(&i, pData, sizeof(i));

    int targetindex = i & (m_nWindowSize - 1);
    pTarget = &m_pHashTarget[targetindex];
    if (pTarget->pData)
    {
        pList = &m_pHashTable[*pTarget->pData];
        if (pTarget->pPrev)
        {
            pList->pEnd = pTarget->pPrev;
            pTarget->pPrev->pNext = 0;
        }
        else
        {
            pList->pEnd = 0;
            pList->pStart = 0;
        }
    }

    pList = &m_pHashTable[*pData];
    pTarget->pData = pData;
    pTarget->pPrev = 0;
    pTarget->pNext = pList->pStart;
    if (pList->pStart)
    {
        pList->pStart->pPrev = pTarget;
    }
    else
    {
        pList->pEnd = pTarget;
    }
    pList->pStart = pTarget;
}

unsigned char* CLZSS::CompressNoAlloc(unsigned char* pInput,
                                      int inputLength,
                                      unsigned char* pOutputBuf,
                                      unsigned int* pOutputSize)
{
    if ((unsigned int)inputLength <= sizeof(lzss_header_t) + 8)
    {
        return NULL;
    }

    // create the compression work buffers, small enough (~64K) for stack
    m_pHashTable = (lzss_list_t*)alloca(256 * sizeof(lzss_list_t));
    memset(m_pHashTable, 0, 256 * sizeof(lzss_list_t));
    m_pHashTarget = (lzss_node_t*)alloca(m_nWindowSize * sizeof(lzss_node_t));
    memset(m_pHashTarget, 0, m_nWindowSize * sizeof(lzss_node_t));

    // allocate the output buffer, compressed buffer is expected to be less,
    // caller will free
    unsigned char* pStart = pOutputBuf;
    // prevent compression failure (inflation), leave enough to allow dribble
    // eof bytes
    unsigned char* pEnd = pStart + inputLength - sizeof(lzss_header_t) - 8;

    // set the header
    lzss_header_t* pHeader = (lzss_header_t*)pStart;
    pHeader->id = LZSS_ID;
    pHeader->actualSize = LittleLong(inputLength);

    unsigned char* pOutput = pStart + sizeof(lzss_header_t);
    unsigned char* pLookAhead = pInput;
    unsigned char* pWindow = pInput;
    unsigned char* pEncodedPosition = NULL;
    unsigned char* pCmdByte = NULL;
    int putCmdByte = 0;

    while (inputLength > 0)
    {
        pWindow = pLookAhead - m_nWindowSize;
        if (pWindow < pInput)
        {
            pWindow = pInput;
        }

        if (!putCmdByte)
        {
            pCmdByte = pOutput++;
            *pCmdByte = 0;
        }
        putCmdByte = (putCmdByte + 1) & 0x07;

        int encodedLength = 0;
        int lookAheadLength = inputLength < LZSS_LOOKAHEAD ? inputLength :
                                                             LZSS_LOOKAHEAD;

        lzss_node_t* pHash = m_pHashTable[pLookAhead[0]].pStart;
        while (pHash)
        {
            int matchLength = 0;
            int length = lookAheadLength;
            while (length-- &&
                   pHash->pData[matchLength] == pLookAhead[matchLength])
            {
                matchLength++;
            }
            if (matchLength > encodedLength)
            {
                encodedLength = matchLength;
                pEncodedPosition = pHash->pData;
            }
            if (matchLength == lookAheadLength)
            {
                break;
            }
            pHash = pHash->pNext;
        }

        if (encodedLength >= 3)
        {
            *pCmdByte = (*pCmdByte >> 1) | 0x80;
            *pOutput++ = ((pLookAhead - pEncodedPosition - 1) >>
                          LZSS_LOOKSHIFT);
            *pOutput++ = ((pLookAhead - pEncodedPosition - 1)
                          << LZSS_LOOKSHIFT) |
                         (encodedLength - 1);
        }
        else
        {
            encodedLength = 1;
            *pCmdByte = (*pCmdByte >> 1);
            *pOutput++ = *pLookAhead;
        }

        for (int i = 0; i < encodedLength; i++)
        {
            BuildHash(pLookAhead++);
        }

        inputLength -= encodedLength;

        if (pOutput >= pEnd)
        {
            // compression is worse, abandon
            return NULL;
        }
    }

    if (inputLength != 0)
    {
        // unexpected failure
        assert(0);
        return NULL;
    }

    if (!putCmdByte)
    {
        pCmdByte = pOutput++;
        *pCmdByte = 0x01;
    }
    else
    {
        *pCmdByte = ((*pCmdByte >> 1) | 0x80) >> (7 - putCmdByte);
    }

    *pOutput++ = 0;
    *pOutput++ = 0;

    if (pOutputSize)
    {
        *pOutputSize = pOutput - pStart;
    }

    return pStart;
}

//-----------------------------------------------------------------------------
// Compress an input buffer. Caller must free output compressed buffer.
// Returns NULL if compression failed (i.e. compression yielded worse results)
//-----------------------------------------------------------------------------
unsigned char* CLZSS::Compress(unsigned char* pInput,
                               int inputLength,
                               unsigned int* pOutputSize)
{
    unsigned char* pStart = (unsigned char*)malloc(inputLength);
    unsigned char* pFinal =
        CompressNoAlloc(pInput, inputLength, pStart, pOutputSize);
    if (!pFinal)
    {
        free(pStart);
        return NULL;
    }

    return pStart;
}

//-----------------------------------------------------------------------------
// Uncompress a buffer, Returns the uncompressed size. Caller must provide an
// adequate sized output buffer or memory corruption will occur.
//-----------------------------------------------------------------------------
unsigned int CLZSS::Uncompress(unsigned char* pInput, unsigned char* pOutput)
{
    unsigned int totalBytes = 0;
    int cmdByte = 0;
    int getCmdByte = 0;

    unsigned int actualSize = GetActualSize(pInput);
    if (!actualSize)
    {
        // unrecognized
        return 0;
    }

    pInput += sizeof(lzss_header_t);

    for (;;)
    {
        if (!getCmdByte)
        {
            cmdByte = *pInput++;
        }
        getCmdByte = (getCmdByte + 1) & 0x07;

        if (cmdByte & 0x01)
        {
            int position = *pInput++ << LZSS_LOOKSHIFT;
            position |= (*pInput >> LZSS_LOOKSHIFT);
            int count = (*pInput++ & 0x0F) + 1;
            if (count == 1)
            {
                break;
            }
            unsigned char* pSource = pOutput - position - 1;
            for (int i = 0; i < count; i++)
            {
                *pOutput++ = *pSource++;
            }
            totalBytes += count;
        }
        else
        {
            *pOutput++ = *pInput++;
            totalBytes++;
        }
        cmdByte = cmdByte >> 1;
    }

    if (totalBytes != actualSize)
    {
        // unexpected failure
        assert(0);
        return 0;
    }

    return totalBytes;
}

CLZSS::CLZSS(int nWindowSize)
{
    m_nWindowSize = nWindowSize;
}
