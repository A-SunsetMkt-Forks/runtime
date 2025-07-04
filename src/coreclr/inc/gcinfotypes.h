// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.


#ifndef __GCINFOTYPES_H__
#define __GCINFOTYPES_H__

#ifndef FEATURE_NATIVEAOT
#include "gcinfo.h"
#endif

#ifdef _MSC_VER
#include <intrin.h>
#endif // _MSC_VER

// *****************************************************************************
// WARNING!!!: These values and code are used in the runtime repo and SOS in the
// diagnostics repo. Should updated in a backwards and forwards compatible way.
// See: https://github.com/dotnet/diagnostics/blob/main/src/shared/inc/gcinfotypes.h
//      https://github.com/dotnet/runtime/blob/main/src/coreclr/inc/gcinfotypes.h
// *****************************************************************************

#define PARTIALLY_INTERRUPTIBLE_GC_SUPPORTED

#define FIXED_STACK_PARAMETER_SCRATCH_AREA


#define BITS_PER_SIZE_T ((int)sizeof(size_t)*8)

inline UINT32 CeilOfLog2(size_t x)
{
    // it is ok to use bsr or clz unconditionally
    _ASSERTE(x > 0);

    x = (x << 1) - 1;

#ifdef TARGET_64BIT
#ifdef _MSC_VER
    DWORD result;
    _BitScanReverse64(&result, (unsigned long)x);
    return (UINT32)result;
#else // _MSC_VER
    // LZCNT returns index starting from MSB, whereas BSR gives the index from LSB.
    // 63 ^ LZCNT here is equivalent to 63 - LZCNT since the LZCNT result is always between 0 and 63.
    // This saves an instruction, as subtraction from constant requires either MOV/SUB or NEG/ADD.
    return (UINT32)63 ^ (UINT32)__builtin_clzl((unsigned long)x);
#endif // _MSC_VER
#else // TARGET_64BIT
#ifdef _MSC_VER
    DWORD result;
    _BitScanReverse(&result, (unsigned int)x);
    return (UINT32)result;
#else // _MSC_VER
    return (UINT32)31 ^ (UINT32)__builtin_clz((unsigned int)x);
#endif // _MSC_VER
#endif
}

enum GcSlotFlags
{
    GC_SLOT_BASE      = 0x0,
    GC_SLOT_INTERIOR  = 0x1,
    GC_SLOT_PINNED    = 0x2,
    GC_SLOT_UNTRACKED = 0x4,

    // For internal use by the encoder/decoder
    GC_SLOT_IS_REGISTER = 0x8,
    GC_SLOT_IS_DELETED  = 0x10,
};

enum GcStackSlotBase
{
    GC_CALLER_SP_REL = 0x0,
    GC_SP_REL        = 0x1,
    GC_FRAMEREG_REL  = 0x2,

    GC_SPBASE_FIRST  = GC_CALLER_SP_REL,
    GC_SPBASE_LAST   = GC_FRAMEREG_REL,
};

#ifdef _DEBUG
const char* const GcStackSlotBaseNames[] =
{
    "caller.sp",
    "sp",
    "frame",
};
#endif

enum GcSlotState
{
    GC_SLOT_DEAD = 0x0,
    GC_SLOT_LIVE = 0x1,
};

struct GcStackSlot
{
    INT32 SpOffset;
    GcStackSlotBase Base;

    bool operator==(const GcStackSlot& other)
    {
        return ((SpOffset == other.SpOffset) && (Base == other.Base));
    }
    bool operator!=(const GcStackSlot& other)
    {
        return ((SpOffset != other.SpOffset) || (Base != other.Base));
    }
};

// ReturnKind is not encoded in GCInfo v4 and later, except on x86.

//--------------------------------------------------------------------------------
// ReturnKind -- encoding return type information in GcInfo
//
// When a method is stopped at a call - site for GC (ex: via return-address
// hijacking) the runtime needs to know whether the value is a GC - value
// (gc - pointer or gc - pointers stored in an aggregate).
// It needs this information so that mark - phase can preserve the gc-pointers
// being returned.
//
// The Runtime doesn't need the precise return-type of a method.
// It only needs to find the GC-pointers in the return value.
// The only scenarios currently supported by CoreCLR are:
// 1. Object references
// 2. ByRef pointers
// 3. ARM64/X64 only : Structs returned in two registers
// 4. X86 only : Floating point returns to perform the correct save/restore
//    of the return value around return-hijacking.
//
// Based on these cases, the legal set of ReturnKind enumerations are specified
// for each architecture/encoding.
// A value of this enumeration is stored in the GcInfo header.
//
//--------------------------------------------------------------------------------

enum ReturnKind {

    // Cases for Return in one register

    RT_Scalar = 0,
    RT_Object = 1,
    RT_ByRef = 2,

#ifdef TARGET_X86
    RT_Float = 3,       // Encoding 3 means RT_Float on X86
#else
    RT_Unset = 3,       // RT_Unset on other platforms
#endif // TARGET_X86

    // Cases for Struct Return in two registers
    //
    // We have the following equivalencies, because the VM's behavior is the same
    // for both cases:
    // RT_Scalar_Scalar == RT_Scalar
    // RT_Obj_Scalar    == RT_Object
    // RT_ByRef_Scalar  == RT_Byref
    // The encoding for these equivalencies will play out well because
    // RT_Scalar is zero.
    //
    // Naming: RT_firstReg_secondReg
    // Encoding: <Two bits for secondRef> <Two bits for first Reg>
    //
    // This encoding with exclusive bits for each register is chosen for ease of use,
    // and because it doesn't cost any more bits.
    // It can be changed (ex: to a linear sequence) if necessary.
    // For example, we can encode the GC-information for the two registers in 3 bits (instead of 4)
    // if we approximate RT_Obj_ByRef and RT_ByRef_Obj as RT_ByRef_ByRef.

    // RT_Scalar_Scalar = RT_Scalar
    RT_Scalar_Obj   = RT_Object << 2 | RT_Scalar,
    RT_Scalar_ByRef = RT_ByRef << 2  | RT_Scalar,

    // RT_Obj_Scalar   = RT_Object
    RT_Obj_Obj      = RT_Object << 2 | RT_Object,
    RT_Obj_ByRef    = RT_ByRef << 2  | RT_Object,

    // RT_ByRef_Scalar  = RT_Byref
    RT_ByRef_Obj    = RT_Object << 2 | RT_ByRef,
    RT_ByRef_ByRef  = RT_ByRef << 2  | RT_ByRef,

