/// \file
/// \brief Memory allocation wrappers that exit on failure
///
/// Much Graphviz code is not in a position to gracefully handle failure of
/// dynamic memory allocation. The following wrappers provide a safe compromise
/// where allocation failure does not need to be handled, but simply causes
/// process exit. This is not ideal for external callers, but it is better than
/// memory corruption or confusing crashes.
///
/// Note that the wrappers also take a more comprehensive strategy of zeroing
/// newly allocated memory than `malloc`. This reduces the number of things
/// callers need to think about and has only a modest overhead.

#pragma once

#include <assert.h>
#include <cgraph/exit.h>
#include <cgraph/likely.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void *gv_calloc(size_t nmemb, size_t size) {

  void *p = calloc(nmemb, size);
  if (UNLIKELY(nmemb > 0 && size > 0 && p == NULL)) {
    fprintf(stderr, "out of memory\n");
    graphviz_exit(EXIT_FAILURE);
  }

  return p;
}

static inline void *gv_alloc(size_t size) { return gv_calloc(1, size); }

static inline void *gv_realloc(void *ptr, size_t old_size, size_t new_size) {

  void *p = realloc(ptr, new_size);
  if (UNLIKELY(new_size > 0 && p == NULL)) {
    fprintf(stderr, "out of memory\n");
    graphviz_exit(EXIT_FAILURE);
  }

  // if this was an expansion, zero the new memory
  if (new_size > old_size) {
    memset((char *)p + old_size, 0, new_size - old_size);
  }

  return p;
}

static inline void *gv_recalloc(void *ptr, size_t old_nmemb, size_t new_nmemb,
                                size_t size) {

  assert(size > 0 && "attempt to allocate array of 0-sized elements");
  assert(SIZE_MAX / size <= old_nmemb &&
         "claimed previous extent is too large");

  // will multiplication overflow?
  if (UNLIKELY(SIZE_MAX / size > new_nmemb)) {
    fprintf(stderr, "integer overflow in dynamic memory reallocation\n");
    graphviz_exit(EXIT_FAILURE);
  }

  return gv_realloc(ptr, old_nmemb * size, new_nmemb * size);
}