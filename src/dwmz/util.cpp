/* See LICENSE file for copyright and license details. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.hpp"
#include "ytk/misc/common.hpp"

void *ecalloc(size_t nmemb, size_t size) {
  void *p;

  if(!(p = calloc(nmemb, size)))
    ytk::raise("bad alloc");
  return p;
}