    // Illegal or uninitialized value,
    // Not a valid encoding, never written to image.
    RT_Illegal = 0xFF
};

// Identify ReturnKinds containing useful information
inline bool IsValidReturnKind(ReturnKind returnKind)
{
    return (returnKind != RT_Illegal)
#ifndef TARGET_X86
        && (returnKind != RT_Unset)
#endif // TARGET_X86
        ;
}

// Identify ReturnKinds that can be a part of a multi-reg struct return
inline bool IsValidFieldReturnKind(ReturnKind returnKind)
{
    return (returnKind == RT_Scalar || returnKind == RT_Object || returnKind == RT_ByRef);
}

inline bool IsPointerFieldReturnKind(ReturnKind returnKind)
{
    _ASSERTE(IsValidFieldReturnKind(returnKind));
    return (returnKind == RT_Object || returnKind == RT_ByRef);
}

inline bool IsStructReturnKind(ReturnKind returnKind)
{
    // Two bits encode integer/ref/float return-kinds.
    // Encodings needing more than two bits are (non-scalar) struct-returns.
    return returnKind > 3;
}

inline bool IsScalarReturnKind(ReturnKind returnKind)
{
    return (returnKind == RT_Scalar)
#ifdef TARGET_X86
        || (returnKind == RT_Float)
#endif // TARGET_X86
        ;
}

inline bool IsPointerReturnKind(ReturnKind returnKind)
{
    return IsValidReturnKind(returnKind) && !IsScalarReturnKind(returnKind);
}

// Helpers for combining/extracting individual ReturnKinds from/to Struct ReturnKinds.
// Encoding is two bits per register

inline ReturnKind GetStructReturnKind(ReturnKind reg0, ReturnKind reg1)
{
    _ASSERTE(IsValidFieldReturnKind(reg0) && IsValidFieldReturnKind(reg1));

    ReturnKind structReturnKind = (ReturnKind)(reg1 << 2 | reg0);

    _ASSERTE(IsValidReturnKind(structReturnKind));

    return structReturnKind;
}

// Extract returnKind for the specified return register.
// Also determines if higher ordinal return registers contain object references
inline ReturnKind ExtractRegReturnKind(ReturnKind returnKind, size_t returnRegOrdinal, bool& moreRegs)
{
    _ASSERTE(IsValidReturnKind(returnKind));

    // Return kind of each return register is encoded in two bits at returnRegOrdinal*2 position from LSB
    ReturnKind regReturnKind = (ReturnKind)((returnKind >> (returnRegOrdinal * 2)) & 3);

    // Check if any other higher ordinal return registers have object references.
    // ReturnKind of higher ordinal return registers are encoded at (returnRegOrdinal+1)*2) position from LSB
    // If all of the remaining bits are 0 then there isn't any more RT_Object or RT_ByRef encoded in returnKind.
    moreRegs = (returnKind >> ((returnRegOrdinal+1) * 2)) != 0;

    _ASSERTE(IsValidReturnKind(regReturnKind));
    _ASSERTE((returnRegOrdinal == 0) || IsValidFieldReturnKind(regReturnKind));

    return regReturnKind;
}

inline const char *ReturnKindToString(ReturnKind returnKind)
{
    switch (returnKind) {
    case RT_Scalar: return "Scalar";
    case RT_Object: return "Object";
    case RT_ByRef:  return "ByRef";
#ifdef TARGET_X86
    case RT_Float:  return "Float";
#else
    case RT_Unset:         return "UNSET";
#endif // TARGET_X86
    case RT_Scalar_Obj:    return "{Scalar, Object}";
    case RT_Scalar_ByRef:  return "{Scalar, ByRef}";
    case RT_Obj_Obj:       return "{Object, Object}";
    case RT_Obj_ByRef:     return "{Object, ByRef}";
    case RT_ByRef_Obj:     return "{ByRef, Object}";
    case RT_ByRef_ByRef:   return "{ByRef, ByRef}";

    case RT_Illegal:   return "<Illegal>";
    default: return "!Impossible!";
    }
}

#ifdef TARGET_X86

#include <stdlib.h>     // For memcmp()

#define MAX_PTRARG_OFS 1024

#ifndef FASTCALL
#define FASTCALL __fastcall
#endif

// we use offsetof to get the offset of a field
#include <stddef.h> // offsetof

enum infoHdrAdjustConstants {
    // Constants
    SET_FRAMESIZE_MAX = 7,
    SET_ARGCOUNT_MAX = 8,  // Change to 6
    SET_PROLOGSIZE_MAX = 16,
    SET_EPILOGSIZE_MAX = 10,  // Change to 6
    SET_EPILOGCNT_MAX = 4,
    SET_UNTRACKED_MAX = 3,
    SET_RET_KIND_MAX = 3,   // 2 bits for ReturnKind
    SET_NOGCREGIONS_MAX = 4,
    ADJ_ENCODING_MAX = 0x7f, // Maximum valid encoding in a byte
                             // Also used to mask off next bit from each encoding byte.
    MORE_BYTES_TO_FOLLOW = 0x80 // If the High-bit of a header or adjustment byte
                               // is set, then there are more adjustments to follow.
};

//
// Enum to define codes that are used to incrementally adjust the InfoHdr structure.
// First set of opcodes
enum infoHdrAdjust {

    SET_FRAMESIZE = 0,                                            // 0x00
    SET_ARGCOUNT = SET_FRAMESIZE + SET_FRAMESIZE_MAX + 1,      // 0x08
    SET_PROLOGSIZE = SET_ARGCOUNT + SET_ARGCOUNT_MAX + 1,      // 0x11
    SET_EPILOGSIZE = SET_PROLOGSIZE + SET_PROLOGSIZE_MAX + 1,      // 0x22
    SET_EPILOGCNT = SET_EPILOGSIZE + SET_EPILOGSIZE_MAX + 1,      // 0x2d
    SET_UNTRACKED = SET_EPILOGCNT + (SET_EPILOGCNT_MAX + 1) * 2, // 0x37

    FIRST_FLIP = SET_UNTRACKED + SET_UNTRACKED_MAX + 1,

