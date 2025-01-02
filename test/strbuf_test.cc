#include <gtest/gtest.h>
#include "lib/strbuf.h"

TEST(StrbufTest, Init) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  EXPECT_NE(strbuf__init_buf, buffer->ptr);
  EXPECT_EQ(0, buffer->size);
  // 16 = ((10 + 10 / 2 + 1) + 7) & ~7
  EXPECT_EQ(16, buffer->asize);

  strbuf_free(buffer);
}

TEST(StrbufTest, Grow) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  strbuf_grow(buffer, 20);

  EXPECT_NE(strbuf__init_buf, buffer->ptr);
  EXPECT_EQ(0, buffer->size);
  // 32 = ((20 + 20 / 2 + 1) + 7) & ~7
  EXPECT_EQ(32, buffer->asize);

  strbuf_free(buffer);
}

TEST(StrbufTest, GrowOverflow) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));

  EXPECT_DEATH(strbuf_grow(buffer, BUFSIZE_MAX), ".*");
}

TEST(StrbufTest, Free) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  strbuf_free(buffer);

  EXPECT_EQ(strbuf__init_buf, buffer->ptr);
  EXPECT_EQ(0, buffer->size);
  EXPECT_EQ(0, buffer->asize);
}

TEST(StrbufTest, Clear) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  strbuf_clear(buffer);

  EXPECT_NE(strbuf__init_buf, buffer->ptr);
  EXPECT_EQ('\0', buffer->ptr[0]);

  strbuf_free(buffer);
}

TEST(StrbufTest, Set) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "hello";
  strbuf_set(buffer, data, 5);

  EXPECT_NE(strbuf__init_buf, buffer->ptr);
  EXPECT_EQ(5, buffer->size);
  EXPECT_EQ(16, buffer->asize);
  EXPECT_STREQ("hello", (char*)buffer->ptr);

  strbuf_free(buffer);
}

TEST(StrbufTest, SetS) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  strbuf_sets(buffer, "hello");

  EXPECT_NE(strbuf__init_buf, buffer->ptr);
  EXPECT_EQ(5, buffer->size);
  EXPECT_EQ(16, buffer->asize);
  EXPECT_STREQ("hello", (char*)buffer->ptr);

  strbuf_free(buffer);
}

TEST(StrbufTest, PutC) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  strbuf_putc(buffer, 'h');

  EXPECT_NE(strbuf__init_buf, buffer->ptr);
  EXPECT_EQ(1, buffer->size);
  EXPECT_EQ(16, buffer->asize);
  EXPECT_STREQ("h", (char*)buffer->ptr);

  strbuf_free(buffer);
}

TEST(StrbufTest, Put) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "hello";
  strbuf_put(buffer, data, 5);

  EXPECT_NE(strbuf__init_buf, buffer->ptr);
  EXPECT_EQ(5, buffer->size);
  EXPECT_EQ(16, buffer->asize);
  EXPECT_STREQ("hello", (char*)buffer->ptr);

  strbuf_free(buffer);
}

TEST(StrbufTest, PutS) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  strbuf_puts(buffer, "hello");

  EXPECT_NE(strbuf__init_buf, buffer->ptr);
  EXPECT_EQ(5, buffer->size);
  EXPECT_EQ(16, buffer->asize);
  EXPECT_STREQ("hello", (char*)buffer->ptr);

  strbuf_free(buffer);
}

TEST(StrbufTest, CopyCstr) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "hello";
  strbuf_set(buffer, data, 5);

  char dest[10];
  strbuf_copy_cstr(dest, 10, buffer);

  EXPECT_STREQ("hello", dest);

  strbuf_free(buffer);
}

TEST(StrbufTest, Swap) {
  strbuf *buffer1;
  buffer1 = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer1, 10);

  strbuf *buffer2;
  buffer2 = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer2, 10);

  unsigned char data1[] = "hello";
  strbuf_set(buffer1, data1, 5);

  unsigned char data2[] = "world";
  strbuf_set(buffer2, data2, 5);

  strbuf_swap(buffer1, buffer2);

  EXPECT_EQ(5, buffer1->size);
  EXPECT_STREQ("world", (char*)buffer1->ptr);

  EXPECT_EQ(5, buffer2->size);
  EXPECT_STREQ("hello", (char*)buffer2->ptr);

  strbuf_free(buffer1);
  strbuf_free(buffer2);
}

