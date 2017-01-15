/*      CWPack - cwpack.c   */
/*
 The MIT License (MIT)
 
 Copyright (c) 2017 Claes Wihlborg
 
 Permission is hereby granted, free of charge, to any person obtaining a copy of this
 software and associated documentation files (the "Software"), to deal in the Software
 without restriction, including without limitation the rights to use, copy, modify,
 merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 persons to whom the Software is furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */


#include <stdlib.h>
#include <string.h>

#include "cwpack.h"



/*************************   B Y T E   O R D E R   ****************************/

/*
 * The pack/unpack routines are written to be insensitive to byte order, but as
 * you can get some speed gain if the byte order is known, we try that when
 * we can certainly detect it.
 * Define COMPILE_FOR_BIG_ENDIAN or COMPILE_FOR_LITTLE_ENDIAN if you know.
 */


#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && defined(__ORDER_BIG_ENDIAN__)

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define COMPILE_FOR_BIG_ENDIAN
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define COMPILE_FOR_LITTLE_ENDIAN
#endif

#elif defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && defined(__BIG_ENDIAN)

#if __BYTE_ORDER == __BIG_ENDIAN
#define COMPILE_FOR_BIG_ENDIAN
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define COMPILE_FOR_LITTLE_ENDIAN
#endif

#elif defined(__BIG_ENDIAN__)
#define COMPILE_FOR_BIG_ENDIAN

#elif defined(__LITTLE_ENDIAN__)
#define COMPILE_FOR_LITTLE_ENDIAN

#elif defined(__i386__) || defined(__x86_64__)
#define COMPILE_FOR_LITTLE_ENDIAN

#endif



static int test_byte_order(void)
{
#ifdef COMPILE_FOR_BIG_ENDIAN
    const char *endianness = "1234";
    if (*(uint32_t*)endianness != 0x31323334UL)
        return CWP_RC_WRONG_BYTE_ORDER;
#else

#ifdef COMPILE_FOR_LITTLE_ENDIAN
    const char *endianness = "1234";
    if (*(uint32_t*)endianness != 0x34333231UL)
        return CWP_RC_WRONG_BYTE_ORDER;
#endif
#endif
    return CWP_RC_OK;
}


/*******************************   P A C K   **********************************/



int cw_pack_context_init (cw_pack_context* pack_context, void* data, unsigned long length, int (*hpo)(struct cw_pack_context*, unsigned long))
{
    pack_context->return_code = test_byte_order();
    if (pack_context->return_code)
        return pack_context->return_code;
    pack_context->start = pack_context->current = (uint8_t*)data;
    pack_context->end = pack_context->start + length;
    pack_context->return_code = 0;
    pack_context->errno = 0;
    pack_context->handle_pack_overflow = hpo;
    return CWP_RC_OK;
}


/*  Packing routines  --------------------------------------------------------------------------------  */


#ifdef COMPILE_FOR_BIG_ENDIAN

#define cw_store16(x)  *(uint16_t*)pack_context->current = *(uint16_t*)&x; pack_context->current += 2
#define cw_store32(x)  *(uint32_t*)pack_context->current = *(uint32_t*)&x; pack_context->current += 4
#define cw_store64(x)  *(uint64_t*)pack_context->current = *(uint64_t*)&x; pack_context->current += 8

#else	/* Byte order little endian or undetermined */

#define cw_store16(d)                                   \
    *(pack_context->current)++ = (uint8_t)(d >> 8);     \
    *(pack_context->current)++ = (uint8_t)d;

#ifdef COMPILE_FOR_LITTLE_ENDIAN

#define cw_store32(x)									\
	*(uint32_t*)pack_context->current =                 \
	((uint32_t)((((uint32_t)(x)) >> 24) |				\
	(((uint32_t)(x) & 0x00ff0000) >>  8) |				\
	(((uint32_t)(x) & 0x0000ff00) <<  8) |				\
	(((uint32_t)(x)) << 24)));                          \
    pack_context->current += 4;

#define cw_store64(x)									\
	*(uint64_t*)pack_context->current =                 \
	((uint64_t)(										\
	(((((uint64_t)(x)) >> 40) |							\
	(((uint64_t)(x)) << 24)) & 0x0000ff000000ff00ULL) | \
	(((((uint64_t)(x)) >> 24) |							\
	(((uint64_t)(x)) << 40)) & 0x00ff000000ff0000ULL) | \
	(((uint64_t)(x) & 0x000000ff00000000ULL) >>  8) |	\
	(((uint64_t)(x) & 0x00000000ff000000ULL) <<  8) |	\
	(((uint64_t)(x)) >> 56) |							\
    (((uint64_t)(x)) << 56)));                          \
    pack_context->current += 8;

