# -*- cmake -*-
#
# Compilation options shared by all Second Life components.

if(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
set(${CMAKE_CURRENT_LIST_FILE}_INCLUDED "YES")

include(Variables)

# Portable compilation flags.
set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG -DLL_DEBUG=1")
set(CMAKE_CXX_FLAGS_RELEASE
    "-DLL_RELEASE=1 -DLL_RELEASE_FOR_DOWNLOAD=1 -DNDEBUG") 

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
    "-DLL_RELEASE=1 -DNDEBUG -DLL_RELEASE_WITH_DEBUG_INFO=1")

# Configure crash reporting
set(RELEASE_CRASH_REPORTING OFF CACHE BOOL "Enable use of crash reporting in release builds")
set(NON_RELEASE_CRASH_REPORTING OFF CACHE BOOL "Enable use of crash reporting in developer builds")

if(RELEASE_CRASH_REPORTING)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DLL_SEND_CRASH_REPORTS=1")
endif()

if(NON_RELEASE_CRASH_REPORTING)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -DLL_SEND_CRASH_REPORTS=1")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DLL_SEND_CRASH_REPORTS=1")
endif()  

# Don't bother with a MinSizeRel build.
set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Release;Debug" CACHE STRING
    "Supported build types." FORCE)


# Platform-specific compilation flags.

if (WINDOWS)
  # Don't build DLLs.
  set(BUILD_SHARED_LIBS OFF)

  # for "backwards compatibility", cmake sneaks in the Zm1000 option which royally
  # screws incredibuild. this hack disables it.
  # for details see: http://connect.microsoft.com/VisualStudio/feedback/details/368107/clxx-fatal-error-c1027-inconsistent-values-for-ym-between-creation-and-use-of-precompiled-headers
  # http://www.ogre3d.org/forums/viewtopic.php?f=2&t=60015
  # http://www.cmake.org/pipermail/cmake/2009-September/032143.html
  string(REPLACE "/Zm1000" " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})

  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi /MDd /MP -D_SCL_SECURE_NO_WARNINGS=1"
      CACHE STRING "C++ compiler debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO 
      "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Od /Zi /MD /Ob0 /MP -D_SECURE_STL=0"
      CACHE STRING "C++ compiler release-with-debug options" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE
      "${CMAKE_CXX_FLAGS_RELEASE} ${LL_CXX_FLAGS} /O2 /Zi /MD /MP /Ob2 /Oi /Ot /GF /Gy -D_SECURE_STL=0 -D_HAS_ITERATOR_DEBUGGING=0"
      CACHE STRING "C++ compiler release options" FORCE)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")


  set(CMAKE_CXX_STANDARD_LIBRARIES "")
  set(CMAKE_C_STANDARD_LIBRARIES "")

# <FS:Ansariel> [AVX Optimization]
#  add_definitions(
#      /DLL_WINDOWS=1
#      /DDOM_DYNAMIC
#      /DUNICODE
#      /D_UNICODE 
#      /GS
#      /TP
#      /W3
#      /c
#      /Zc:forScope
#      /nologo
#      /Oy-
#      /Zc:wchar_t-
#      /arch:SSE2
#      /fp:fast
#      )
  if (USE_AVX_OPTIMIZATION)
    add_definitions(
        /DLL_WINDOWS=1
        /DDOM_DYNAMIC
        /DUNICODE
        /D_UNICODE 
        /GS
        /TP
        /W3
        /c
        /Zc:forScope
        /nologo
        /Oy-
        /Zc:wchar_t-
        /arch:AVX
#        /fp:fast
        )
  else (USE_AVX_OPTIMIZATION)
    add_definitions(
        /DLL_WINDOWS=1
      /DNOMINMAX
        /DDOM_DYNAMIC
        /DUNICODE
        /D_UNICODE 
        /GS
        /TP
        /W3
        /c
        /Zc:forScope
        /nologo
        /Oy-
        /Zc:wchar_t-
        /arch:SSE2
#        /fp:fast
        )
  endif (USE_AVX_OPTIMIZATION)
