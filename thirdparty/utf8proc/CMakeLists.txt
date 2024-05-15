add_library(utf8proc OBJECT
  src/utf8proc.c
)

target_include_directories(utf8proc PUBLIC include)

add_library(utf8proc::utf8proc ALIAS utf8proc)