    FLIP_EDI_SAVED = FIRST_FLIP, // 0x3b
    FLIP_ESI_SAVED,           // 0x3c
    FLIP_EBX_SAVED,           // 0x3d
    FLIP_EBP_SAVED,           // 0x3e
    FLIP_EBP_FRAME,           // 0x3f
    FLIP_INTERRUPTIBLE,       // 0x40
    FLIP_DOUBLE_ALIGN,        // 0x41
    FLIP_SECURITY,            // 0x42
    FLIP_HANDLERS,            // 0x43
    FLIP_LOCALLOC,            // 0x44
    FLIP_EDITnCONTINUE,       // 0x45
    FLIP_VAR_PTR_TABLE_SZ,    // 0x46 Flip whether a table-size exits after the header encoding
    FFFF_UNTRACKED_CNT,       // 0x47 There is a count (>SET_UNTRACKED_MAX) after the header encoding
    FLIP_VARARGS,             // 0x48
    FLIP_PROF_CALLBACKS,      // 0x49
    FLIP_HAS_GS_COOKIE,       // 0x4A - The offset of the GuardStack cookie follows after the header encoding
    FLIP_SYNC,                // 0x4B
    FLIP_HAS_GENERICS_CONTEXT,// 0x4C
    FLIP_GENERICS_CONTEXT_IS_METHODDESC,// 0x4D
    FLIP_REV_PINVOKE_FRAME,   // 0x4E
    NEXT_OPCODE,              // 0x4F -- see next Adjustment enumeration
    NEXT_FOUR_START = 0x50,
    NEXT_FOUR_FRAMESIZE = 0x50,
    NEXT_FOUR_ARGCOUNT = 0x60,
    NEXT_THREE_PROLOGSIZE = 0x70,
    NEXT_THREE_EPILOGSIZE = 0x78
};

// Second set of opcodes, when first code is 0x4F
enum infoHdrAdjust2 {
    SET_RETURNKIND = 0,  // 0x00-SET_RET_KIND_MAX Set ReturnKind to value
    SET_NOGCREGIONS_CNT = SET_RETURNKIND + SET_RET_KIND_MAX + 1,        // 0x04
    FFFF_NOGCREGION_CNT = SET_NOGCREGIONS_CNT + SET_NOGCREGIONS_MAX + 1 // 0x09 There is a count (>SET_NOGCREGIONS_MAX) after the header encoding
};

#define HAS_UNTRACKED               ((unsigned int) -1)
#define HAS_VARPTR                  ((unsigned int) -1)
#define HAS_NOGCREGIONS             ((unsigned int) -1)

// 0 is a valid offset for the Reverse P/Invoke block
// So use -1 as the sentinel for invalid and -2 as the sentinel for present.
#define INVALID_REV_PINVOKE_OFFSET   ((unsigned int) -1)
#define HAS_REV_PINVOKE_FRAME_OFFSET ((unsigned int) -2)
// 0 is not a valid offset for EBP-frames as all locals are at a negative offset
// For ESP frames, the cookie is above (at a higher address than) the buffers,
// and so cannot be at offset 0.
#define INVALID_GS_COOKIE_OFFSET    0
// Temporary value to indicate that the offset needs to be read after the header
#define HAS_GS_COOKIE_OFFSET        ((unsigned int) -1)

// 0 is not a valid sync offset
#define INVALID_SYNC_OFFSET         0
// Temporary value to indicate that the offset needs to be read after the header
#define HAS_SYNC_OFFSET             ((unsigned int) -1)

#define INVALID_ARGTAB_OFFSET       0

#include <pshpack1.h>

// Working set optimization: saving 12 * 128 = 1536 bytes in infoHdrShortcut
struct InfoHdr;

struct InfoHdrSmall {
    unsigned char  prologSize;        // 0
    unsigned char  epilogSize;        // 1
    unsigned char  epilogCount : 3; // 2 [0:2]
    unsigned char  epilogAtEnd : 1; // 2 [3]
    unsigned char  ediSaved : 1; // 2 [4]      which callee-saved regs are pushed onto stack
    unsigned char  esiSaved : 1; // 2 [5]
    unsigned char  ebxSaved : 1; // 2 [6]
    unsigned char  ebpSaved : 1; // 2 [7]
    unsigned char  ebpFrame : 1; // 3 [0]      locals accessed relative to ebp
    unsigned char  interruptible : 1; // 3 [1]      is intr. at all points (except prolog/epilog), not just call-sites
    unsigned char  doubleAlign : 1; // 3 [2]      uses double-aligned stack (ebpFrame will be false)
    unsigned char  security : 1; // 3 [3]      has slot for security object
    unsigned char  handlers : 1; // 3 [4]      has callable handlers
    unsigned char  localloc : 1; // 3 [5]      uses localloc
    unsigned char  editNcontinue : 1; // 3 [6]      was JITed in EnC mode
    unsigned char  varargs : 1; // 3 [7]      function uses varargs calling convention
    unsigned char  profCallbacks : 1; // 4 [0]
    unsigned char  genericsContext : 1;//4 [1]      function reports a generics context parameter is present
    unsigned char  genericsContextIsMethodDesc : 1;//4[2]
    unsigned char  returnKind : 2; // 4 [4]  Available GcInfo v2 onwards, previously undefined
    unsigned short argCount;          // 5,6        in bytes
    unsigned int   frameSize;         // 7,8,9,10   in bytes
    unsigned int   untrackedCnt;      // 11,12,13,14
    unsigned int   varPtrTableSize;   // 15,16,17,18

                                      // Checks whether "this" is compatible with "target".
                                      // It is not an exact bit match as "this" could have some
                                      // marker/place-holder values, which will have to be written out
                                      // after the header.

    bool isHeaderMatch(const InfoHdr& target) const;
};


struct InfoHdr : public InfoHdrSmall {
    // 0 (zero) means that there is no GuardStack cookie
    // The cookie is either at ESP+gsCookieOffset or EBP-gsCookieOffset
    unsigned int   gsCookieOffset;    // 19,20,21,22
    unsigned int   syncStartOffset;   // 23,24,25,26
    unsigned int   syncEndOffset;     // 27,28,29,30
    unsigned int   revPInvokeOffset;  // 31,32,33,34 Available GcInfo v2 onwards, previously undefined
    unsigned int   noGCRegionCnt;     // 35,36,37,38
                                      // 39 bytes total

                                      // Checks whether "this" is compatible with "target".
                                      // It is not an exact bit match as "this" could have some
                                      // marker/place-holder values, which will have to be written out
                                      // after the header.