# </FS:Ansariel> [AVX Optimization]	
     
  # Are we using the crummy Visual Studio KDU build workaround?
  if (NOT VS_DISABLE_FATAL_WARNINGS)
    add_definitions(/WX)
  endif (NOT VS_DISABLE_FATAL_WARNINGS)

  # configure win32 API for windows XP+ compatibility
  set(WINVER "0x0501" CACHE STRING "Win32 API Target version (see http://msdn.microsoft.com/en-us/library/aa383745%28v=VS.85%29.aspx)")
  add_definitions("/DWINVER=${WINVER}" "/D_WIN32_WINNT=${WINVER}")

  if( ND_BUILD64BIT_ARCH )
   add_definitions("/wd4267 /DND_BUILD64BIT_ARCH" )
  else( ND_BUILD64BIT_ARCH )
   add_definitions("/fp:fast" )
  endif( ND_BUILD64BIT_ARCH )
 
endif (WINDOWS)


if (LINUX)
  set(CMAKE_SKIP_RPATH TRUE)

  # Here's a giant hack for Fedora 8, where we can't use
  # _FORTIFY_SOURCE if we're using a compiler older than gcc 4.1.

  find_program(GXX g++)
  mark_as_advanced(GXX)

  if (GXX)
    execute_process(
        COMMAND ${GXX} --version
        COMMAND sed "s/^[gc+ ]*//"
        COMMAND head -1
        OUTPUT_VARIABLE GXX_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
  else (GXX)
    set(GXX_VERSION x)
  endif (GXX)

  # The quoting hack here is necessary in case we're using distcc or
  # ccache as our compiler.  CMake doesn't pass the command line
  # through the shell by default, so we end up trying to run "distcc"
  # " g++" - notice the leading space.  Ugh.

  execute_process(
      COMMAND sh -c "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} --version"
      COMMAND sed "s/^[gc+ ]*//"
      COMMAND head -1
      OUTPUT_VARIABLE CXX_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE)

  #<FS:ND> Gentoo defines _FORTIFY_SOURCE by default
  if (NOT ${GXX_VERSION} MATCHES "Gentoo 4.[78].*")
  #</FS:ND>

  if (${GXX_VERSION} STREQUAL ${CXX_VERSION})
    add_definitions(-D_FORTIFY_SOURCE=2)
  else (${GXX_VERSION} STREQUAL ${CXX_VERSION})
    if (NOT ${GXX_VERSION} MATCHES " 4.1.*Red Hat")
      add_definitions(-D_FORTIFY_SOURCE=2)
    endif (NOT ${GXX_VERSION} MATCHES " 4.1.*Red Hat")
  endif (${GXX_VERSION} STREQUAL ${CXX_VERSION})

  #<FS:ND> Gentoo defines _FORTIFY_SOURCE by default
  endif (NOT ${GXX_VERSION} MATCHES "Gentoo 4.[78].*")
  #</FS:ND>

  # Let's actually get a numerical version of gxx's version
  STRING(REGEX REPLACE ".* ([0-9])\\.([0-9])\\.([0-9]).*" "\\1\\2\\3" CXX_VERSION_NUMBER ${CXX_VERSION})

  # Hacks to work around gcc 4.1 TC build pool machines which can't process pragma warning disables
  # This is pure rubbish; I wish there was another way.
  #
  if(${CXX_VERSION_NUMBER} LESS 420)
    set(CMAKE_CXX_FLAGS "-Wno-deprecated -Wno-uninitialized -Wno-unused-variable -Wno-unused-function ${CMAKE_CXX_FLAGS}")
  endif (${CXX_VERSION_NUMBER} LESS 420)

  if(${CXX_VERSION_NUMBER} GREATER 459)
    set(CMAKE_CXX_FLAGS "-Wno-deprecated -Wno-unused-but-set-variable -Wno-unused-variable ${CMAKE_CXX_FLAGS}")
  endif (${CXX_VERSION_NUMBER} GREATER 459)

  # gcc 4.3 and above don't like the LL boost and also
  # cause warnings due to our use of deprecated headers
  if(${CXX_VERSION_NUMBER} GREATER 429)
    add_definitions(-Wno-parentheses)
    set(CMAKE_CXX_FLAGS "-Wno-deprecated ${CMAKE_CXX_FLAGS}")
  endif (${CXX_VERSION_NUMBER} GREATER 429)

  #<FS:ND> Disable unused-but-set-variable for GCC >= 4.6. It causes a lot of warning/errors all over the source. Fixing that would result in changing a good amount of files.
  if(${CXX_VERSION_NUMBER} GREATER 460)
    set(CMAKE_CXX_FLAGS "-Wno-unused-but-set-variable ${CMAKE_CXX_FLAGS}")
  endif (${CXX_VERSION_NUMBER} GREATER 460)
  #</FS:ND>
  #<FS:ND> Disable attribute warnings for GCC >= 4.7. It causes a lot of warning/errors in boost.
  if(${CXX_VERSION_NUMBER} GREATER 470)
    set(CMAKE_CXX_FLAGS "-Wno-attributes ${CMAKE_CXX_FLAGS}")
  endif (${CXX_VERSION_NUMBER} GREATER 470)
  #</FS:ND>
  #<FS:ND> Disable unsed local typedef warnings for GCC >= 4.8. It causes a lot of warning/errors in boost.
  if(${CXX_VERSION_NUMBER} GREATER 480)
    set(CMAKE_CXX_FLAGS "-Wno-unused-local-typedefs ${CMAKE_CXX_FLAGS}")
  endif (${CXX_VERSION_NUMBER} GREATER 480)
  #</FS:ND>



  # End of hacks.

  add_definitions(
      -DLL_LINUX=1
      -D_REENTRANT
      -fexceptions
      -fno-math-errno
      -fno-strict-aliasing
      -fsigned-char
      -g
      -msse2
      -mfpmath=sse
      -pthread
#      -std=gnu++0x
      )

  add_definitions(-DAPPID=secondlife)
  add_definitions(-fvisibility=hidden)
  # don't catch SIGCHLD in our base application class for the viewer - some of our 3rd party libs may need their *own* SIGCHLD handler to work.  Sigh!  The viewer doesn't need to catch SIGCHLD anyway.
  add_definitions(-DLL_IGNORE_SIGCHLD)
  if (WORD_SIZE EQUAL 32)
    add_definitions(-march=pentium4)
  endif (WORD_SIZE EQUAL 32)
  add_definitions(-mfpmath=sse)
  #add_definitions(-ftree-vectorize) # THIS CRASHES GCC 3.1-3.2
  if (NOT STANDALONE)
    # this stops us requiring a really recent glibc at runtime
    add_definitions(-fno-stack-protector)
    # linking can be very memory-hungry, especially the final viewer link
    set(CMAKE_CXX_LINK_FLAGS "-Wl,--no-keep-memory -Wl,--build-id -Wl,-rpath,'$ORIGIN:$ORIGIN/../lib'")
  endif (NOT STANDALONE)

  # <FS:TS> Enable AVX optimizations if requested and at least GCC 4.6.
  if (USE_AVX_OPTIMIZATION)
    if (NOT (${CXX_VERSION_NUMBER} LESS 460))
      add_definitions(-mavx)
    else (NOT (${CXX_VERSION_NUMBER} LESS 460))
      error ("AVX optimizations require at least version 4.6.0 of GCC.")
    endif (NOT (${CXX_VERSION_NUMBER} LESS 460))
  endif (USE_AVX_OPTIMIZATION)

  set(CMAKE_CXX_FLAGS_DEBUG "-fno-inline ${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 ${CMAKE_CXX_FLAGS_RELEASE}")

  # <FS:ND> Build without frame pointer if requested. Otherwise profiling might not work reliable. N.B. Win32 uses FP based calling by default.
  if( NO_OMIT_FRAMEPOINTER )
    set(CMAKE_CXX_FLAGS_RELEASE "-fno-omit-frame-pointer ${CMAKE_CXX_FLAGS_RELEASE}")
  endif( NO_OMIT_FRAMEPOINTER )
  # </FS:ND>

endif (LINUX)


if (DARWIN)
  add_definitions(-DLL_DARWIN=1)
  set(CMAKE_CXX_LINK_FLAGS "-Wl,-no_compact_unwind -Wl,-headerpad_max_install_names,-search_paths_first")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_CXX_LINK_FLAGS}")
  set(DARWIN_extra_cstar_flags "-g")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${DARWIN_extra_cstar_flags}")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}  ${DARWIN_extra_cstar_flags}")
  # NOTE: it's critical that the optimization flag is put in front.
  # NOTE: it's critical to have both CXX_FLAGS and C_FLAGS covered.
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O0 ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O0 ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  if (USE_AVX_OPTIMIZATION)
    if (XCODE_VERSION GREATER 4.9)
      set(CMAKE_XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS AVX)
      set(CMAKE_XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL -Ofast)
      set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -mavx ${CMAKE_CXX_FLAGS_RELEASE}")
      set(CMAKE_C_FLAGS_RELEASE "-Ofast -mavx ${CMAKE_C_FLAGS_RELEASE}")
	else (XCODE_VERSION GREATER 4.9)
	  error("Darwin AVX Optimizations only available on Xcode5 with Clang, silly person!")
	endif (XCODE_VERSION GREATER 4.9)
  else (USE_AVX_OPTIMIZATION)
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_X86_VECTOR_INSTRUCTIONS SSE3)
	set(CMAKE_XCODE_ATTRIBUTE_GCC_OPTIMIZATION_LEVEL -O3)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -msse3 ${CMAKE_CXX_FLAGS_RELEASE}")
	set(CMAKE_C_FLAGS_RELEASE "-O3 -msse3 ${CMAKE_C_FLAGS_RELEASE}")
  endif (USE_AVX_OPTIMIZATION)
  if (XCODE_VERSION GREATER 4.2)
    set(ENABLE_SIGNING TRUE)
    set(SIGNING_IDENTITY "Developer ID Application: Linden Research, Inc.")
  endif (XCODE_VERSION GREATER 4.2)
  # <FS:ND> Build without frame pointer if requested. Otherwise profiling might not work reliable. N.B. Win32 uses FP based calling by default.
  if( NO_OMIT_FRAMEPOINTER )
    set(CMAKE_CXX_FLAGS_RELEASE "-fno-omit-frame-pointer ${CMAKE_CXX_FLAGS_RELEASE}")
  endif( NO_OMIT_FRAMEPOINTER )
  # </FS:ND>