TEST(StrbufTest, Detach) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "hello";
  strbuf_set(buffer, data, 5);

  unsigned char *data2 = strbuf_detach(buffer);

  EXPECT_EQ(0, buffer->size);
  EXPECT_EQ(0, buffer->asize);
  EXPECT_EQ(0, buffer->ptr[0]);
  EXPECT_STREQ("hello", (char*)data2);

  free(data2);
}

TEST(StrbufTest, Cmp) {
  strbuf *buffer1;
  buffer1 = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer1, 10);

  strbuf *buffer2;
  buffer2 = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer2, 10);

  unsigned char data1[] = "hello";
  strbuf_set(buffer1, data1, 5);

  unsigned char data2[] = "world";
  strbuf_set(buffer2, data2, 5);

  int result = strbuf_cmp(buffer1, buffer2);
  EXPECT_LT(result, 0);

  strbuf_set(buffer2, data1, 5);
  result = strbuf_cmp(buffer1, buffer2);
  EXPECT_EQ(0, result);

  strbuf_free(buffer1);
  strbuf_free(buffer2);
}

TEST(StrbufTest, StrChr) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "hello";
  strbuf_set(buffer, data, 5);

  bufsize_t result = strbuf_strchr(buffer, 'l', 0);
  EXPECT_EQ(2, result);

  result = strbuf_strchr(buffer, 'l', 3);
  EXPECT_EQ(3, result);

  result = strbuf_strchr(buffer, 'l', 4);
  EXPECT_EQ(-1, result);

  strbuf_free(buffer);
}

TEST(StrbufTest, StrRCchr) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "hello";
  strbuf_set(buffer, data, 5);

  bufsize_t result = strbuf_strrchr(buffer, 'l', 4);
  EXPECT_EQ(3, result);

  result = strbuf_strrchr(buffer, 'l', 3);
  EXPECT_EQ(3, result);

  result = strbuf_strrchr(buffer, 'l', 2);
  EXPECT_EQ(2, result);

  result = strbuf_strrchr(buffer, 'l', 1);
  EXPECT_EQ(-1, result);

  strbuf_free(buffer);
}

TEST(StrbufTest, Truncate) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "hello";
  strbuf_set(buffer, data, 5);

  strbuf_truncate(buffer, 3);

  EXPECT_EQ(3, buffer->size);
  EXPECT_STREQ("hel", (char*)buffer->ptr);

  strbuf_free(buffer);
}

TEST(StrbufTest, Drop) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "hello";
  strbuf_set(buffer, data, 5);

  strbuf_drop(buffer, 3);

  EXPECT_EQ(2, buffer->size);
  EXPECT_STREQ("lo", (char*)buffer->ptr);

  strbuf_free(buffer);
}

TEST(StrbufTest, RTrim) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "hello  ";
  strbuf_set(buffer, data, 7);

  strbuf_rtrim(buffer);

  EXPECT_EQ(5, buffer->size);
  EXPECT_STREQ("hello", (char*)buffer->ptr);

  strbuf_free(buffer);
}

TEST(StrbufTest, Trim) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "  hello  ";
  strbuf_set(buffer, data, 9);

  strbuf_trim(buffer);

  EXPECT_EQ(5, buffer->size);
  EXPECT_STREQ("hello", (char*)buffer->ptr);

  strbuf_free(buffer);
}

TEST(StrbufTest, NormalizeWhitespace) {
  strbuf *buffer;
  buffer = (strbuf*)calloc(1, sizeof(strbuf));
  strbuf_init(buffer, 10);

  unsigned char data[] = "  hello  world   ";
  strbuf_set(buffer, data, 16);

  strbuf_normalize_whitespace(buffer);

  EXPECT_EQ(11, buffer->size);
  EXPECT_STREQ("hello world", (char*)buffer->ptr);

  strbuf_free(buffer);
}

TEST(StrbufTest, Unescape) {}
