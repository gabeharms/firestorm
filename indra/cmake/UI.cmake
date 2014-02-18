# -*- cmake -*-
include(Prebuilt)
include(FreeType)

if (STANDALONE)
  include(FindPkgConfig)
    
  if (LINUX)
    set(PKGCONFIG_PACKAGES
        atk
        cairo
        gdk-2.0
        gdk-pixbuf-2.0
        glib-2.0
        gmodule-2.0
        gtk+-2.0
        gthread-2.0
        libpng
        pango
        pangoft2
        pangox
        pangoxft
        sdl
        )
  endif (LINUX)

  foreach(pkg ${PKGCONFIG_PACKAGES})
    pkg_check_modules(${pkg} REQUIRED ${pkg})
    include_directories(${${pkg}_INCLUDE_DIRS})
    link_directories(${${pkg}_LIBRARY_DIRS})
    list(APPEND UI_LIBRARIES ${${pkg}_LIBRARIES})
    add_definitions(${${pkg}_CFLAGS_OTHERS})
  endforeach(pkg)
else (STANDALONE)
  use_prebuilt_binary(gtk-atk-pango-glib)
  if (LINUX)
    set(UI_LIB_NAMES
        freetype
        atk-1.0
        gdk-x11-2.0
        gdk_pixbuf-2.0
        glib-2.0
        gmodule-2.0
        gobject-2.0
        gthread-2.0
        gtk-x11-2.0
        pango-1.0
        pangoft2-1.0
        pangox-1.0
        pangoxft-1.0
        )
    foreach(libname ${UI_LIB_NAMES})
      find_library(UI_LIB_${libname}
                   NAMES ${libname}
                   PATHS
                     debug ${LIBS_PREBUILT_DIR}/lib/debug
                     optimized ${LIBS_PREBUILT_DIR}/lib/release
                   NO_DEFAULT_PATH
                   )
      set(UI_LIBRARIES ${UI_LIBRARIES} ${UI_LIB_${libname}})
    endforeach(libname)

    if (ND_BUILD64BIT_ARCH)
      find_library(UI_LIB_gio-2.0
                   NAMES gio-2.0
                   PATHS
                     debug ${LIBS_PREBUILT_DIR}/lib/debug
                     optimized ${LIBS_PREBUILT_DIR}/lib/release
                   NO_DEFAULT_PATH
                   )
      set(UI_LIBRARIES ${UI_LIBRARIES} ${UI_LIB_gio-2.0})
    endif(ND_BUILD64BIT_ARCH)

    set(UI_LIBRARIES ${UI_LIBRARIES} Xinerama)
  endif (LINUX)

  include_directories (
      ${LIBS_PREBUILT_DIR}/include
      ${LIBS_PREBUILT_DIR}/include
      )
  foreach(include ${${LL_ARCH}_INCLUDES})
      include_directories(${LIBS_PREBUILT_DIR}/include/${include})
  endforeach(include)
endif (STANDALONE)

if (LINUX)
  add_definitions(-DLL_GTK=1 -DLL_X11=1)
endif (LINUX)
