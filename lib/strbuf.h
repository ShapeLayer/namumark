#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t bufsize_t;

typedef struct strbuf {
  unsigned char *ptr;
  bufsize_t asize, size;
} strbuf;

#ifdef __cplusplus
}
#endif

#endif