#else   /* Byte order undetermined */

#define cw_store32(d)                                   \
	*(pack_context->current)++ = (uint8_t)(d >> 24);    \
	*(pack_context->current)++ = (uint8_t)(d >> 16);    \
	*(pack_context->current)++ = (uint8_t)(d >> 8);     \
	*(pack_context->current)++ = (uint8_t)d;

#define cw_store64(z)                                   \
	*(pack_context->current)++ = (uint8_t)(z >> 56);    \
	*(pack_context->current)++ = (uint8_t)(z >> 48);    \
	*(pack_context->current)++ = (uint8_t)(z >> 40);    \
	*(pack_context->current)++ = (uint8_t)(z >> 32);    \
	*(pack_context->current)++ = (uint8_t)(z >> 24);    \
	*(pack_context->current)++ = (uint8_t)(z >> 16);    \
	*(pack_context->current)++ = (uint8_t)(z >> 8);     \
	*(pack_context->current)++ = (uint8_t)z;
#endif
#endif



#define cw_pack_reserve_space(more)                                                             \
    if ((unsigned long)(more) > (unsigned long)(pack_context->end - pack_context->current))     \
        {                                                                                       \
            if (!pack_context->handle_pack_overflow)                                            \
            {                                                                                   \
                pack_context->return_code = CWP_RC_BUFFER_OVERFLOW;                             \
                return CWP_RC_BUFFER_OVERFLOW;                                                  \
            }                                                                                   \
            int rc = pack_context->handle_pack_overflow (pack_context, (unsigned long)more);    \
            if (rc) {                                                                           \
                pack_context->return_code = rc;                                                 \
                return rc;                                                                      \
            }                                                                                   \
        }


#define tryMove0(t)										\
{														\
    cw_pack_reserve_space(1)                            \
	*pack_context->current++ = (uint8_t)(t);			\
	return CWP_RC_OK;									\
}

#define tryMove1(t,d)									\
{														\
    cw_pack_reserve_space(2)                            \
	*pack_context->current++ = (uint8_t)t;				\
	*pack_context->current++ = (uint8_t)d;				\
	return CWP_RC_OK;									\
}

#define tryMove2(t,d)									\
{														\
    cw_pack_reserve_space(3)                            \
	*pack_context->current++ = (uint8_t)t;				\
	cw_store16(d);									    \
	return CWP_RC_OK;									\
}

#define tryMove4(t,d)									\
{														\
    cw_pack_reserve_space(5)                            \
	*pack_context->current++ = (uint8_t)t;				\
	cw_store32(d);										\
	return CWP_RC_OK;									\
}

#define tryMove8(t,d)									\
{														\
    cw_pack_reserve_space(9)                            \
	*pack_context->current++ = (uint8_t)t;				\
	cw_store64(d);										\
	return CWP_RC_OK;									\
}


int cw_pack_unsigned(cw_pack_context* pack_context, uint64_t i)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
	if (i < 128)
		tryMove0(i);

    if (i < 256)
        tryMove1(0xcc, i);

    if (i < 0x10000L)
        tryMove2(0xcd, i);

    if (i < 0x100000000LL)
        tryMove4(0xce, i);

    tryMove8(0xcf,i);
}


int cw_pack_signed(cw_pack_context* pack_context, int64_t i)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
    if (i >= 0)
        return cw_pack_unsigned(pack_context, (uint64_t)i);
    
	if (i >= -32)
		tryMove0(i);

    if (i >= -128)
        tryMove1(0xd0, i);
    
    if (i >= -32768)
        tryMove2(0xd1,i);

    if (i >= (int64_t)0xffffffff80000000LL)
        tryMove4(0xd2,i);

    tryMove8(0xd3,i);
}


int cw_pack_float(cw_pack_context* pack_context, float f)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
	uint32_t tmp = *((uint32_t*)&f);
	tryMove4(0xca,tmp);
}


int cw_pack_double(cw_pack_context* pack_context, double d)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
	uint64_t tmp = *((uint64_t*)&d);
	tryMove8(0xcb,tmp);
}


