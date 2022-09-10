/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

#pragma once

#include <assert.h>
#include <cgraph/alloc.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/// a description of where a buffer is located
typedef enum {
  AGXBUF_INLINE = 0, ///< buffer is _within_ the containing agxbuf
  AGXBUF_ON_HEAP,    ///< buffer is dynamically allocated
  AGXBUF_ON_STACK    ///< buffer is statically allocated
} agxbuf_loc_t;

/// extensible buffer
///
/// Malloc'ed memory is never released until \p agxbdisown or \p agxbfree is
/// called.
///
/// This has the following layout assuming x86-64:
///
///                                                                  located
///                                                                  ↓
///   ┌───────────────┬───────────────┬───────────────┬──────────────┬┐
///   │      buf      │      ptr      │     eptr      │    unused    ││
///   ├───────────────┴───────────────┴───────────────┴──────────────┴┤
///   │                             store                             │
///   └───────────────────────────────────────────────────────────────┘
///   0               8               16              24              32
///
/// \p buf, \p ptr, and \p eptr are in use when \p located is \p AGXBUF_ON_HEAP
/// or \p AGXBUF_ONSTACK. \p store is in use when \p located is
/// \p AGXBUF_INLINE. Note that \p located actually _overlaps_ with \p store.
/// This works because \p store is consistently NUL terminated. That is, when
/// \p located is \p AGXBUF_INLINE (0), byte 31 actually serves as _both_
/// \p located and possibly a NUL terminator.
typedef struct {
  union {
    struct {
      char *buf;                       ///< start of buffer
      char *ptr;                       ///< next place to write
      char *eptr;                      ///< end of buffer
      char unused[sizeof(char *) - 1]; ///< padding
      uint8_t located; ///< where does the backing memory for this buffer live?
    };
    char store[sizeof(char *) *
               4]; ///< inline storage used when \p located is \p AGXBUF_INLINE
  };
} agxbuf;

/* agxbinit:
 * Initializes new agxbuf; caller provides memory.
 * Assume if init is non-null, hint = sizeof(init[])
 */
static inline void agxbinit(agxbuf *xb, unsigned int hint, char *init) {
  if (init != NULL) {
    xb->buf = init;
    xb->located = AGXBUF_ON_STACK;
  } else {
    memset(xb->store, 0, sizeof(xb->store));
    xb->located = AGXBUF_INLINE;
    return;
  }
  xb->eptr = xb->buf + hint;
  xb->ptr = xb->buf;
  *xb->ptr = '\0';
}

/* agxbfree:
 * Free any malloced resources.
 */
static inline void agxbfree(agxbuf *xb) {
  if (xb->located == AGXBUF_ON_HEAP)
    free(xb->buf);
}

/* agxblen:
 * Return number of characters currently stored.
 */
static inline size_t agxblen(const agxbuf *xb) {
  if (xb->located == AGXBUF_INLINE) {
    return strlen(xb->store);
  }
  return (size_t)(xb->ptr - xb->buf);
}

/// get the size of the backing memory of a buffer
///
/// In contrast to \p agxblen, this is the total number of usable bytes in the
/// backing store, not the total number of currently stored bytes.
///
/// \param xb Buffer to operate on
/// \return Number of usable bytes in the backing store
static inline size_t agxbsizeof(const agxbuf *xb) {
  if (xb->located == AGXBUF_INLINE) {
    return sizeof(xb->store) - 1;
  }
  return (size_t)(xb->eptr - xb->buf);
}

/* agxbpop:
 * Removes last character added, if any.
 */
static inline int agxbpop(agxbuf *xb) {

  size_t len = agxblen(xb);
  if (len == 0) {
    return -1;
  }

  if (xb->located == AGXBUF_INLINE) {
    int c = xb->store[len - 1];
    xb->store[len - 1] = '\0';
    return c;
  }

  int c = *xb->ptr--;
  return c;
}

/* agxbmore:
 * Expand buffer to hold at least ssz more bytes.
 */
static inline void agxbmore(agxbuf *xb, size_t ssz) {
  size_t cnt = 0;   // current no. of characters in buffer
  size_t size = 0;  // current buffer size
  size_t nsize = 0; // new buffer size
  char *nbuf;       // new buffer

  size = agxbsizeof(xb);
  nsize = size < BUFSIZ ? BUFSIZ : (2 * size);
  if (size + ssz > nsize)
    nsize = size + ssz;
  cnt = agxblen(xb);

  if (xb->located == AGXBUF_ON_HEAP) {
    nbuf = (char *)gv_recalloc(xb->buf, size, nsize, sizeof(char));
  } else if (xb->located == AGXBUF_ON_STACK) {
    nbuf = (char *)gv_calloc(nsize, sizeof(char));
    memcpy(nbuf, xb->buf, cnt);
  } else {
    nbuf = (char *)gv_calloc(nsize, sizeof(char));
    memcpy(nbuf, xb->store, cnt);
  }
  xb->buf = nbuf;
  xb->ptr = xb->buf + cnt;
  xb->eptr = xb->buf + nsize;
  xb->located = AGXBUF_ON_HEAP;
}

/* support for extra API misuse warnings if available */
#ifdef __GNUC__
#define PRINTF_LIKE(index, first) __attribute__((format(printf, index, first)))
#else
#define PRINTF_LIKE(index, first) /* nothing */
#endif

/* agxbprint:
 * Printf-style output to an agxbuf
 */
