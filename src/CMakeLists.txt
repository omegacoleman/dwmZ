add_subdirectory(dwmz)
add_subdirectory(setdwmzpanel)

if(${WITH_UTILITY})
  add_subdirectory(wmbartest)

  if(${WITH_WP})
    add_subdirectory(wpvolc)
  endif()
endif()

