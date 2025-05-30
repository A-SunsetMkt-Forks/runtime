// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// --------------------------------------------------------------------------------
// PEImage.h
//

// --------------------------------------------------------------------------------


#ifndef PEIMAGE_H_
#define PEIMAGE_H_

// --------------------------------------------------------------------------------
// Required headers
// --------------------------------------------------------------------------------

#include "clrtypes.h"
#include "peimagelayout.h"
#include "sstring.h"
#include "holder.h"
#include <assemblyprobeextension.h>
#include "cdacdata.h"

class SimpleRWLock;
// --------------------------------------------------------------------------------
// Forward declarations
// --------------------------------------------------------------------------------

class Crst;

// --------------------------------------------------------------------------------
// PEImage is a PE file loaded into memory.
//
// The actual data is represented by PEImageLayout instances which are created on demand.
//
// Various PEImageLayouts can be classified into two kinds -
//  - Flat    - the same layout as on disk/array or
//
//  - Loaded  - PE sections are mapped into virtual addresses.
//              PE relocations are applied.
//              Native exception handlers are registered with OS (on Windows).
//
// Flat layouts are sufficient for operations that do not require running native code,
// Anything based on RVA, such as retrieving IL method bodies, is slightly less efficient,
// since RVA must be translated to file offsets by iterating through section headers.
// The additional cost is not very high though, since our PEs have only a few sections.
//
// Loaded layouts are functional supersets of Flat - anything that can be done with Flat
// can be done with Loaded.
//
// Running native code in the PE (i.e. R2R or IJW scenarios) requires Loaded layout.
// It is possible to execute R2R assembly from Flat layout in IL mode, but its R2R functionality
// will be disabled. When R2R is explicitly turned off, Flat is sufficient for any scenario with
// R2R assemblies.
// In a case of IJW, the PE must be loaded by the native loader to ensure that native dependencies
// are resolved.
//
// In some scenarios we create Loaded layouts by manually mapping images into memory.
// That is particularly true on Unix where we cannot rely on OS loader.
// Manual creation of layouts is limited to "IL only" images. This can be checked
// for via `PEDecoder::IsILOnlyImage`
// NOTE: historically, and somewhat confusingly, R2R PEs are considered IsILOnlyImage for this
//       purpose. That is true even for composite R2R PEs that do not contain IL.
//
// A PEImage, depending on scenario, may end up creating both Flat and Loaded layouts,
// thus it has two slots - m_pLayouts[IMAGE_COUNT].
//
// m_pLayouts[IMAGE_FLAT]
//   When initialized contains a layout that allows operations for which Flat layout is sufficient -
//   i.e. reading metadata
//
// m_pLayouts[IMAGE_LOADED]
//   When initialized contains a layout that allows loading/running code.
//
// The layouts can only be unloaded together with the owning PEImage, so if we have Flat and
// then need Loaded, we can only add one more. Thus we have two slots.
//
// Often the slots refer to the same layout though. That is because if we create Loaded before Flat,
// we put Loaded into both slots, since it is functionally a superset of Flat.
// Also for pure-IL assemblies Flat is sufficient for anything, so we may put Flat into both slots.
//

#define CV_SIGNATURE_RSDS   0x53445352

// CodeView RSDS debug information -> PDB 7.00
struct CV_INFO_PDB70
{
    DWORD      magic;
    GUID       signature;       // unique identifier
    DWORD      age;             // an always-incrementing value
    char       path[MAX_LONGPATH];  // zero terminated string with the name of the PDB file
};

typedef DPTR(class PEImage)                PTR_PEImage;

class PEImage final
{

public:
    // ------------------------------------------------------------
    // Public API
    // ------------------------------------------------------------

    // initialize static data (i.e. locks, unique instance cache, etc..)
    static void Startup();

    ~PEImage();
    explicit PEImage(const WCHAR* path);

    BOOL Equals(PEImage* pImage);