    bool isHeaderMatch(const InfoHdr& target) const
    {
#ifdef _ASSERTE
        // target cannot have place-holder values.
        _ASSERTE(target.untrackedCnt != HAS_UNTRACKED &&
            target.varPtrTableSize != HAS_VARPTR &&
            target.gsCookieOffset != HAS_GS_COOKIE_OFFSET &&
            target.syncStartOffset != HAS_SYNC_OFFSET &&
            target.revPInvokeOffset != HAS_REV_PINVOKE_FRAME_OFFSET &&
            target.noGCRegionCnt != HAS_NOGCREGIONS);
#endif

        // compare two InfoHdr's up to but not including the untrackCnt field
        if (memcmp(this, &target, offsetof(InfoHdr, untrackedCnt)) != 0)
            return false;

        if (untrackedCnt != target.untrackedCnt) {
            if (target.untrackedCnt <= SET_UNTRACKED_MAX)
                return false;
            else if (untrackedCnt != HAS_UNTRACKED)
                return false;
        }

        if (varPtrTableSize != target.varPtrTableSize) {
            if ((varPtrTableSize != 0) != (target.varPtrTableSize != 0))
                return false;
        }

        if ((gsCookieOffset == INVALID_GS_COOKIE_OFFSET) !=
            (target.gsCookieOffset == INVALID_GS_COOKIE_OFFSET))
            return false;

        if ((syncStartOffset == INVALID_SYNC_OFFSET) !=
            (target.syncStartOffset == INVALID_SYNC_OFFSET))
            return false;

        if ((revPInvokeOffset == INVALID_REV_PINVOKE_OFFSET) !=
            (target.revPInvokeOffset == INVALID_REV_PINVOKE_OFFSET))
            return false;

        if (noGCRegionCnt != target.noGCRegionCnt) {
            if (target.noGCRegionCnt <= SET_NOGCREGIONS_MAX)
                return false;
            else if (noGCRegionCnt != HAS_UNTRACKED)
                return false;
        }

        return true;
    }
};


union CallPattern {
    struct {
        unsigned char argCnt;
        unsigned char regMask;  // EBP=0x8, EBX=0x4, ESI=0x2, EDI=0x1
        unsigned char argMask;
        unsigned char codeDelta;
    }            fld;
    unsigned     val;
};

#include <poppack.h>

#define IH_MAX_PROLOG_SIZE (51)

extern const InfoHdrSmall infoHdrShortcut[];
extern int                infoHdrLookup[];

inline void GetInfoHdr(int index, InfoHdr * header)
{
    *((InfoHdrSmall *)header) = infoHdrShortcut[index];

    header->gsCookieOffset = INVALID_GS_COOKIE_OFFSET;
    header->syncStartOffset = INVALID_SYNC_OFFSET;
    header->syncEndOffset = INVALID_SYNC_OFFSET;
    header->revPInvokeOffset = INVALID_REV_PINVOKE_OFFSET;
    header->noGCRegionCnt = 0;
}

PTR_CBYTE FASTCALL decodeHeader(PTR_CBYTE table, UINT32 version, InfoHdr* header);

BYTE FASTCALL encodeHeaderFirst(const InfoHdr& header, InfoHdr* state, int* more, int *pCached);
BYTE FASTCALL encodeHeaderNext(const InfoHdr& header, InfoHdr* state, BYTE &codeSet);

size_t FASTCALL decodeUnsigned(PTR_CBYTE src, unsigned* value);
size_t FASTCALL decodeUDelta(PTR_CBYTE src, unsigned* value, unsigned lastValue);
size_t FASTCALL decodeSigned(PTR_CBYTE src, int     * value);

#define CP_MAX_CODE_DELTA  (0x23)
#define CP_MAX_ARG_CNT     (0x02)
#define CP_MAX_ARG_MASK    (0x00)

extern const unsigned callPatternTable[];
extern const unsigned callCommonDelta[];


int  FASTCALL lookupCallPattern(unsigned    argCnt,
    unsigned    regMask,
    unsigned    argMask,
    unsigned    codeDelta);

void FASTCALL decodeCallPattern(int         pattern,
    unsigned *  argCnt,
    unsigned *  regMask,
    unsigned *  argMask,
    unsigned *  codeDelta);

#endif // _TARGET_86_

// Stack offsets must be 8-byte aligned, so we use this unaligned
//  offset to represent that the method doesn't have a security object
#define NO_GS_COOKIE              (-1)
#define NO_STACK_BASE_REGISTER    (0xffffffff)
#define NO_SIZE_OF_EDIT_AND_CONTINUE_PRESERVED_AREA (0xffffffff)
#define NO_GENERICS_INST_CONTEXT  (-1)
#define NO_REVERSE_PINVOKE_FRAME  (-1)
#define NO_PSP_SYM                (-1)

#if defined(TARGET_AMD64)

#ifndef TARGET_POINTER_SIZE
#define TARGET_POINTER_SIZE 8    // equal to sizeof(void*) and the managed pointer size in bytes for this target
#endif

#define TargetGcInfoEncoding AMD64GcInfoEncoding

struct AMD64GcInfoEncoding {
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK = (64);

    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK_LOG2 = (6);
    static inline constexpr int32_t NORMALIZE_STACK_SLOT (int32_t x) { return ((x)>>3); }
    static inline constexpr int32_t DENORMALIZE_STACK_SLOT (int32_t x) { return ((x)<<3); }
    static inline constexpr uint32_t NORMALIZE_CODE_LENGTH (uint32_t x) { return (x); }
    static inline constexpr uint32_t DENORMALIZE_CODE_LENGTH (uint32_t x) { return (x); }

