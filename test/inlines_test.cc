#include <gtest/gtest.h>
#include "lib/inlines.h"

TEST(InlilnesTest, IsLineEndChar) {
  EXPECT_TRUE(is_line_end_char('\n'));
  EXPECT_TRUE(is_line_end_char('\r'));
  EXPECT_FALSE(is_line_end_char(' '));
  EXPECT_FALSE(is_line_end_char('a'));
}
