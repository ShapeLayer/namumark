#include <gtest/gtest.h>
#include "lib/strbuf.h"

TEST(StrbufTest, InitAssertions) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 0);

  EXPECT_EQ(strbuf__init_buf, buffer->ptr);
  EXPECT_EQ(0, buffer->size);
  EXPECT_EQ(0, buffer->asize);

  strbuf_free(buffer);
}
