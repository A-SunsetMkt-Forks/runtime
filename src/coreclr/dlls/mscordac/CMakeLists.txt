add_definitions(-DFEATURE_NO_HOST)

set(CLR_DAC_SOURCES
)

add_definitions(-DFX_VER_INTERNALNAME_STR=mscordaccore.dll)

if(CLR_CMAKE_HOST_WIN32)
    list(APPEND CLR_DAC_SOURCES
        Native.rc
    )
    set(DEF_SOURCES
        mscordac.src
    )

    set(CURRENT_BINARY_DIR_FOR_CONFIG ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR})

    # Preprocess exports definition file
    preprocess_file(${CMAKE_CURRENT_SOURCE_DIR}/${DEF_SOURCES} ${CURRENT_BINARY_DIR_FOR_CONFIG}/mscordac.def)

    # Create target to add file dependency on mscordac.def
    add_custom_target(mscordaccore_def DEPENDS ${CURRENT_BINARY_DIR_FOR_CONFIG}/mscordac.def)

    # No library groups for Win32
    set(START_LIBRARY_GROUP)
    set(END_LIBRARY_GROUP)
else(CLR_CMAKE_HOST_WIN32)
    set(DEF_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/mscordac_unixexports.src)
    set(EXPORTS_FILE ${CMAKE_CURRENT_BINARY_DIR}/mscordac.exports)

    # Add dependency on export file
    add_custom_target(mscordaccore_exports DEPENDS ${EXPORTS_FILE})

    if(CLR_CMAKE_HOST_APPLE OR CLR_CMAKE_HOST_FREEBSD OR CLR_CMAKE_HOST_NETBSD OR CLR_CMAKE_HOST_SUNOS OR CLR_CMAKE_HOST_HAIKU)
        generate_exports_file(${DEF_SOURCES} ${EXPORTS_FILE})
    endif(CLR_CMAKE_HOST_APPLE OR CLR_CMAKE_HOST_FREEBSD OR CLR_CMAKE_HOST_NETBSD OR CLR_CMAKE_HOST_SUNOS OR CLR_CMAKE_HOST_HAIKU)

    if(CORECLR_SET_RPATH AND CLR_CMAKE_HOST_OSX AND CLR_CMAKE_HOST_ARCH_ARM64)
      set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
      set(CMAKE_INSTALL_NAME_DIR "@rpath")
    endif(CORECLR_SET_RPATH AND CLR_CMAKE_HOST_OSX AND CLR_CMAKE_HOST_ARCH_ARM64)

    if(CLR_CMAKE_HOST_LINUX)

        # Generate DAC export file with the DAC_ prefix
        generate_exports_file_prefix(${DEF_SOURCES} ${EXPORTS_FILE} DAC_)

        if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            set(REDEFINES_FILE_SCRIPT ${CMAKE_SOURCE_DIR}/generateredefinesfile.ps1)
        else()
            set(REDEFINES_FILE_SCRIPT ${CMAKE_SOURCE_DIR}/generateredefinesfile.sh)
        endif()

        if (CLR_CMAKE_HOST_ARCH_ARM OR CLR_CMAKE_HOST_ARCH_ARM64 OR CLR_CMAKE_HOST_ARCH_LOONGARCH64)
            set(JUMP_INSTRUCTION b)
        elseif (CLR_CMAKE_HOST_ARCH_RISCV64)
            set(JUMP_INSTRUCTION tail)
        else()
            set(JUMP_INSTRUCTION jmp)
        endif()

        # Generate the palredefines.inc file to map from the imported prefixed APIs (foo to DAC_foo)
        set(PAL_REDEFINES_INC ${GENERATED_INCLUDE_DIR}/palredefines.inc)

        # Win32 may be false when cross compiling
        if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            add_custom_command(
                OUTPUT ${PAL_REDEFINES_INC}
                COMMAND powershell -NoProfile -ExecutionPolicy ByPass -File \"${REDEFINES_FILE_SCRIPT}\" -filename \"${DEF_SOURCES}\" -jump ${JUMP_INSTRUCTION} -prefix1 \"\" -prefix2 \"DAC_\" > ${PAL_REDEFINES_INC}
                DEPENDS ${DEF_SOURCES} ${REDEFINES_FILE_SCRIPT}
                COMMENT "Generating PAL redefines file -> ${PAL_REDEFINES_INC}"
            )
        else()
            add_custom_command(
                OUTPUT ${PAL_REDEFINES_INC}
                COMMAND ${REDEFINES_FILE_SCRIPT} ${DEF_SOURCES} ${JUMP_INSTRUCTION} "" DAC_ > ${PAL_REDEFINES_INC}
                DEPENDS ${DEF_SOURCES} ${REDEFINES_FILE_SCRIPT}
                COMMENT "Generating PAL redefines file -> ${PAL_REDEFINES_INC}"
                VERBATIM
            )
        endif()
        add_custom_target(pal_redefines_file DEPENDS ${PAL_REDEFINES_INC})

        # Generate the libredefines.inc file for the DAC to export the prefixed APIs (DAC_foo to foo)
        set(LIB_REDEFINES_INC ${GENERATED_INCLUDE_DIR}/libredefines.inc)

        # Win32 may be false when cross compiling
        if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            add_custom_command(
                OUTPUT ${LIB_REDEFINES_INC}
                COMMAND powershell -NoProfile -ExecutionPolicy ByPass -File \"${REDEFINES_FILE_SCRIPT}\" -filename \"${DEF_SOURCES}\" -jump ${JUMP_INSTRUCTION} -prefix1 \"DAC_\" > ${LIB_REDEFINES_INC}
                DEPENDS ${DEF_SOURCES} ${REDEFINES_FILE_SCRIPT}
                COMMENT "Generating DAC export redefines file -> ${LIB_REDEFINES_INC}"
            )
        else()
            add_custom_command(
                OUTPUT ${LIB_REDEFINES_INC}
                COMMAND ${REDEFINES_FILE_SCRIPT} ${DEF_SOURCES} ${JUMP_INSTRUCTION} DAC_ > ${LIB_REDEFINES_INC}
                DEPENDS ${DEF_SOURCES} ${REDEFINES_FILE_SCRIPT}
                COMMENT "Generating DAC export redefines file -> ${LIB_REDEFINES_INC}"
                VERBATIM
            )
        endif()
        add_custom_target(lib_redefines_inc DEPENDS ${LIB_REDEFINES_INC})

        # Add lib redefines file to DAC
        list(APPEND CLR_DAC_SOURCES libredefines.S)
    endif(CLR_CMAKE_HOST_LINUX)

    if(CLR_CMAKE_HOST_LINUX OR CLR_CMAKE_HOST_FREEBSD OR CLR_CMAKE_HOST_NETBSD OR CLR_CMAKE_HOST_SUNOS OR CLR_CMAKE_HOST_HAIKU)
        # This option is necessary to ensure that the overloaded delete operator defined inside
        # of the utilcode will be used instead of the standard library delete operator.
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Xlinker -Bsymbolic")

        # The following linked options can be inserted into the linker libraries list to
        # ensure proper resolving of circular references between a subset of the libraries.
        set(START_LIBRARY_GROUP -Wl,--start-group)
        set(END_LIBRARY_GROUP -Wl,--end-group)
    endif(CLR_CMAKE_HOST_LINUX OR CLR_CMAKE_HOST_FREEBSD OR CLR_CMAKE_HOST_NETBSD OR CLR_CMAKE_HOST_SUNOS OR CLR_CMAKE_HOST_HAIKU)

    set_exports_linker_option(${EXPORTS_FILE})