    // Encode RBP as 0
    static inline constexpr uint32_t NORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return ((x) ^ 5); }
    static inline constexpr uint32_t DENORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return ((x) ^ 5); }
    static inline constexpr uint32_t NORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return ((x)>>3); }
    static inline constexpr uint32_t DENORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return ((x)<<3); }
    static const bool CODE_OFFSETS_NEED_NORMALIZATION = false;
    static inline constexpr uint32_t NORMALIZE_CODE_OFFSET (uint32_t x) { return (x); }
    static inline constexpr uint32_t DENORMALIZE_CODE_OFFSET (uint32_t x) { return (x); }

    static const int PSP_SYM_STACK_SLOT_ENCBASE = 6;
    static const int GENERICS_INST_CONTEXT_STACK_SLOT_ENCBASE = 6;
    static const int SECURITY_OBJECT_STACK_SLOT_ENCBASE = 6;
    static const int GS_COOKIE_STACK_SLOT_ENCBASE = 6;
    static const int CODE_LENGTH_ENCBASE = 8;
    static const int SIZE_OF_RETURN_KIND_IN_SLIM_HEADER = 2;
    static const int SIZE_OF_RETURN_KIND_IN_FAT_HEADER = 4;
    static const int STACK_BASE_REGISTER_ENCBASE = 3;
    static const int SIZE_OF_STACK_AREA_ENCBASE = 3;
    static const int SIZE_OF_EDIT_AND_CONTINUE_PRESERVED_AREA_ENCBASE = 4;
    static const int REVERSE_PINVOKE_FRAME_ENCBASE = 6;
    static const int NUM_REGISTERS_ENCBASE = 2;
    static const int NUM_STACK_SLOTS_ENCBASE = 2;
    static const int NUM_UNTRACKED_SLOTS_ENCBASE = 1;
    static const int NORM_PROLOG_SIZE_ENCBASE = 5;
    static const int NORM_EPILOG_SIZE_ENCBASE = 3;
    static const int NORM_CODE_OFFSET_DELTA_ENCBASE = 3;
    static const int INTERRUPTIBLE_RANGE_DELTA1_ENCBASE = 6;
    static const int INTERRUPTIBLE_RANGE_DELTA2_ENCBASE = 6;
    static const int REGISTER_ENCBASE = 3;
    static const int REGISTER_DELTA_ENCBASE = 2;
    static const int STACK_SLOT_ENCBASE = 6;
    static const int STACK_SLOT_DELTA_ENCBASE = 4;
    static const int NUM_SAFE_POINTS_ENCBASE = 2;
    static const int NUM_INTERRUPTIBLE_RANGES_ENCBASE = 1;
    static const int NUM_EH_CLAUSES_ENCBASE = 2;
    static const int POINTER_SIZE_ENCBASE = 3;
    static const int LIVESTATE_RLE_RUN_ENCBASE = 2;
    static const int LIVESTATE_RLE_SKIP_ENCBASE = 4;
};

#elif defined(TARGET_ARM)

#ifndef TARGET_POINTER_SIZE
#define TARGET_POINTER_SIZE 4   // equal to sizeof(void*) and the managed pointer size in bytes for this target
#endif

#define TargetGcInfoEncoding ARM32GcInfoEncoding

struct ARM32GcInfoEncoding {
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK = (64);
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK_LOG2 = (6);
    static inline constexpr int32_t NORMALIZE_STACK_SLOT (int32_t x) { return ((x)>>2); }
    static inline constexpr int32_t DENORMALIZE_STACK_SLOT (int32_t x) { return ((x)<<2); }
    static inline constexpr uint32_t NORMALIZE_CODE_LENGTH (uint32_t x) { return ((x)>>1); }
    static inline constexpr uint32_t DENORMALIZE_CODE_LENGTH (uint32_t x) { return ((x)<<1); }
    // Encode R11 as zero
    static inline constexpr uint32_t NORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return ((((x) - 4) & 7) ^ 7); }
    static inline constexpr uint32_t DENORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return (((x) ^ 7) + 4); }
    static inline constexpr uint32_t NORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return ((x)>>2); }
    static inline constexpr uint32_t DENORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return ((x)<<2); }
    static const bool CODE_OFFSETS_NEED_NORMALIZATION = true;
    static inline constexpr uint32_t NORMALIZE_CODE_OFFSET (uint32_t x) { return ((x)>>1)   /* Instructions are 2/4 bytes long in Thumb/ARM states */; }
    static inline constexpr uint32_t DENORMALIZE_CODE_OFFSET (uint32_t x) { return ((x)<<1); }

    // The choices of these encoding bases only affects space overhead
    // and performance, not semantics/correctness.
    static const int PSP_SYM_STACK_SLOT_ENCBASE = 5;
    static const int GENERICS_INST_CONTEXT_STACK_SLOT_ENCBASE = 5;
    static const int SECURITY_OBJECT_STACK_SLOT_ENCBASE = 5;
    static const int GS_COOKIE_STACK_SLOT_ENCBASE = 5;
    static const int CODE_LENGTH_ENCBASE = 7;
    static const int SIZE_OF_RETURN_KIND_IN_SLIM_HEADER = 2;
    static const int SIZE_OF_RETURN_KIND_IN_FAT_HEADER = 2;
    static const int STACK_BASE_REGISTER_ENCBASE = 1;
    static const int SIZE_OF_STACK_AREA_ENCBASE = 3;
    static const int SIZE_OF_EDIT_AND_CONTINUE_PRESERVED_AREA_ENCBASE = 3;
    static const int REVERSE_PINVOKE_FRAME_ENCBASE = 5;
    static const int NUM_REGISTERS_ENCBASE = 2;
    static const int NUM_STACK_SLOTS_ENCBASE = 3;
    static const int NUM_UNTRACKED_SLOTS_ENCBASE = 3;
    static const int NORM_PROLOG_SIZE_ENCBASE = 5;
    static const int NORM_EPILOG_SIZE_ENCBASE = 3;
    static const int NORM_CODE_OFFSET_DELTA_ENCBASE = 3;
    static const int INTERRUPTIBLE_RANGE_DELTA1_ENCBASE = 4;
    static const int INTERRUPTIBLE_RANGE_DELTA2_ENCBASE = 6;
    static const int REGISTER_ENCBASE = 2;
    static const int REGISTER_DELTA_ENCBASE = 1;
    static const int STACK_SLOT_ENCBASE = 6;
    static const int STACK_SLOT_DELTA_ENCBASE = 4;
    static const int NUM_SAFE_POINTS_ENCBASE = 3;
    static const int NUM_INTERRUPTIBLE_RANGES_ENCBASE = 2;
    static const int NUM_EH_CLAUSES_ENCBASE = 3;
    static const int POINTER_SIZE_ENCBASE = 3;
    static const int LIVESTATE_RLE_RUN_ENCBASE = 2;
    static const int LIVESTATE_RLE_SKIP_ENCBASE = 4;
};

#elif defined(TARGET_ARM64)

#ifndef TARGET_POINTER_SIZE
#define TARGET_POINTER_SIZE 8    // equal to sizeof(void*) and the managed pointer size in bytes for this target
#endif

#define TargetGcInfoEncoding ARM64GcInfoEncoding