static inline PRINTF_LIKE(2, 3) int agxbprint(agxbuf *xb, const char *fmt,
                                              ...) {
  va_list ap;
  size_t size;
  int result;

  va_start(ap, fmt);

  // determine how many bytes we need to print
  {
    va_list ap2;
    int rc;
    va_copy(ap2, ap);
    rc = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (rc < 0) {
      va_end(ap);
      return rc;
    }
    size = (size_t)rc + 1; // account for NUL terminator
  }

  // do we need to expand the buffer?
  {
    size_t unused_space = agxbsizeof(xb) - agxblen(xb);
    if (unused_space < size) {
      size_t extra = size - unused_space;
      agxbmore(xb, extra);
    }
  }

  // we can now safely print into the buffer
  size_t len = agxblen(xb);
  char *dst = xb->located == AGXBUF_INLINE ? &xb->store[len] : xb->ptr;
  result = vsnprintf(dst, size, fmt, ap);
  assert(result == (int)(size - 1) || result < 0);
  if (result > 0 && xb->located != AGXBUF_INLINE) {
    xb->ptr += (size_t)result;
  }

  va_end(ap);
  return result;
}

#undef PRINTF_LIKE

/* agxbput_n:
 * Append string s of length ssz into xb
 */
static inline size_t agxbput_n(agxbuf *xb, const char *s, size_t ssz) {
  if (ssz == 0) {
    return 0;
  }
  if (ssz > agxbsizeof(xb) - agxblen(xb))
    agxbmore(xb, ssz);
  if (xb->located == AGXBUF_INLINE) {
    size_t len = agxblen(xb);
    memcpy(&xb->store[len], s, ssz);
    xb->store[len + ssz] = '\0';
  } else {
    memcpy(xb->ptr, s, ssz);
    xb->ptr += ssz;
  }
  return ssz;
}

/* agxbput:
 * Append string s into xb
 */
static inline size_t agxbput(agxbuf *xb, const char *s) {
  size_t ssz = strlen(s);

  return agxbput_n(xb, s, ssz);
}

/* agxbputc:
 * Add character to buffer.
 *  int agxbputc(agxbuf*, char)
 */
static inline int agxbputc(agxbuf *xb, char c) {

  // ignore pushing a NUL terminator into an inline buffer, because it is always
  // maintained NUL terminated
  if (xb->located == AGXBUF_INLINE && c == '\0') {
    return 0;
  }

  if (agxblen(xb) >= agxbsizeof(xb)) {
    agxbmore(xb, 1);
  }
  if (xb->located == AGXBUF_INLINE) {
    size_t len = agxblen(xb);
    xb->store[len] = c;
    xb->store[len + 1] = '\0';
  } else {
    *xb->ptr++ = c;
  }
  return 0;
}

/* agxbuse:
 * Null-terminates buffer; resets and returns pointer to data. The buffer is
 * still associated with the agxbuf and will be overwritten on the next, e.g.,
 * agxbput. If you want to retrieve and disassociate the buffer, use agxbdisown
 * instead.
 */
static inline char *agxbuse(agxbuf *xb) {

  // for an inline buffer, we need to do something unorthodox in order to return
  // a usable string, but also reset the buffer for reuse in the next call
  if (xb->located == AGXBUF_INLINE) {
    // if we have enough room, shuffle the string one byte forwards, treat the
    // string beginning at `[1]` as the value to return and write a NUL byte to
    // `[0]` to create an empty string to be seen by the next call
    size_t len = agxblen(xb);
    if (len < agxbsizeof(xb)) {
      memmove(&xb->store[1], &xb->store[0], len + 1);
      xb->store[0] = '\0';
      return &xb->store[1];
    }

    // if not, force this buffer to be relocated to the heap
    agxbmore(xb, 1);
  }

  (void)agxbputc(xb, '\0');
  xb->ptr = xb->buf;
  return xb->ptr;
}

/* agxbstart:
 * Return pointer to beginning of buffer.
 */
static inline char *agxbstart(agxbuf *xb) {
  if (xb->located == AGXBUF_INLINE) {
    return xb->store;
  }
  return xb->buf;
}

/* agxbclear:
 * Resets pointer to data;
 */
static inline void agxbclear(agxbuf *xb) {
  if (xb->located == AGXBUF_INLINE) {
    xb->store[0] = '\0';
  } else {
    xb->ptr = xb->buf;
  }
}

/* agxbnext:
 * Next position for writing.
 */
static inline char *agxbnext(agxbuf *xb) {
  if (xb->located == AGXBUF_INLINE) {
    return &xb->store[agxblen(xb)];
  }
  return xb->ptr;
}

/* agxbdisown:
 * Disassociate the backing buffer from this agxbuf and return it. The buffer is
 * NUL terminated before being returned. If the agxbuf is using stack memory,
 * this will first copy the data to a new heap buffer to then return. If you
 * want to temporarily access the string in the buffer, but have it overwritten
 * and reused the next time, e.g., agxbput is called, use agxbuse instead of
 * agxbdisown.
 */
static inline char *agxbdisown(agxbuf *xb) {
  char *buf;

  if (xb->located == AGXBUF_INLINE) {
    // the (NUL terminated) string lives in `store`, so we need to copy its
    // contents to heap memory
    buf = gv_strdup(xb->store);

  } else if (xb->located == AGXBUF_ON_STACK) {
    // the buffer is not dynamically allocated, so we need to copy its contents
    // to heap memory

    buf = gv_strndup(xb->buf, agxblen(xb));

  } else {
    // the buffer is already dynamically allocated, so terminate it and then
    // take it as-is
    agxbputc(xb, '\0');
    buf = xb->buf;
  }

  // reset xb to a state where it is usable
  agxbinit(xb, 0, NULL);

  return buf;
}