endif(CLR_CMAKE_HOST_WIN32)

# Create object library to enable creation of proper dependency of mscordaccore.exp on mscordac.obj and
# mscordaccore on both the mscordaccore.exp and mscordac.obj.
add_library_clr(mscordacobj OBJECT mscordac.cpp)

add_library_clr(mscordaccore SHARED ${CLR_DAC_SOURCES} $<TARGET_OBJECTS:mscordacobj>)

set_target_properties(mscordacobj mscordaccore PROPERTIES DAC_COMPONENT TRUE)

if(CLR_CMAKE_HOST_LINUX)
    add_dependencies(mscordaccore lib_redefines_inc)
endif(CLR_CMAKE_HOST_LINUX)

if(CLR_CMAKE_HOST_UNIX)
    add_dependencies(mscordaccore mscordaccore_exports)
    set_property(TARGET mscordaccore APPEND_STRING PROPERTY LINK_FLAGS ${EXPORTS_LINKER_OPTION})
    set_property(TARGET mscordaccore APPEND_STRING PROPERTY LINK_DEPENDS ${EXPORTS_FILE})
endif(CLR_CMAKE_HOST_UNIX)

# IMPORTANT! Please do not rearrange the order of the libraries. The linker on Linux is
# order dependent and changing the order can result in undefined symbols in the shared
# library.
set(COREDAC_LIBRARIES
    ${START_LIBRARY_GROUP} # Start group of libraries that have circular references
    cee_dac
    cordbee_dac
    corguids
    daccess
    dbgutil
    mdcompiler_dac
    mdruntime_dac
    mdruntimerw_dac
    utilcode_dac
    unwinder_dac
    ${END_LIBRARY_GROUP} # End group of libraries that have circular references
)