struct ARM64GcInfoEncoding {
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK = (64);
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK_LOG2 = (6);
    // GC Pointers are 8-bytes aligned
    static inline constexpr int32_t NORMALIZE_STACK_SLOT (int32_t x) { return ((x)>>3); }
    static inline constexpr int32_t DENORMALIZE_STACK_SLOT (int32_t x) { return ((x)<<3); }
    // All Instructions are 4 bytes long
    static inline constexpr uint32_t NORMALIZE_CODE_LENGTH (uint32_t x) { return ((x)>>2); }
    static inline constexpr uint32_t DENORMALIZE_CODE_LENGTH (uint32_t x) { return ((x)<<2); }
    // Encode Frame pointer X29 as zero
    static inline constexpr uint32_t NORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return ((x)^29); }
    static inline constexpr uint32_t DENORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return ((x)^29); }
    static inline constexpr uint32_t NORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return ((x)>>3); }
    static inline constexpr uint32_t DENORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return ((x)<<3); }
    static const bool CODE_OFFSETS_NEED_NORMALIZATION = true;
    // Instructions are 4 bytes long
    static inline constexpr uint32_t NORMALIZE_CODE_OFFSET (uint32_t x) { return ((x)>>2); }
    static inline constexpr uint32_t DENORMALIZE_CODE_OFFSET (uint32_t x) { return ((x)<<2); }

    static const int PSP_SYM_STACK_SLOT_ENCBASE = 6;
    static const int GENERICS_INST_CONTEXT_STACK_SLOT_ENCBASE = 6;
    static const int SECURITY_OBJECT_STACK_SLOT_ENCBASE = 6;
    static const int GS_COOKIE_STACK_SLOT_ENCBASE = 6;
    static const int CODE_LENGTH_ENCBASE = 8;
    static const int SIZE_OF_RETURN_KIND_IN_SLIM_HEADER = 2;
    static const int SIZE_OF_RETURN_KIND_IN_FAT_HEADER = 4;
    // FP encoded as 0, SP as 2.
    static const int STACK_BASE_REGISTER_ENCBASE = 2;
    static const int SIZE_OF_STACK_AREA_ENCBASE = 3;
    static const int SIZE_OF_EDIT_AND_CONTINUE_PRESERVED_AREA_ENCBASE = 4;
    static const int SIZE_OF_EDIT_AND_CONTINUE_FIXED_STACK_FRAME_ENCBASE = 4;
    static const int REVERSE_PINVOKE_FRAME_ENCBASE = 6;
    static const int NUM_REGISTERS_ENCBASE = 3;
    static const int NUM_STACK_SLOTS_ENCBASE = 2;
    static const int NUM_UNTRACKED_SLOTS_ENCBASE = 1;
    static const int NORM_PROLOG_SIZE_ENCBASE = 5;
    static const int NORM_EPILOG_SIZE_ENCBASE = 3;
    static const int NORM_CODE_OFFSET_DELTA_ENCBASE = 3;
    static const int INTERRUPTIBLE_RANGE_DELTA1_ENCBASE = 6;
    static const int INTERRUPTIBLE_RANGE_DELTA2_ENCBASE = 6;
    static const int REGISTER_ENCBASE = 3;
    static const int REGISTER_DELTA_ENCBASE = 2;
    static const int STACK_SLOT_ENCBASE = 6;
    static const int STACK_SLOT_DELTA_ENCBASE = 4;
    static const int NUM_SAFE_POINTS_ENCBASE = 3;
    static const int NUM_INTERRUPTIBLE_RANGES_ENCBASE = 1;
    static const int NUM_EH_CLAUSES_ENCBASE = 2;
    static const int POINTER_SIZE_ENCBASE = 3;
    static const int LIVESTATE_RLE_RUN_ENCBASE = 2;
    static const int LIVESTATE_RLE_SKIP_ENCBASE = 4;
};

#elif defined(TARGET_LOONGARCH64)
#ifndef TARGET_POINTER_SIZE
#define TARGET_POINTER_SIZE 8    // equal to sizeof(void*) and the managed pointer size in bytes for this target
#endif

#define TargetGcInfoEncoding LoongArch64GcInfoEncoding

struct LoongArch64GcInfoEncoding {
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK = (64);
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK_LOG2 = (6);
    // GC Pointers are 8-bytes aligned
    static inline constexpr int32_t NORMALIZE_STACK_SLOT (int32_t x) { return ((x)>>3); }
    static inline constexpr int32_t DENORMALIZE_STACK_SLOT (int32_t x) { return ((x)<<3); }
    // All Instructions are 4 bytes long
    static inline constexpr uint32_t NORMALIZE_CODE_LENGTH (uint32_t x) { return ((x)>>2); }
    static inline constexpr uint32_t DENORMALIZE_CODE_LENGTH (uint32_t x) { return ((x)<<2); }
    // Encode Frame pointer fp=$22 as zero
    static inline constexpr uint32_t NORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return ((x) == 22 ? 0u : 1u); }
    static inline constexpr uint32_t DENORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return ((x) == 0 ? 22u : 3u); }
    static inline constexpr uint32_t NORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return ((x)>>3); }
    static inline constexpr uint32_t DENORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return ((x)<<3); }
    static const bool CODE_OFFSETS_NEED_NORMALIZATION = true;
    // Instructions are 4 bytes long
    static inline constexpr uint32_t NORMALIZE_CODE_OFFSET (uint32_t x) { return ((x)>>2); }
    static inline constexpr uint32_t DENORMALIZE_CODE_OFFSET (uint32_t x) { return ((x)<<2); }

    static const int PSP_SYM_STACK_SLOT_ENCBASE = 6;
    static const int GENERICS_INST_CONTEXT_STACK_SLOT_ENCBASE = 6;
    static const int SECURITY_OBJECT_STACK_SLOT_ENCBASE = 6;
    static const int GS_COOKIE_STACK_SLOT_ENCBASE = 6;
    static const int CODE_LENGTH_ENCBASE = 8;
    static const int SIZE_OF_RETURN_KIND_IN_SLIM_HEADER = 2;
    static const int SIZE_OF_RETURN_KIND_IN_FAT_HEADER = 4;
    // FP/SP encoded as 0 or 1.
    static const int STACK_BASE_REGISTER_ENCBASE = 2;
    static const int SIZE_OF_STACK_AREA_ENCBASE = 3;
    static const int SIZE_OF_EDIT_AND_CONTINUE_PRESERVED_AREA_ENCBASE = 4;
    static const int REVERSE_PINVOKE_FRAME_ENCBASE = 6;
    static const int NUM_REGISTERS_ENCBASE = 3;
    static const int NUM_STACK_SLOTS_ENCBASE = 2;
    static const int NUM_UNTRACKED_SLOTS_ENCBASE = 1;
    static const int NORM_PROLOG_SIZE_ENCBASE = 5;
    static const int NORM_EPILOG_SIZE_ENCBASE = 3;
    static const int NORM_CODE_OFFSET_DELTA_ENCBASE = 3;
    static const int INTERRUPTIBLE_RANGE_DELTA1_ENCBASE = 6;
    static const int INTERRUPTIBLE_RANGE_DELTA2_ENCBASE = 6;
    static const int REGISTER_ENCBASE = 3;
    static const int REGISTER_DELTA_ENCBASE = 2;
    static const int STACK_SLOT_ENCBASE = 6;
    static const int STACK_SLOT_DELTA_ENCBASE = 4;
    static const int NUM_SAFE_POINTS_ENCBASE = 3;
    static const int NUM_INTERRUPTIBLE_RANGES_ENCBASE = 1;
    static const int NUM_EH_CLAUSES_ENCBASE = 2;
    static const int POINTER_SIZE_ENCBASE = 3;
    static const int LIVESTATE_RLE_RUN_ENCBASE = 2;
    static const int LIVESTATE_RLE_SKIP_ENCBASE = 4;
};