endif (DARWIN)


if (LINUX OR DARWIN)
  set(GCC_WARNINGS "-Wall -Wno-sign-compare -Wno-trigraphs")

  if (NOT GCC_DISABLE_FATAL_WARNINGS)
    set(GCC_WARNINGS "${GCC_WARNINGS} -Werror")
  endif (NOT GCC_DISABLE_FATAL_WARNINGS)

  if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" AND DARWIN AND XCODE_VERSION GREATER 4.9)
    set(GCC_CXX_WARNINGS "$[GCC_WARNINGS] -Wno-reorder -Wno-unused-const-variable -Wno-format-extra-args -Wno-unused-private-field -Wno-unused-function -Wno-tautological-compare -Wno-empty-body -Wno-unused-variable -Wno-unused-value")
  else (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" AND DARWIN AND XCODE_VERSION GREATER 4.9)
  #elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    set(GCC_CXX_WARNINGS "${GCC_WARNINGS} -Wno-reorder -Wno-non-virtual-dtor")
  endif ()

  set(CMAKE_C_FLAGS "${GCC_WARNINGS} ${CMAKE_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${GCC_CXX_WARNINGS} ${CMAKE_CXX_FLAGS}")

  if (WORD_SIZE EQUAL 32)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
  elseif (WORD_SIZE EQUAL 64)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64")
  endif (WORD_SIZE EQUAL 32)

  if (ND_BUILD64BIT_ARCH)
   add_definitions(-DND_BUILD64BIT_ARCH)
  endif (ND_BUILD64BIT_ARCH)
endif (LINUX OR DARWIN)


if (STANDALONE)
  add_definitions(-DLL_STANDALONE=1)

  if (LINUX AND ${ARCH} STREQUAL "i686")
    add_definitions(-march=pentiumpro)
  endif (LINUX AND ${ARCH} STREQUAL "i686")

else (STANDALONE)
  set(${ARCH}_linux_INCLUDES
      atk-1.0
      cairo
      freetype
      glib-2.0
      gstreamer-0.10
      gtk-2.0
      pango-1.0
      )
endif (STANDALONE)

endif(NOT DEFINED ${CMAKE_CURRENT_LIST_FILE}_INCLUDED)