int cw_pack_nil(cw_pack_context* pack_context)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
	tryMove0(0xc0);
}


int cw_pack_boolean(cw_pack_context* pack_context, bool b)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
	tryMove0(b? 0xc3: 0xc2);
}


int cw_pack_array_size(cw_pack_context* pack_context, uint32_t n)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
	if (n < 16)
		tryMove0(0x90 | n);

    if (n < 65536)
		tryMove2(0xdc, n);

    tryMove4(0xdd, n);
}


int cw_pack_map_size(cw_pack_context* pack_context, uint32_t n)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
	if (n < 16)
		tryMove0(0x80 | n);

    if (n < 65536)
		tryMove2(0xde, n);

    tryMove4(0xdf, n);
}


int cw_pack_str(cw_pack_context* pack_context, const char* v, uint32_t l)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
    if (l < 32)             // Fixstr
    {
        cw_pack_reserve_space(l+1);
        *(pack_context->current)++ = (uint8_t)(0xa0 + l);
        if (l==0)
            return CWP_RC_OK;
    }
    else if (l < 256)       // Str 8
    {
        cw_pack_reserve_space(l+2);
        *pack_context->current++ = (uint8_t)(0xd9);
        *pack_context->current++ = (uint8_t)(l);
    }
    else if (l < 65536)     // Str 16
    {
        cw_pack_reserve_space(l+3)
        *pack_context->current++ = (uint8_t)0xda;
        cw_store16(l);
    }
    else                    // Str 32
    {
        cw_pack_reserve_space(l+5)
        *pack_context->current++ = (uint8_t)0xdb;
        cw_store32(l);
    }
    memcpy(pack_context->current,v,l);
    pack_context->current += l;
    return CWP_RC_OK;
}


int cw_pack_bin(cw_pack_context* pack_context, const void* v, uint32_t l)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
    if (l < 256)            // Bin 8
    {
        cw_pack_reserve_space(l+2);
        *pack_context->current++ = (uint8_t)(0xc4);
        *pack_context->current++ = (uint8_t)(l);
    }
    else if (l < 65536)     // Bin 16
    {
        cw_pack_reserve_space(l+3)
        *pack_context->current++ = (uint8_t)0xc5;
        cw_store16(l);
    }
    else                    // Bin 32
    {
        cw_pack_reserve_space(l+5)
        *pack_context->current++ = (uint8_t)0xc6;
        cw_store32(l);
    }
    memcpy(pack_context->current,v,l);
    pack_context->current += l;
    return CWP_RC_OK;
}


int cw_pack_ext (cw_pack_context* pack_context, int8_t type, const void* v, uint32_t l)
{
    if (pack_context->return_code)
        return CWP_RC_STOPPED;
    
    switch (l)
    {
        case 1:                                         // Fixext 1
            cw_pack_reserve_space(3);
            *pack_context->current++ = (uint8_t)0xd4;
            *pack_context->current++ = (uint8_t)type;
            *pack_context->current++ = *(uint8_t*)v;
            return CWP_RC_OK;
        case 2:                                         // Fixext 2
            cw_pack_reserve_space(4);
            *pack_context->current++ = (uint8_t)0xd5;
            break;
        case 4:                                         // Fixext 4
            cw_pack_reserve_space(6);
            *pack_context->current++ = (uint8_t)0xd6;
            break;
        case 8:                                         // Fixext 8
            cw_pack_reserve_space(10);
            *pack_context->current++ = (uint8_t)0xd7;
            break;
        case 16:                                        // Fixext16
            cw_pack_reserve_space(18);
            *pack_context->current++ = (uint8_t)0xd8;
            break;
        default:
            if (l < 256)                                // Ext 8
            {
                cw_pack_reserve_space(l+3);
                *pack_context->current++ = (uint8_t)0xc7;
                *pack_context->current++ = (uint8_t)(l);
            }
            else if (l < 65536)                         // Ext 16
            {
                cw_pack_reserve_space(l+4)
                *pack_context->current++ = (uint8_t)0xc8;
                cw_store16(l);
            }
            else                                        // Ext 32
            {
                cw_pack_reserve_space(l+6)
                *pack_context->current++ = (uint8_t)0xc9;
                cw_store32(l);
            }
    }
    *pack_context->current++ = (uint8_t)type;
    memcpy(pack_context->current,v,l);
    pack_context->current += l;
    return CWP_RC_OK;
}



