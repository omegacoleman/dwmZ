add_subdirectory(dwmz)

if(${WITH_UTILITY})
  add_subdirectory(wmbartest)

  if(${WITH_FCITX})
    add_subdirectory(fcitximc)
  endif()
  if(${WITH_WP})
    add_subdirectory(wpvolc)
  endif()
endif()

