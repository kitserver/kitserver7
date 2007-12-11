#include "crc32.h"

#define QUOTIENT 0x04c11db7

// This function computes CRC32 of the encoded buffer
DWORD Sign(PACKED_BIN* buffer)
{
    DWORD len = buffer->header.sizePacked;
    BYTE* data = buffer->data;
    return GetCRC(data,len);
}

DWORD GetCRC(BYTE* data, DWORD len)
{
    DWORD result;
    int i,j;
    BYTE octet;
    
    result = -1;
    
    for (i=0; i<len; i++)
    {
        octet = *(data++);
        for (j=0; j<8; j++)
        {
            if ((octet >> 7) ^ (result >> 31))
            {
                result = (result << 1) ^ QUOTIENT;
            }
            else
            {
                result = (result << 1);
            }
            octet <<= 1;
        }
    }
    
    return ~result; // The complement of the remainder
}