/*******************************   U N P A C K   **********************************/


int cw_unpack_context_init (cw_unpack_context* unpack_context, void* data, unsigned long len, int (*huu)(struct cw_unpack_context*, unsigned long))
{
    unpack_context->return_code = test_byte_order();
    if (unpack_context->return_code)
        return unpack_context->return_code;
    unpack_context->start = unpack_context->current = (uint8_t*)data;
    unpack_context->end = unpack_context->start + len;
    unpack_context->return_code = 0;
    unpack_context->errno = 0;
    unpack_context->handle_unpack_underflow = huu;
    return CWP_RC_OK;
}


/*  Unpacking routines  ----------------------------------------------------------  */

#ifdef COMPILE_FOR_BIG_ENDIAN

#define cw_load16  tmpu16 = *(uint16_t*)unpack_context->current; unpack_context->current += 2
#define cw_load32  tmpu32 = *(uint32_t*)unpack_context->current; unpack_context->current += 4
#define cw_load64(z)  unpack_context->item.as.u64 = *(uint64_t*)unpack_context->current; unpack_context->current += 4

#else	/* Byte order little endian or undetermined */

#define cw_load16                                               \
    tmpu16 = (uint16_t)((*unpack_context->current++) << 8);     \
    tmpu16 |= (uint16_t)(*unpack_context->current++)

#define cw_load32                                               \
    tmpu32 = (uint32_t)((*unpack_context->current++) << 24);   \
    tmpu32 |= (uint32_t)((*unpack_context->current++) << 16);   \
    tmpu32 |= (uint32_t)((*unpack_context->current++) << 8);    \
    tmpu32 |= (uint32_t)(*unpack_context->current++)

#ifdef COMPILE_FOR_LITTLE_ENDIAN

#define cw_load64(q)                                            \
    tmpu64 = *((uint64_t*)q);                                   \
    q += 8;                                                     \
    unpack_context->item.as.u64 =                   (           \
        (((tmpu64 >> 40) |                                      \
        (tmpu64 << 24)) & 0x0000ff000000ff00ULL) |              \
        (((tmpu64 >> 24) |                                      \
        (tmpu64 << 40)) & 0x00ff000000ff0000ULL) |              \
        ((tmpu64 & 0x000000ff00000000ULL) >>  8) |              \
        ((tmpu64 & 0x00000000ff000000ULL) <<  8) |              \
        (tmpu64 >> 56) |                                        \
        (tmpu64 << 56)                              )

#else /* Byte order undetermined */

#define cw_load64(q)                                            \
    unpack_context->item.as.u64 =                   (           \
        tmpu64 = ((uint64_t)*q++) << 56,                        \
        tmpu64 |= ((uint64_t)*q++) << 48,                       \
        tmpu64 |= ((uint64_t)*q++) << 40,                       \
        tmpu64 |= ((uint64_t)*q++) << 32,                       \
        tmpu64 |= ((uint64_t)*q++) << 24,                       \
        tmpu64 |= ((uint64_t)*q++) << 16,                       \
        tmpu64 |= ((uint64_t)*q++) << 8,                        \
        tmpu64 |= (uint64_t)*q++                    )

#endif
#endif


#define cw_unpack_assert_space(more)                                                    \
    if ((unpack_context->current + (long)(more)) > unpack_context->end)                 \
    {                                                                                   \
        if (!unpack_context->handle_unpack_underflow)                                   \
        {                                                                               \
            unpack_context->return_code = buffer_end_return_code;                       \
            return buffer_end_return_code;                                              \
        }                                                                               \
        int rc = unpack_context->handle_unpack_underflow (unpack_context, (unsigned long)(more)); \
        if (rc != 0)                                                                    \
        {                                                                               \
            if (rc != CWP_RC_END_OF_INPUT)                                              \
                unpack_context->return_code = rc;                                       \
            else                                                                        \
                unpack_context->return_code = buffer_end_return_code;                   \
            return unpack_context->return_code;                                         \
        }                                                                               \
    }


#define getDDItem(typ,var,val)                                              \
    unpack_context->item.type = typ;                                        \
    unpack_context->item.as.var = val;

#define getDDItem1(typ,var,cast)                                            \
    unpack_context->item.type = typ;                                        \
    cw_unpack_assert_space(1);                                              \
    unpack_context->item.as.var = (cast)*(unpack_context->current)++;