    ULONG AddRef();
    ULONG Release();

#ifndef DACCESS_COMPILE
    static PTR_PEImage CreateFromByteArray(const BYTE* array, COUNT_T size);
#ifndef TARGET_UNIX
    static PTR_PEImage CreateFromHMODULE(HMODULE hMod);
#endif // !TARGET_UNIX
    static PTR_PEImage OpenImage(
        LPCWSTR pPath,
        MDInternalImportFlags flags = MDInternalImport_Default,
        ProbeExtensionResult probeExtensionResult = ProbeExtensionResult::Invalid());

    static PTR_PEImage FindByPath(LPCWSTR pPath, BOOL isInBundle, BOOL isExternalData);
    void AddToHashMap();
#endif

    BOOL IsOpened();
    PTR_PEImageLayout GetOrCreateLayout(DWORD imageLayoutMask);

    BOOL HasLoadedLayout();
    PTR_PEImageLayout GetLoadedLayout();
    PTR_PEImageLayout GetFlatLayout();

    const SString& GetPath();
    const SString& GetPathToLoad();
    LPCWSTR GetPathForErrorMessages() { return GetPath(); }

    BOOL IsFile();
    BOOL IsInBundle() const;
    BOOL IsExternalData() const;
    void* GetExternalData(INT64* size);
    INT64 GetOffset() const;
    INT64 GetSize() const;
    BOOL IsCompressed(INT64* uncompressedSize = NULL) const;

#ifndef DACCESS_COMPILE
    HANDLE GetFileHandle();
    HRESULT TryOpenFile(bool takeLock = false);
#endif

    void GetMVID(GUID *pMvid);
    IMDInternalImport* GetMDImport();
    BOOL MDImportLoaded();

    BOOL HasContents() ;
    BOOL IsPtrInImage(PTR_CVOID data);

    BOOL HasNTHeaders();
    BOOL HasCorHeader();
    BOOL HasReadyToRunHeader();
    BOOL HasDirectoryEntry(int entry);
    BOOL Has32BitNTHeaders();

    void GetPEKindAndMachine(DWORD* pdwKind, DWORD* pdwMachine);

    BOOL IsILOnly();
    BOOL IsReferenceAssembly();
    BOOL IsComponentAssembly();

    PTR_CVOID GetNativeManifestMetadata(COUNT_T* pSize = NULL);
    mdToken GetEntryPointToken();
    DWORD GetCorHeaderFlags();

    PTR_CVOID GetMetadata(COUNT_T* pSize = NULL);

    // Check utilities
    static CHECK CheckStartup();
    static CHECK CheckCanonicalFullPath(const SString& path);

    CHECK CheckFormat();
    CHECK CheckILFormat();
    CHECK CheckUniqueInstance();

    void SetModuleFileNameHintForDAC();
#ifdef DACCESS_COMPILE
    void EnumMemoryRegions(CLRDataEnumMemoryFlags flags);
#endif
    const SString &GetModuleFileNameHintForDAC();

private:
#ifndef DACCESS_COMPILE
    // Get or create the layout corresponding to the mask, with an AddRef
    PTR_PEImageLayout GetOrCreateLayoutInternal(DWORD imageLayoutMask);

    // Create the mapped layout
    PTR_PEImageLayout CreateLoadedLayout(bool throwOnFailure);

    // Create the flat layout
    PTR_PEImageLayout CreateFlatLayout();

    void   SetLayout(DWORD dwLayout, PTR_PEImageLayout pLayout);
#endif
    // Get an existing layout corresponding to the mask, no AddRef
    PTR_PEImageLayout GetExistingLayoutInternal(DWORD imageLayoutMask);

    void OpenMDImport();
    // ------------------------------------------------------------
    // Private routines
    // ------------------------------------------------------------

    void Init(ProbeExtensionResult probeExtensionResult);

    struct PEImageLocator
    {
        LPCWSTR m_pPath;
        BOOL m_bIsInBundle;
        BOOL m_bIsExternalData;

        PEImageLocator(LPCWSTR pPath, BOOL bIsInBundle, BOOL bIsExternalData)
            : m_pPath(pPath)
            , m_bIsInBundle(bIsInBundle)
            , m_bIsExternalData(bIsExternalData)
        {
        }

