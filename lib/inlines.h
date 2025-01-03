#ifndef __INLINES_H__
#define __INLINES_H__
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline bool S_is_line_end_char(unsigned char c);
bool is_line_end_char(unsigned char c);

#ifdef __cplusplus
}
#endif

#endif // __INLINES_H__
