# -*- cmake -*-

# Growl is actually libnotify on linux systems.

if (STANDALONE)
    if( LINUX )
	  add_definitions( -DHAS_GROWL)
    endif( LINUX )
    #set(LIBNOTIFY_FIND_REQUIRED ON)
    #include(FindLibnotify)
    #set(GROWL_INCLUDE_DIRS ${LIBNOTIFY_INCLUDE_DIR})
    #set(GROWL_LIBRARY ${LIBNOTIFY_LIBRARIES})
else (STANDALONE)
    if (DARWIN OR WINDOWS)
      include(Prebuilt)
      use_prebuilt_binary(Growl)
      set(GROWL_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include/Growl)
      set(GROWL_LIBRARY growl++)
      add_definitions( -DHAS_GROWL)
	elseif (LINUX)
      add_definitions( -DHAS_GROWL)
    endif ()
endif (STANDALONE)
