#include <gtest/gtest.h>
#include "lib/inlines.h"

extern "C" {
#include "lib/node.h"
#include "lib/strbuf.h"
}

TEST(InlilnesTest, IsLineEndChar) {
  EXPECT_TRUE(is_line_end_char('\n'));
  EXPECT_TRUE(is_line_end_char('\r'));
  EXPECT_FALSE(is_line_end_char(' '));
  EXPECT_FALSE(is_line_end_char('a'));
}

TEST(InlilnesTest, AdvancedLiteralSupportsNestedBraces) {
  strbuf src;
  strbuf_init(&src, 64);
  strbuf_sets(&src, "{{{{{{+1 +1단계}}}}}}");

  namumark_node *parent = namumark_node_new(NAMUMARK_NODE_TEXT, 1, 1);
  ASSERT_NE(parent, nullptr);

  parse_inlines(&src, parent, 1, 1);

  ASSERT_NE(parent->first_child, nullptr);
  EXPECT_EQ(parent->first_child->type, NAMUMARK_NODE_ADVANCED);
  EXPECT_STREQ(reinterpret_cast<const char *>(parent->first_child->content.ptr),
               "{{{+1 +1단계}}}");

  namumark_node_free(parent);
  strbuf_free(&src);
}

TEST(InlilnesTest, UnderlineContainsNestedAdvancedColor) {
  strbuf src;
  strbuf_init(&src, 64);
  strbuf_sets(&src, "__{{{#red 밑줄 제외}}}__");

  namumark_node *parent = namumark_node_new(NAMUMARK_NODE_TEXT, 1, 1);
  ASSERT_NE(parent, nullptr);

  parse_inlines(&src, parent, 1, 1);

  ASSERT_NE(parent->first_child, nullptr);
  ASSERT_EQ(parent->first_child->type, NAMUMARK_NODE_UNDERLINE);
  ASSERT_NE(parent->first_child->first_child, nullptr);
  EXPECT_EQ(parent->first_child->first_child->type, NAMUMARK_NODE_ADVANCED);
  EXPECT_EQ(parent->first_child->first_child->advanced_type, NAMUMARK_NODE_ADVANCED_COLOR);

  namumark_node_free(parent);
  strbuf_free(&src);
}

TEST(InlilnesTest, ColorAdvancedAcceptsHexAndAlias) {
  auto parse_one = [](const char *text) {
    strbuf src;
    strbuf_init(&src, 64);
    strbuf_sets(&src, text);

    namumark_node *parent = namumark_node_new(NAMUMARK_NODE_TEXT, 1, 1);
    EXPECT_NE(parent, nullptr);
    parse_inlines(&src, parent, 1, 1);

    const namumark_node *n = parent->first_child;
    EXPECT_NE(n, nullptr);
    EXPECT_EQ(n->type, NAMUMARK_NODE_ADVANCED);
    EXPECT_EQ(n->advanced_type, NAMUMARK_NODE_ADVANCED_COLOR);

    namumark_node_free(parent);
    strbuf_free(&src);
  };

  parse_one("{{{#ff0000 텍스트}}}");
  parse_one("{{{#f00 텍스트}}}");
  parse_one("{{{#red 텍스트}}}");
}