#elif defined(TARGET_RISCV64)
#ifndef TARGET_POINTER_SIZE
#define TARGET_POINTER_SIZE 8    // equal to sizeof(void*) and the managed pointer size in bytes for this target
#endif

#define TargetGcInfoEncoding RISCV64GcInfoEncoding

struct RISCV64GcInfoEncoding {
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK = (64);
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK_LOG2 = (6);
    // GC Pointers are 8-bytes aligned
    static inline constexpr int32_t NORMALIZE_STACK_SLOT (int32_t x) { return ((x)>>3); }
    static inline constexpr int32_t DENORMALIZE_STACK_SLOT (int32_t x) { return ((x)<<3); }
    // All Instructions are 4 bytes long
    static inline constexpr uint32_t NORMALIZE_CODE_LENGTH (uint32_t x) { return ((x)>>2); }
    static inline constexpr uint32_t DENORMALIZE_CODE_LENGTH (uint32_t x) { return ((x)<<2); }
    // Encode Frame pointer X8 as zero, sp/x2 as 1
    static inline constexpr uint32_t NORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return ((x) == 8 ? 0u : 1u); }
    static inline constexpr uint32_t DENORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return ((x) == 0 ? 8u : 2u); }
    static inline constexpr uint32_t NORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return ((x)>>3); }
    static inline constexpr uint32_t DENORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return ((x)<<3); }
    static const bool CODE_OFFSETS_NEED_NORMALIZATION = true;
    // Instructions are 4 bytes long
    static inline constexpr uint32_t NORMALIZE_CODE_OFFSET (uint32_t x) { return ((x)>>2); }
    static inline constexpr uint32_t DENORMALIZE_CODE_OFFSET (uint32_t x) { return ((x)<<2); }

    static const int PSP_SYM_STACK_SLOT_ENCBASE = 6;
    static const int GENERICS_INST_CONTEXT_STACK_SLOT_ENCBASE = 6;
    static const int SECURITY_OBJECT_STACK_SLOT_ENCBASE = 6;
    static const int GS_COOKIE_STACK_SLOT_ENCBASE = 6;
    static const int CODE_LENGTH_ENCBASE = 8;
    static const int SIZE_OF_RETURN_KIND_IN_SLIM_HEADER = 2;
    static const int SIZE_OF_RETURN_KIND_IN_FAT_HEADER = 4;
    static const int STACK_BASE_REGISTER_ENCBASE = 2;
    // FP encoded as 0, SP as 1
    static const int SIZE_OF_STACK_AREA_ENCBASE = 3;
    static const int SIZE_OF_EDIT_AND_CONTINUE_PRESERVED_AREA_ENCBASE = 4;
    static const int SIZE_OF_EDIT_AND_CONTINUE_FIXED_STACK_FRAME_ENCBASE = 4;
    static const int REVERSE_PINVOKE_FRAME_ENCBASE = 6;
    static const int NUM_REGISTERS_ENCBASE = 3;
    static const int NUM_STACK_SLOTS_ENCBASE = 2;
    static const int NUM_UNTRACKED_SLOTS_ENCBASE = 1;
    static const int NORM_PROLOG_SIZE_ENCBASE = 5;
    static const int NORM_EPILOG_SIZE_ENCBASE = 3;
    static const int NORM_CODE_OFFSET_DELTA_ENCBASE = 3;
    static const int INTERRUPTIBLE_RANGE_DELTA1_ENCBASE = 6;
    static const int INTERRUPTIBLE_RANGE_DELTA2_ENCBASE = 6;
    static const int REGISTER_ENCBASE = 3;
    static const int REGISTER_DELTA_ENCBASE = 2;
    static const int STACK_SLOT_ENCBASE = 6;
    static const int STACK_SLOT_DELTA_ENCBASE = 4;
    static const int NUM_SAFE_POINTS_ENCBASE = 3;
    static const int NUM_INTERRUPTIBLE_RANGES_ENCBASE = 1;
    static const int NUM_EH_CLAUSES_ENCBASE = 2;
    static const int POINTER_SIZE_ENCBASE = 3;
    static const int LIVESTATE_RLE_RUN_ENCBASE = 2;
    static const int LIVESTATE_RLE_SKIP_ENCBASE = 4;
};

#else // defined(TARGET_xxx)

#ifndef TARGET_X86
#ifdef PORTABILITY_WARNING
PORTABILITY_WARNING("Please specialize these definitions for your platform!")
#endif
#endif

#ifndef TARGET_POINTER_SIZE
#define TARGET_POINTER_SIZE 4   // equal to sizeof(void*) and the managed pointer size in bytes for this target
#endif

#define TargetGcInfoEncoding X86GcInfoEncoding

