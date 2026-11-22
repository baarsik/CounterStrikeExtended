/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#pragma once

typedef enum _fieldtypes
{
	FIELD_FLOAT = 0,		// Any floating point value
	FIELD_STRING,			// A string ID (return from ALLOC_STRING)
	FIELD_ENTITY,			// An entity offset (EOFFSET)
	FIELD_CLASSPTR,			// CBaseEntity *
	FIELD_EHANDLE,			// Entity handle
	FIELD_EVARS,			// EVARS *
	FIELD_EDICT,			// edict_t *, or edict_t *  (same thing)
	FIELD_VECTOR,			// Any vector
	FIELD_POSITION_VECTOR,	// A world coordinate (these are fixed up across level transitions automagically)
	FIELD_POINTER,			// Reserved legacy value; owned save metadata rejects raw pointers.
	FIELD_INTEGER,			// Any integer or enum
	FIELD_FUNCTION,			// A class function pointer (Think, Use, etc)
	FIELD_BOOLEAN,			// boolean, implemented as an int, I may use this as a hint for compression
	FIELD_SHORT,			// 2 byte integer
	FIELD_CHARACTER,		// a byte
	FIELD_TIME,				// a floating point time (these are fixed up automatically too!)
	FIELD_MODELNAME,		// Engine string that is a model name (needs precache)
	FIELD_SOUNDNAME,		// Engine string that is a sound name (needs precache)

	FIELD_TYPECOUNT,		// MUST BE LAST
} FIELDTYPE;

#if !defined(offsetof)  && !defined(GNUC)
#define offsetof(s,m)	(size_t)&(((s *)0)->m)
#endif

#include "save_field_storage.h"

#ifdef __cplusplus
template<typename Storage, FIELDTYPE Kind>
constexpr unsigned int CseSaveNativeStride() {
    constexpr auto stride = cse::save::NativeStorageTraits<Storage>::Stride;
    static_assert(Kind != FIELD_POINTER, "Raw pointers require a symbolic save representation");
    static_assert(stride <= UINT32_MAX, "Native save-field stride exceeds its descriptor width");
    static_assert((Kind != FIELD_STRING && Kind != FIELD_MODELNAME && Kind != FIELD_SOUNDNAME)
                      || stride == sizeof(string_t),
                  "Saved engine strings must use native string_t storage");
    static_assert(Kind != FIELD_ENTITY || stride == sizeof(EOFFSET),
                  "Saved entity offsets must use native EOFFSET storage");
    return static_cast<unsigned int>(Kind == FIELD_CHARACTER ? sizeof(char)
        : ((Kind == FIELD_VECTOR || Kind == FIELD_POSITION_VECTOR)
            ? sizeof(float) * CSE_SAVE_VECTOR_COMPONENTS : stride));
}

#define CSE_SAVE_NATIVE_STRIDE(type, name, fieldtype, count) \
    CseSaveNativeStride<decltype(((type *)nullptr)->name), fieldtype>()
#define CSE_SAVE_NATIVE_FLAGS(type, name) \
    cse::save::NativeStorageTraits<decltype(((type *)nullptr)->name)>::Flags
#else
#define CSE_SAVE_NATIVE_STRIDE(type, name, fieldtype, count) \
    ((fieldtype) == FIELD_CHARACTER ? sizeof(char) \
        : (((fieldtype) == FIELD_VECTOR || (fieldtype) == FIELD_POSITION_VECTOR) \
            ? sizeof(float) * CSE_SAVE_VECTOR_COMPONENTS : sizeof(((type *)0)->name) / (count)))
#define CSE_SAVE_NATIVE_FLAGS(type, name) 0
#endif

#define _FIELD(type, name, fieldtype, count, flags) \
    { fieldtype, #name, offsetof(type, name), count, flags, \
        CSE_SAVE_NATIVE_STRIDE(type, name, fieldtype, count), CSE_SAVE_NATIVE_FLAGS(type, name), \
        sizeof(((type *)0)->name) }
#define DEFINE_FIELD(type,name,fieldtype)			_FIELD(type, name, fieldtype, 1, 0)
#define DEFINE_ARRAY(type,name,fieldtype,count)		_FIELD(type, name, fieldtype, count, 0)
#define DEFINE_ENTITY_FIELD(name,fieldtype)			_FIELD(entvars_t, name, fieldtype, 1, 0 )
#define DEFINE_ENTITY_GLOBAL_FIELD(name,fieldtype)	_FIELD(entvars_t, name, fieldtype, 1, FTYPEDESC_GLOBAL )
#define DEFINE_GLOBAL_FIELD(type,name,fieldtype)	_FIELD(type, name, fieldtype, 1, FTYPEDESC_GLOBAL )


#define FTYPEDESC_GLOBAL			0x0001		// This field is masked for global entity save/restore

typedef struct
{
    FIELDTYPE fieldType;
    const char *fieldName;
    int fieldOffset;
    short fieldSize;
    short flags;
    // Native storage facts never change the legacy field's encoded width.
    unsigned int nativeStride;
    unsigned int nativeFlags;
    unsigned int nativeBytes;
} TYPEDESCRIPTION;