#define getDDItem2(typ,var,cast)                                            \
    unpack_context->item.type = typ;                                        \
    cw_unpack_assert_space(2);                                              \
    cw_load16; unpack_context->item.as.var = (cast)tmpu16;

#define getDDItem4(typ,var,cast)                                            \
    unpack_context->item.type = typ;                                        \
    cw_unpack_assert_space(4);                                              \
    cw_load32;unpack_context->item.as.var = (cast)tmpu32;

#define getDDItem8(typ)                                                     \
    unpack_context->item.type = typ;                                        \
    cw_unpack_assert_space(8);                                              \
    cw_load64(unpack_context->current);

#define getDDItemFix(len)                                                   \
    cw_unpack_assert_space(1);                                              \
    unpack_context->item.type = *(int8_t*)unpack_context->current++;        \
    unpack_context->item.as.ext.length = len;                               \
    cw_unpack_assert_blob(ext);


#define cw_unpack_assert_blob(blob)                                         \
    cw_unpack_assert_space(unpack_context->item.as.blob.length);            \
    unpack_context->item.as.blob.start = unpack_context->current;           \
    unpack_context->current += unpack_context->item.as.blob.length;         \
    break;


int cw_unpack_next (cw_unpack_context* unpack_context)
{
    if (unpack_context->return_code)
        return CWP_RC_STOPPED;

#ifndef COMPILE_FOR_BIG_ENDIAN
    uint64_t    tmpu64;
#endif
    uint32_t    tmpu32;
    uint16_t    tmpu16;
    
    int buffer_end_return_code = CWP_RC_END_OF_INPUT;
    cw_unpack_assert_space(1);
    uint8_t c = *unpack_context->current++;
    
    buffer_end_return_code = CWP_RC_BUFFER_UNDERFLOW;
    switch (c)
    {
        case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
        case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
        case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
        case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
        case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
        case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
        case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
        case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
        case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
        case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
        case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
        case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
        case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
        case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
                    getDDItem(CWP_ITEM_POSITIVE_INTEGER, i64, c);		break;  // positive fixnum
        case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
        case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
                    getDDItem(CWP_ITEM_MAP, map.size, c & 0x0f);		break;  // fixmap
        case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
        case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
                    getDDItem(CWP_ITEM_ARRAY, array.size, c & 0x0f);	break;	// fixarray
        case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
        case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
        case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
        case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
                    getDDItem(CWP_ITEM_STR, str.length, c & 0x1f);              // fixraw
                    cw_unpack_assert_blob(str);
        case 0xc0:  unpack_context->item.type = CWP_ITEM_NIL;			break;	// nil
        case 0xc2:  getDDItem(CWP_ITEM_BOOLEAN, boolean, false);        break;  // false
        case 0xc3:  getDDItem(CWP_ITEM_BOOLEAN, boolean, true);         break;  // true
        case 0xc4:  getDDItem1(CWP_ITEM_BIN, bin.length, uint8_t);              // bin 8
                    cw_unpack_assert_blob(bin);
        case 0xc5:  getDDItem2(CWP_ITEM_BIN, bin.length, uint16_t);             // bin 16
                    cw_unpack_assert_blob(bin);
        case 0xc6:  getDDItem4(CWP_ITEM_BIN, bin.length, uint32_t);             // bin 32
                    cw_unpack_assert_blob(bin);
        case 0xc7:  getDDItem1(CWP_ITEM_EXT, ext.length, uint8_t);              // ext 8
                    cw_unpack_assert_space(1);
                    unpack_context->item.type = *(int8_t*)(unpack_context->current)++;
                    cw_unpack_assert_blob(ext);
        case 0xc8:  getDDItem2(CWP_ITEM_EXT, ext.length, uint16_t);             // ext 16
                    cw_unpack_assert_space(1);
                    unpack_context->item.type = *(int8_t*)(unpack_context->current)++;
                    cw_unpack_assert_blob(ext);
        case 0xc9:  getDDItem4(CWP_ITEM_EXT, ext.length, uint32_t);             // ext 32
                    cw_unpack_assert_space(1);
                    unpack_context->item.type = *(int8_t*)(unpack_context->current)++;
                    cw_unpack_assert_blob(ext);
        case 0xca:  unpack_context->item.type = CWP_ITEM_FLOAT;                 // float
                    cw_unpack_assert_space(4);
                    cw_load32;
                    unpack_context->item.as.real = *(float*)&tmpu32;					break;
        case 0xcb:  getDDItem8(CWP_ITEM_DOUBLE);						break;	// double
        case 0xcc:  getDDItem1(CWP_ITEM_POSITIVE_INTEGER, u64, uint8_t); break;	// unsigned int  8
        case 0xcd:  getDDItem2(CWP_ITEM_POSITIVE_INTEGER, u64, uint16_t); break; // unsigned int 16
        case 0xce:  getDDItem4(CWP_ITEM_POSITIVE_INTEGER, u64, uint32_t); break; // unsigned int 32
        case 0xcf:  getDDItem8(CWP_ITEM_POSITIVE_INTEGER);				break;	// unsigned int 64
        case 0xd0:  getDDItem1(CWP_ITEM_NEGATIVE_INTEGER, i64, int8_t);         // signed int  8
                    if (unpack_context->item.as.i64 >= 0)
                        unpack_context->item.type = CWP_ITEM_POSITIVE_INTEGER;
                    break;
        case 0xd1:  getDDItem2(CWP_ITEM_NEGATIVE_INTEGER, i64, int16_t);        // signed int 16
                    if (unpack_context->item.as.i64 >= 0)
                        unpack_context->item.type = CWP_ITEM_POSITIVE_INTEGER;
                    break;
        case 0xd2:  getDDItem4(CWP_ITEM_NEGATIVE_INTEGER, i64, int32_t);        // signed int 32
                    if (unpack_context->item.as.i64 >= 0)
                        unpack_context->item.type = CWP_ITEM_POSITIVE_INTEGER;
                    break;
        case 0xd3:  getDDItem8(CWP_ITEM_NEGATIVE_INTEGER);						// signed int 64
                    if (unpack_context->item.as.i64 >= 0)
                        unpack_context->item.type = CWP_ITEM_POSITIVE_INTEGER;
                    break;
        case 0xd4:  getDDItemFix(1);                                            // fixext 1
        case 0xd5:  getDDItemFix(2);                                            // fixext 2
        case 0xd6:  getDDItemFix(4);                                            // fixext 4
        case 0xd7:  getDDItemFix(8);                                            // fixext 8
        case 0xd8:  getDDItemFix(16);                                           // fixext 16
        case 0xd9:  getDDItem1(CWP_ITEM_STR, str.length, uint8_t);              // str 8
                    cw_unpack_assert_blob(str);
        case 0xda:  getDDItem2(CWP_ITEM_STR, str.length, uint16_t);             // str 16
                    cw_unpack_assert_blob(str);
        case 0xdb:  getDDItem4(CWP_ITEM_STR, str.length, uint32_t);             // str 32
                    cw_unpack_assert_blob(str);
        case 0xdc:  getDDItem2(CWP_ITEM_ARRAY, array.size, uint16_t);   break;  // array 16
        case 0xdd:  getDDItem4(CWP_ITEM_ARRAY, array.size, uint32_t);   break;  // array 32
        case 0xde:  getDDItem2(CWP_ITEM_MAP, map.size, uint16_t);       break;  // map 16
        case 0xdf:  getDDItem4(CWP_ITEM_MAP, map.size, uint32_t);       break;  // map 32
        case 0xe0: case 0xe1: case 0xe2: case 0xe3: case 0xe4: case 0xe5: case 0xe6: case 0xe7:
        case 0xe8: case 0xe9: case 0xea: case 0xeb: case 0xec: case 0xed: case 0xee: case 0xef:
        case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf7:
        case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
                    getDDItem(CWP_ITEM_NEGATIVE_INTEGER, i64, (int8_t)c); break;    // negative fixnum
        default:
                    unpack_context->return_code = CWP_RC_MALFORMED_INPUT;
                    return CWP_RC_MALFORMED_INPUT;
    }
    
    return CWP_RC_OK;
}