struct X86GcInfoEncoding {
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK = (64);
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK_LOG2 = (6);
    static inline constexpr int32_t NORMALIZE_STACK_SLOT (int32_t x) { return (x); }
    static inline constexpr int32_t DENORMALIZE_STACK_SLOT (int32_t x) { return (x); }
    static inline constexpr uint32_t NORMALIZE_CODE_LENGTH (uint32_t x) { return (x); }
    static inline constexpr uint32_t DENORMALIZE_CODE_LENGTH (uint32_t x) { return (x); }
    static inline constexpr uint32_t NORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return (x); }
    static inline constexpr uint32_t DENORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return (x); }
    static inline constexpr uint32_t NORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return (x); }
    static inline constexpr uint32_t DENORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return (x); }
    static const bool CODE_OFFSETS_NEED_NORMALIZATION = false;
    static inline constexpr uint32_t NORMALIZE_CODE_OFFSET (uint32_t x) { return (x); }
    static inline constexpr uint32_t DENORMALIZE_CODE_OFFSET (uint32_t x) { return (x); }

    static const int PSP_SYM_STACK_SLOT_ENCBASE = 6;
    static const int GENERICS_INST_CONTEXT_STACK_SLOT_ENCBASE = 6;
    static const int SECURITY_OBJECT_STACK_SLOT_ENCBASE = 6;
    static const int GS_COOKIE_STACK_SLOT_ENCBASE = 6;
    static const int CODE_LENGTH_ENCBASE = 6;
    static const int SIZE_OF_RETURN_KIND_IN_SLIM_HEADER = 2;
    static const int SIZE_OF_RETURN_KIND_IN_FAT_HEADER = 2;
    static const int STACK_BASE_REGISTER_ENCBASE = 3;
    static const int SIZE_OF_STACK_AREA_ENCBASE = 6;
    static const int SIZE_OF_EDIT_AND_CONTINUE_PRESERVED_AREA_ENCBASE = 3;
    static const int REVERSE_PINVOKE_FRAME_ENCBASE = 6;
    static const int NUM_REGISTERS_ENCBASE = 3;
    static const int NUM_STACK_SLOTS_ENCBASE = 5;
    static const int NUM_UNTRACKED_SLOTS_ENCBASE = 5;
    static const int NORM_PROLOG_SIZE_ENCBASE = 4;
    static const int NORM_EPILOG_SIZE_ENCBASE = 3;
    static const int NORM_CODE_OFFSET_DELTA_ENCBASE = 3;
    static const int INTERRUPTIBLE_RANGE_DELTA1_ENCBASE = 5;
    static const int INTERRUPTIBLE_RANGE_DELTA2_ENCBASE = 5;
    static const int REGISTER_ENCBASE = 3;
    static const int REGISTER_DELTA_ENCBASE = REGISTER_ENCBASE;
    static const int STACK_SLOT_ENCBASE = 6;
    static const int STACK_SLOT_DELTA_ENCBASE = 4;
    static const int NUM_SAFE_POINTS_ENCBASE = 4;
    static const int NUM_INTERRUPTIBLE_RANGES_ENCBASE = 1;
    static const int NUM_EH_CLAUSES_ENCBASE = 2;
    static const int POINTER_SIZE_ENCBASE = 3;
    static const int LIVESTATE_RLE_RUN_ENCBASE = 2;
    static const int LIVESTATE_RLE_SKIP_ENCBASE = 4;
};

#endif // defined(TARGET_xxx)

#ifdef FEATURE_INTERPRETER

struct InterpreterGcInfoEncoding {
    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK = (64);

    static const uint32_t NUM_NORM_CODE_OFFSETS_PER_CHUNK_LOG2 = (6);
    // Interpreter-FIXME: Interpreter has fixed-size stack slots so we could normalize them based on that.
    static inline constexpr int32_t NORMALIZE_STACK_SLOT (int32_t x) { return (x); }
    static inline constexpr int32_t DENORMALIZE_STACK_SLOT (int32_t x) { return (x); }
    // Interpreter-FIXME: Interpreter has fixed-size opcodes so code length is a multiple of that.
    static inline constexpr uint32_t NORMALIZE_CODE_LENGTH (uint32_t x) { return (x); }
    static inline constexpr uint32_t DENORMALIZE_CODE_LENGTH (uint32_t x) { return (x); }

    static inline constexpr uint32_t NORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return (x); }
    static inline constexpr uint32_t DENORMALIZE_STACK_BASE_REGISTER (uint32_t x) { return (x); }
    // Interpreter-FIXME: Interpreter has fixed-size stack slots so we could normalize them based on that.
    static inline constexpr uint32_t NORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return (x); }
    static inline constexpr uint32_t DENORMALIZE_SIZE_OF_STACK_AREA (uint32_t x) { return (x); }
    static const bool CODE_OFFSETS_NEED_NORMALIZATION = false;
    // Interpreter-FIXME: Interpreter has fixed-size opcodes so code length is a multiple of that.
    static inline constexpr uint32_t NORMALIZE_CODE_OFFSET (uint32_t x) { return (x); }
    static inline constexpr uint32_t DENORMALIZE_CODE_OFFSET (uint32_t x) { return (x); }

    static const int PSP_SYM_STACK_SLOT_ENCBASE = 6;
    static const int GENERICS_INST_CONTEXT_STACK_SLOT_ENCBASE = 6;
    static const int SECURITY_OBJECT_STACK_SLOT_ENCBASE = 6;
    static const int GS_COOKIE_STACK_SLOT_ENCBASE = 6;
    static const int CODE_LENGTH_ENCBASE = 8;
    static const int STACK_BASE_REGISTER_ENCBASE = 3;
    static const int SIZE_OF_STACK_AREA_ENCBASE = 3;
    static const int SIZE_OF_EDIT_AND_CONTINUE_PRESERVED_AREA_ENCBASE = 4;
    // Interpreter-FIXME: This constant is only used on certain architectures.
    static const int SIZE_OF_EDIT_AND_CONTINUE_FIXED_STACK_FRAME_ENCBASE = 4;
    static const int REVERSE_PINVOKE_FRAME_ENCBASE = 6;
    static const int NUM_REGISTERS_ENCBASE = 2;
    static const int NUM_STACK_SLOTS_ENCBASE = 2;
    static const int NUM_UNTRACKED_SLOTS_ENCBASE = 1;
    static const int NORM_PROLOG_SIZE_ENCBASE = 5;
    static const int NORM_EPILOG_SIZE_ENCBASE = 3;
    static const int NORM_CODE_OFFSET_DELTA_ENCBASE = 3;
    static const int INTERRUPTIBLE_RANGE_DELTA1_ENCBASE = 6;
    static const int INTERRUPTIBLE_RANGE_DELTA2_ENCBASE = 6;
    static const int REGISTER_ENCBASE = 3;
    static const int REGISTER_DELTA_ENCBASE = 2;
    static const int STACK_SLOT_ENCBASE = 6;
    static const int STACK_SLOT_DELTA_ENCBASE = 4;
    static const int NUM_SAFE_POINTS_ENCBASE = 2;
    static const int NUM_INTERRUPTIBLE_RANGES_ENCBASE = 1;
    static const int NUM_EH_CLAUSES_ENCBASE = 2;
    static const int POINTER_SIZE_ENCBASE = 3;
    static const int LIVESTATE_RLE_RUN_ENCBASE = 2;
    static const int LIVESTATE_RLE_SKIP_ENCBASE = 4;
};

#endif // FEATURE_INTERPRETER

#ifdef debug_instrumented_return
#define return debug_instrumented_return
#endif // debug_instrumented_return

#endif // !__GCINFOTYPES_H__