        PEImageLocator(PEImage * pImage)
            : m_pPath(pImage->m_path.GetUnicode())
            , m_bIsInBundle(pImage->IsInBundle())
            , m_bIsExternalData(pImage->IsExternalData())
        {
        }
    };

    static BOOL CompareImage(UPTR image1, UPTR image2);
    static BOOL CompareIJWDataBase(UPTR base, UPTR mapping);

public:
    class IJWFixupData
    {
    private:
        Crst            m_lock;
        void*           m_base;
        DWORD           m_flags;

        // the fixup for the next iteration in FixupVTables
        // we use it to make sure that we do not try to fix up the same entry twice
        // if there was a pass that was aborted in the middle
        COUNT_T         m_iNextFixup;
        COUNT_T         m_iNextMethod;

        enum {
            e_FIXED_UP = 0x1
        };

    public:
        IJWFixupData(void* pBase);
        void* GetBase() { LIMITED_METHOD_CONTRACT; return m_base; }
        Crst* GetLock() { LIMITED_METHOD_CONTRACT; return &m_lock; }
        BOOL IsFixedUp() { LIMITED_METHOD_CONTRACT; return m_flags & e_FIXED_UP; }
        void SetIsFixedUp() { LIMITED_METHOD_CONTRACT; m_flags |= e_FIXED_UP; }
        void MarkMethodFixedUp(COUNT_T iFixup, COUNT_T iMethod);
        BOOL IsMethodFixedUp(COUNT_T iFixup, COUNT_T iMethod);
    };

    static IJWFixupData* GetIJWData(void* pBase);

private:

    // ------------------------------------------------------------
    // Static fields
    // ------------------------------------------------------------

    static CrstStatic   s_hashLock;
    static PtrHashMap* s_Images;

//@TODO:workaround: Remove this when we have one PEImage per mapped image,
//@TODO:workaround: and move the lock there
// This is for IJW thunk initialization, as it is no longer guaranteed
// that the initialization will occur under the loader lock.
    static CrstStatic   s_ijwHashLock;
    static PtrHashMap* s_ijwFixupDataHash;

    // ------------------------------------------------------------
    // Instance fields
    // ------------------------------------------------------------

    const SString   m_path;
    ULONG     m_pathHash;
    LONG      m_refCount;

    // means this is a unique (deduped) instance.
    BOOL      m_bInHashMap;

    // Valid if this image is from a probe extension (single-file bundle, external data).
    // If m_probeExtensionResult is valid, it takes precedence over m_path for loading.
    ProbeExtensionResult m_probeExtensionResult;

    // valid handle if we tried to open the file/path and succeeded.
    HANDLE m_hFile;

    DWORD m_dwPEKind;
    DWORD m_dwMachine;

    // This only used by DAC
    // For assemblies loaded from a path or single-file bundle, this is the file name portion of the path
    // For assemblies loaded from memory, this is the module file name from metadata
    SString   m_sModuleFileNameHintUsedByDac;

    enum
    {
        IMAGE_FLAT=0,
        IMAGE_LOADED=1,
        IMAGE_COUNT=2
    };

    SimpleRWLock *m_pLayoutLock;
    PTR_PEImageLayout m_pLayouts[IMAGE_COUNT];
    IMDInternalImport* m_pMDImport;

    friend struct cdac_data<PEImage>;
};

template<>
struct cdac_data<PEImage>
{
    // The loaded PEImageLayout is m_pLayouts[IMAGE_LOADED]
    static constexpr size_t LoadedImageLayout = offsetof(PEImage, m_pLayouts) + sizeof(PTR_PEImageLayout);
    static constexpr size_t ProbeExtensionResult = offsetof(PEImage, m_probeExtensionResult);
};

FORCEINLINE void PEImageRelease(PEImage *i)
{
    WRAPPER_NO_CONTRACT;
    i->Release();
}

typedef Wrapper<PEImage *, DoNothing, PEImageRelease> PEImageHolder;

// ================================================================================
// Inline definitions
// ================================================================================

#include "peimage.inl"

#endif  // PEIMAGE_H_