#define cw_skip_bytes(n)                                \
    cw_unpack_assert_space(n);                          \
    unpack_context->current += n;                       \
    break;

int cw_skip_items (cw_unpack_context* unpack_context, long item_count)
{
    if (unpack_context->return_code)
        return CWP_RC_STOPPED;
    
    uint32_t    tmpu32;
    uint16_t    tmpu16;
    
    while (item_count-- > 0)
    {
        int buffer_end_return_code = CWP_RC_END_OF_INPUT;
        cw_unpack_assert_space(1);
        uint8_t c = *unpack_context->current++;

        buffer_end_return_code = CWP_RC_BUFFER_UNDERFLOW;
        switch (c)
        {
            case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
            case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
            case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
            case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
            case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
            case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
            case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
            case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
            case 0x48: case 0x49: case 0x4a: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f:
            case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
            case 0x58: case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f:
            case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
            case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
            case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7a: case 0x7b: case 0x7c: case 0x7d: case 0x7e: case 0x7f:
                                                                // unsigned fixint
            case 0xe0: case 0xe1: case 0xe2: case 0xe3: case 0xe4: case 0xe5: case 0xe6: case 0xe7:
            case 0xe8: case 0xe9: case 0xea: case 0xeb: case 0xec: case 0xed: case 0xee: case 0xef:
            case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf4: case 0xf5: case 0xf6: case 0xf7:
            case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfc: case 0xfd: case 0xfe: case 0xff:
                                                                // signed fixint
            case 0xc0:                                          // nil
            case 0xc2:			                                // false
            case 0xc3:	break;                                  // true
            case 0xcc:											// unsigned int  8
            case 0xd0:	cw_skip_bytes(1);						// signed int  8
            case 0xcd:											// unsigned int 16
            case 0xd1:											// signed int 16
            case 0xd4:	cw_skip_bytes(2);						// fixext 1
            case 0xd5:	cw_skip_bytes(3);						// fixext 2
            case 0xca:											// float
            case 0xce:											// unsigned int 32
            case 0xd2:	cw_skip_bytes(4);						// signed int 32
            case 0xd6:	cw_skip_bytes(5);						// fixext 4
            case 0xcb:											// double
            case 0xcf:											// unsigned int 64
            case 0xd3:	cw_skip_bytes(8);						// signed int 64
            case 0xd7:	cw_skip_bytes(9);						// fixext 8
            case 0xd8:	cw_skip_bytes(17);						// fixext 16
            case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
            case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
            case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
            case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
                cw_skip_bytes(c & 0x1f);                        // fixstr
            case 0xd9:											// str 8
            case 0xc4:											// bin 8
                cw_unpack_assert_space(1);
                tmpu32 = *unpack_context->current++;
                cw_skip_bytes(tmpu32);
                
            case 0xda:											// str 16
            case 0xc5:											// bin 16
                cw_unpack_assert_space(2);
                cw_load16;
                cw_skip_bytes(tmpu16);
                
            case 0xdb:											// str 32
            case 0xc6:											// bin 32
                cw_unpack_assert_space(4);
                cw_load32;
                cw_skip_bytes(tmpu32);
                
            case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
            case 0x88: case 0x89: case 0x8a: case 0x8b: case 0x8c: case 0x8d: case 0x8e: case 0x8f:
                item_count += 2*(c & 15);                       // FixMap
                break;

            case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
            case 0x98: case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: case 0x9f:
                item_count += c & 15;                           // FixArray
                break;
                
            case 0xdc:											// array 16
                cw_unpack_assert_space(2);
                cw_load16;
                item_count += tmpu16;
                break;
                
            case 0xde:											// map 16
                cw_unpack_assert_space(2);
                cw_load16;
                item_count += 2*tmpu16;
                break;
                
            case 0xdd:											// array 32
                cw_unpack_assert_space(4);
                cw_load32;
                item_count += tmpu32;
                break;
                
            case 0xdf:											// map 32
                cw_unpack_assert_space(4);
                cw_load32;
                item_count += 2*tmpu32;
                break;
                
            case 0xc7:											// ext 8
                cw_unpack_assert_space(1);
                tmpu32 = *unpack_context->current++;
                cw_skip_bytes(tmpu32 +1);
                
            case 0xc8:											// ext 16
                cw_unpack_assert_space(2);
                cw_load16;
                cw_skip_bytes(tmpu16 +1);
                
            case 0xc9:											// ext 32
                cw_unpack_assert_space(4);
                cw_load32;
                cw_skip_bytes(tmpu32 +1);
                
            default:                                            // illegal
                unpack_context->return_code = CWP_RC_MALFORMED_INPUT;
                return CWP_RC_MALFORMED_INPUT;
        }
    }
    return CWP_RC_OK;
}

/* end cwpack.c */