if(CLR_CMAKE_HOST_WIN32)
    # mscordac.def should be generated before mscordaccore.dll is built
    add_dependencies(mscordaccore mscordaccore_def)

    # Generate export file
    add_custom_command(
        DEPENDS mscordaccore_def "${CURRENT_BINARY_DIR_FOR_CONFIG}/mscordac.def" mscordacobj daccess
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/mscordaccore.exp
        COMMAND lib.exe /NOLOGO /OUT:"${CMAKE_CURRENT_BINARY_DIR}/mscordaccore.lib" /DEF:"${CURRENT_BINARY_DIR_FOR_CONFIG}/mscordac.def" "$<TARGET_FILE:daccess>" $<$<CONFIG:Release,Relwithdebifo>:/LTCG> ${STATIC_LIBRARY_FLAGS} $<TARGET_OBJECTS:mscordacobj>
        COMMENT "Generating mscordaccore.exp export file"
    )

    set_source_files_properties(
        ${CMAKE_CURRENT_BINARY_DIR}/mscordaccore.exp
        PROPERTIES GENERATED TRUE
    )

    add_custom_target(mscordaccore_exp DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/mscordaccore.exp)
    add_dependencies(mscordaccore_exp mscordacobj)
    add_dependencies(mscordaccore mscordaccore_exp mscordacobj)

    set(COREDAC_LIBRARIES
        ${CMAKE_CURRENT_BINARY_DIR}/mscordaccore.exp # export file
        ${COREDAC_LIBRARIES}
        kernel32.lib
        advapi32.lib
        ole32.lib
        oleaut32.lib
        uuid.lib
        user32.lib
        coreclrminipal
        ${STATIC_MT_CRT_LIB}
        ${STATIC_MT_VCRT_LIB}
    )
else(CLR_CMAKE_HOST_WIN32)
    list(APPEND COREDAC_LIBRARIES
        mscorrc
        coreclrpal
        coreclrminipal
    )
endif(CLR_CMAKE_HOST_WIN32)

if(CLR_CMAKE_HOST_WIN32 AND CLR_CMAKE_TARGET_UNIX)
    list(APPEND COREDAC_LIBRARIES
        libunwind_xdac
    )
endif(CLR_CMAKE_HOST_WIN32 AND CLR_CMAKE_TARGET_UNIX)

if(CLR_CMAKE_HOST_UNIX)
    list(APPEND COREDAC_LIBRARIES
        coreclrpal_dac
    )
endif(CLR_CMAKE_HOST_UNIX)

target_link_libraries(mscordaccore PRIVATE ${COREDAC_LIBRARIES})

esrp_sign(mscordaccore)

# add the install targets
install_clr(TARGETS mscordaccore DESTINATIONS . sharedFramework COMPONENT debug)

if(CLR_CMAKE_HOST_WIN32)
    set(LONG_NAME_HOST_ARCH ${CLR_CMAKE_HOST_ARCH})
    set(LONG_NAME_TARGET_ARCH ${CLR_CMAKE_TARGET_ARCH})
    if (LONG_NAME_HOST_ARCH STREQUAL x64)
        set(LONG_NAME_HOST_ARCH "amd64")
    endif()
    if (LONG_NAME_TARGET_ARCH STREQUAL x64)
        set(LONG_NAME_TARGET_ARCH "amd64")
    endif()
    message ("Read file version from native version header at '${VERSION_HEADER_PATH}'.")
    file(READ "${VERSION_HEADER_PATH}" NATIVE_VERSION_HEADER)
    string(REGEX MATCH "#define VER_FILEVERSION[ \t]+[0-9]+(,[0-9]+)+" FILE_VERSION_LINE "${NATIVE_VERSION_HEADER}")
    string(REGEX MATCHALL "[0-9]+" FILE_VERSION_COMPONENTS "${FILE_VERSION_LINE}")
    list(JOIN FILE_VERSION_COMPONENTS "." FILE_VERSION)

    # This is unlike all other targets in the runtime, and is largely a long standing contract between
    # debuggers and the runtime. This long form supports postmortem cross-architecture diagnostic scenarios.
    # It doesn't make sense to complicate install_clr for this purpose, so keep it self-contained here.
    set(LONG_NAME_BASE mscordaccore_${LONG_NAME_HOST_ARCH}_${LONG_NAME_TARGET_ARCH}_${FILE_VERSION})
    install(PROGRAMS $<TARGET_FILE:mscordaccore> RENAME ${LONG_NAME_BASE}.dll DESTINATION sharedFramework COMPONENT debug)
    install(FILES $<TARGET_PDB_FILE:mscordaccore> RENAME ${LONG_NAME_BASE}.pdb DESTINATION sharedFramework/PDB COMPONENT debug)
endif()
