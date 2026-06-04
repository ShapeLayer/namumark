#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include "lib/node.h"
#include "lib/parser.h"
}

static bool has_child_of_type(const namumark_node *parent, namumark_node_type type) {
  if (parent == nullptr) {
    return false;
  }

  const namumark_node *child = parent->first_child;
  while (child != nullptr) {
    if (child->type == type) {
      return true;
    }
    child = child->next;
  }

  return false;
}

TEST(ParserTest, ParsesRedirectAsFirstLineOnly) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input = "#redirect 대상 문서\n== 무시되어야 함 ==\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  ASSERT_NE(doc->first_child, nullptr);
  EXPECT_EQ(doc->first_child->type, NAMUMARK_NODE_REDIRECT);
  EXPECT_STREQ(reinterpret_cast<const char *>(doc->first_child->target.ptr), "대상 문서");
  EXPECT_EQ(doc->first_child, doc->last_child);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, CollectsCategoriesOnDocumentRoot) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "[[분류:문법 도움말]]\n"
      "본문\n"
      "[[분류:나무위키#blur]]\n"
      "[[분류:정렬키|출력명]]\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  EXPECT_EQ(doc->category_count, 3);
  ASSERT_GE(doc->category_count, 3);
  EXPECT_STREQ(reinterpret_cast<const char *>(doc->categories[0].ptr), "문법 도움말");
  EXPECT_STREQ(reinterpret_cast<const char *>(doc->categories[1].ptr), "나무위키#blur");
  EXPECT_STREQ(reinterpret_cast<const char *>(doc->categories[2].ptr), "정렬키");
  ASSERT_NE(doc->first_child, nullptr);
  EXPECT_NE(doc->first_child->type, NAMUMARK_NODE_CATEGORY);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, ParsesHeadingAndInlineChildren) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input = "== 제목 ==\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *heading = doc->first_child;
  EXPECT_EQ(heading->type, NAMUMARK_NODE_HEADING);
  EXPECT_EQ(heading->level, 2);
  EXPECT_EQ(heading->folded, 0);
  EXPECT_STREQ(reinterpret_cast<const char *>(heading->content.ptr), "제목");
  EXPECT_TRUE(has_child_of_type(heading, NAMUMARK_NODE_TEXT));

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, SupportsCRLFLineEndings) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input = "* 목록\r\n> 인용\r\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  EXPECT_EQ(doc->first_child->type, NAMUMARK_NODE_LIST);
  EXPECT_EQ(doc->first_child->list_marker, NAMUMARK_LIST_MARKER_BULLET);
  ASSERT_NE(doc->first_child->next, nullptr);
  EXPECT_EQ(doc->first_child->next->type, NAMUMARK_NODE_BLOCKQUOTE);
  EXPECT_EQ(doc->first_child->next->depth, 1);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, ParsesListStartNumberAndInlineTargets) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "1.#11 시작 번호\n"
      "[[문서|표시]] [include(틀:예시, key=값)]\n"
      "[*주석 각주 본문]\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *list = doc->first_child;
  ASSERT_EQ(list->type, NAMUMARK_NODE_LIST);
  EXPECT_EQ(list->list_marker, NAMUMARK_LIST_MARKER_NUMBER);
  EXPECT_EQ(list->start_number, 11);

  const namumark_node *text_line = list->next;
  ASSERT_NE(text_line, nullptr);
  ASSERT_EQ(text_line->type, NAMUMARK_NODE_TEXT);

  const namumark_node *inline_node = text_line->first_child;
  ASSERT_NE(inline_node, nullptr);
  ASSERT_EQ(inline_node->type, NAMUMARK_NODE_LINK);
  EXPECT_STREQ(reinterpret_cast<const char *>(inline_node->target.ptr), "문서");
  EXPECT_STREQ(reinterpret_cast<const char *>(inline_node->args.ptr), "표시");

  ASSERT_NE(inline_node->next, nullptr);
  ASSERT_EQ(inline_node->next->type, NAMUMARK_NODE_TEXT);
  ASSERT_NE(inline_node->next->next, nullptr);
  ASSERT_EQ(inline_node->next->next->type, NAMUMARK_NODE_MACRO);
  EXPECT_STREQ(reinterpret_cast<const char *>(inline_node->next->next->target.ptr), "include");
  EXPECT_EQ(inline_node->next->next->macro_type, NAMUMARK_NODE_MACRO_INCLUDE);

  const namumark_node *footnote = text_line->next;
  ASSERT_NE(footnote, nullptr);
  ASSERT_EQ(footnote->type, NAMUMARK_NODE_FOOTNOTE_DEFINITION);
  EXPECT_STREQ(reinterpret_cast<const char *>(footnote->label.ptr), "주석");

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, ParsesFixedCommentAndAdvancedSubtype) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "##@ 고정 주석\n"
      "문장 중간 ## 주석 아님\n"
      "{{{#!style .x { color: red; }}}}\n"
      "[[https://example.com|외부]]\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *comment_line = doc->first_child;
  ASSERT_EQ(comment_line->type, NAMUMARK_NODE_TEXT);
  ASSERT_NE(comment_line->first_child, nullptr);
  ASSERT_EQ(comment_line->first_child->type, NAMUMARK_NODE_COMMENT);
  EXPECT_EQ(comment_line->first_child->fixed_comment, 1);

  const namumark_node *text_line = comment_line->next;
  ASSERT_NE(text_line, nullptr);
  ASSERT_EQ(text_line->type, NAMUMARK_NODE_TEXT);
  ASSERT_NE(text_line->first_child, nullptr);
  EXPECT_EQ(text_line->first_child->type, NAMUMARK_NODE_TEXT);

  const namumark_node *advanced_line = text_line->next;
  ASSERT_NE(advanced_line, nullptr);
  ASSERT_EQ(advanced_line->type, NAMUMARK_NODE_TEXT);
  ASSERT_NE(advanced_line->first_child, nullptr);
  ASSERT_EQ(advanced_line->first_child->type, NAMUMARK_NODE_ADVANCED);
  EXPECT_EQ(advanced_line->first_child->advanced_type, NAMUMARK_NODE_ADVANCED_STYLE);

  const namumark_node *link_line = advanced_line->next;
  ASSERT_NE(link_line, nullptr);
  ASSERT_EQ(link_line->type, NAMUMARK_NODE_TEXT);
  ASSERT_NE(link_line->first_child, nullptr);
  ASSERT_EQ(link_line->first_child->type, NAMUMARK_NODE_LINK);
  EXPECT_EQ(link_line->first_child->link_type, NAMUMARK_LINK_EXTERNAL);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, ParsesWikiBlockAsBlockNode) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"word-break: keep-all\"\n"
      "|| A || B ||\n"
      "|| 1 || 2 ||\n"
      "}}}\n"
      "끝\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *wiki = doc->first_child;
  ASSERT_EQ(wiki->type, NAMUMARK_NODE_WIKI_BLOCK);
  EXPECT_STREQ(reinterpret_cast<const char *>(wiki->args.ptr), "word-break: keep-all");
  EXPECT_NE(std::string(reinterpret_cast<const char *>(wiki->content.ptr)).find("|| A || B ||"),
            std::string::npos);

  ASSERT_NE(wiki->next, nullptr);
  EXPECT_EQ(wiki->next->type, NAMUMARK_NODE_TEXT);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, ParsesNestedWikiBlocks) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"outer\"\n"
      "바깥\n"
      "{{{#!wiki style=\"inner\"\n"
      "안쪽\n"
      "}}}\n"
      "바깥끝\n"
      "}}}\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *wiki = doc->first_child;
  ASSERT_EQ(wiki->type, NAMUMARK_NODE_WIKI_BLOCK);
  EXPECT_NE(std::string(reinterpret_cast<const char *>(wiki->content.ptr)).find("{{{#!wiki style=\"inner\""),
            std::string::npos);
  EXPECT_NE(std::string(reinterpret_cast<const char *>(wiki->content.ptr)).find("안쪽"),
            std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, WikiBlockDoesNotConsumeInlineLiteralClosers) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"x\"\n"
      "|| {{{{{{+1 +1단계}}}}}} ||\n"
      "}}}\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *wiki = doc->first_child;
  ASSERT_EQ(wiki->type, NAMUMARK_NODE_WIKI_BLOCK);
  EXPECT_NE(std::string(reinterpret_cast<const char *>(wiki->content.ptr)).find("{{{{{{+1 +1단계}}}}}}"),
            std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, ParsesBlockPreformattedFromBareTripleBrace) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{\n"
      "원본: 200x100\n"
      "매개변수: width=120&height=120&object-fit=scale-down\n"
      "}}}\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *pre = doc->first_child;
  ASSERT_EQ(pre->type, NAMUMARK_NODE_PREFORMATTED);
  EXPECT_NE(std::string(reinterpret_cast<const char *>(pre->content.ptr)).find("원본: 200x100"),
            std::string::npos);
  EXPECT_NE(std::string(reinterpret_cast<const char *>(pre->content.ptr)).find("매개변수:"),
            std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, HandlesBlockBoundaryTransitionsOnSameLine) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"display: flex\"\n"
      "{{{#!wiki style=\"margin-top: -15px\"\n"
      "{{{\n"
      "원본: 200x100\n"
      "}}} none 설명\n"
      "}}} {{{#!wiki style=\"word-break: keep-all\"\n"
      "문단\n"
      "}}}\n"
      "}}}\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *outer = doc->first_child;
  ASSERT_EQ(outer->type, NAMUMARK_NODE_WIKI_BLOCK);
  EXPECT_NE(std::string(reinterpret_cast<const char *>(outer->content.ptr)).find("#!wiki style=\"margin-top: -15px\""),
            std::string::npos);
  EXPECT_NE(std::string(reinterpret_cast<const char *>(outer->content.ptr)).find("#!wiki style=\"word-break: keep-all\""),
            std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, ClosesAndReopensWikiBlockOnSameLineWithoutLeakingClosers) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"a\"\n"
      "첫 줄\n"
      "}}} {{{#!wiki style=\"b\"}}}\n"
      "끝\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *first = doc->first_child;
  ASSERT_EQ(first->type, NAMUMARK_NODE_WIKI_BLOCK);
  EXPECT_STREQ(reinterpret_cast<const char *>(first->args.ptr), "a");

  const namumark_node *second = first->next;
  ASSERT_NE(second, nullptr);
  ASSERT_EQ(second->type, NAMUMARK_NODE_WIKI_BLOCK);
  EXPECT_STREQ(reinterpret_cast<const char *>(second->args.ptr), "b");

  const namumark_node *tail = second->next;
  ASSERT_NE(tail, nullptr);
  ASSERT_EQ(tail->type, NAMUMARK_NODE_TEXT);
  EXPECT_STREQ(reinterpret_cast<const char *>(tail->content.ptr), "끝");

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, ParsesTextTailAfterPreCloseOnSameLineAsBlockText) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{\n"
      "코드\n"
      "}}} 꼬리 [[문서]]\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *pre = doc->first_child;
  ASSERT_EQ(pre->type, NAMUMARK_NODE_PREFORMATTED);

  const namumark_node *tail = pre->next;
  ASSERT_NE(tail, nullptr);
  ASSERT_EQ(tail->type, NAMUMARK_NODE_TEXT);
  ASSERT_NE(tail->first_child, nullptr);

  const namumark_node *tail_child = tail->first_child;
  EXPECT_EQ(tail_child->type, NAMUMARK_NODE_TEXT);
  ASSERT_NE(tail_child->next, nullptr);
  EXPECT_EQ(tail_child->next->type, NAMUMARK_NODE_LINK);
  EXPECT_STREQ(reinterpret_cast<const char *>(tail_child->next->target.ptr), "문서");

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, WikiBlockClosesAfterObjectFitScaleDownTableRow) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"word-break: keep-all\"\n"
      "|| {{{object-fit=scale-down}}} ||{{{#!wiki style=\"display: flex\"\n"
      "{{{#!wiki style=\"border: 2px solid #888\"\n"
      "[[파일:example.png|width=120&height=120&object-fit=scale-down]]\n"
      "}}} {{{#!wiki style=\"margin-top: -15px\"\n"
      "{{{원본: 200x100\n"
      "매개변수: width=120&height=120&object-fit=scale-down\n"
      "}}} none과 contain 중 작은 쪽을 사용합니다.}}}}}} ||\n"
      "}}}\n"
      "모바일 문단\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *wiki = doc->first_child;
  ASSERT_EQ(wiki->type, NAMUMARK_NODE_WIKI_BLOCK);

  std::string wiki_content(reinterpret_cast<const char *>(wiki->content.ptr));
  EXPECT_EQ(wiki_content.find("모바일 문단"), std::string::npos);

  const namumark_node *tail = wiki->next;
  ASSERT_NE(tail, nullptr);
  ASSERT_EQ(tail->type, NAMUMARK_NODE_TEXT);
  EXPECT_STREQ(reinterpret_cast<const char *>(tail->content.ptr), "모바일 문단");

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, TrailingWikiCloseInsideTableRowDoesNotLeak) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"x\"\n"
      "|| A ||}}}||\n"
      "끝\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *wiki = doc->first_child;
  ASSERT_EQ(wiki->type, NAMUMARK_NODE_WIKI_BLOCK);
  std::string wiki_content(reinterpret_cast<const char *>(wiki->content.ptr));
  EXPECT_EQ(wiki_content.find("}}}"), std::string::npos);
  EXPECT_EQ(wiki_content.find("||}}}"), std::string::npos);

  const namumark_node *tail = wiki->next;
  ASSERT_NE(tail, nullptr);
  ASSERT_EQ(tail->type, NAMUMARK_NODE_TEXT);
  EXPECT_STREQ(reinterpret_cast<const char *>(tail->content.ptr), "끝");

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, NestedLinkWithFileTargetParsesAsSingleOuterLink) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input = "[[나무위키|[[파일:나무위키:로고2.png]]]]\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *text = doc->first_child;
  ASSERT_EQ(text->type, NAMUMARK_NODE_TEXT);
  ASSERT_NE(text->first_child, nullptr);

  const namumark_node *link = text->first_child;
  ASSERT_EQ(link->type, NAMUMARK_NODE_LINK);
  EXPECT_EQ(link->link_type, NAMUMARK_LINK_INTERNAL);
  EXPECT_STREQ(reinterpret_cast<const char *>(link->target.ptr), "나무위키");
  EXPECT_STREQ(reinterpret_cast<const char *>(link->args.ptr), "[[파일:나무위키:로고2.png]]");
  EXPECT_EQ(link->next, nullptr);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(ParserTest, AdvancedPreformattedClosesMidLineAndRendersFollowingText) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{||<width=50%><nopad> [[파일:example.png|width=100%]] ||<width=50%><nopad> [[파일:example.png|width=100%]] ||\n"
      "|| 설명 1 || 설명 2 ||}}}위의 방식보다는 아래 방식이 권장됩니다.\n"
      "{{{||<nopad> [[파일:example.png|width=100%]] ||<nopad> [[파일:example.png|width=100%]] ||\n"
      "||<width=50%> 설명 1 ||<width=50%> 설명 2 ||}}}[각주]\n"
      "== 동영상 ==\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);
  ASSERT_NE(doc->first_child, nullptr);

  const namumark_node *first_pre = doc->first_child;
  ASSERT_EQ(first_pre->type, NAMUMARK_NODE_PREFORMATTED);

  const namumark_node *after_first = first_pre->next;
  ASSERT_NE(after_first, nullptr);
  ASSERT_EQ(after_first->type, NAMUMARK_NODE_TEXT);
  EXPECT_NE(std::string(reinterpret_cast<const char *>(after_first->content.ptr)).find("위의 방식보다는 아래 방식"),
            std::string::npos);

  const namumark_node *second_pre = after_first->next;
  ASSERT_NE(second_pre, nullptr);
  ASSERT_EQ(second_pre->type, NAMUMARK_NODE_PREFORMATTED);

  const namumark_node *after_second = second_pre->next;
  ASSERT_NE(after_second, nullptr);
  ASSERT_EQ(after_second->type, NAMUMARK_NODE_TEXT);
  EXPECT_STREQ(reinterpret_cast<const char *>(after_second->content.ptr), "[각주]");

  const namumark_node *heading = after_second->next;
  ASSERT_NE(heading, nullptr);
  ASSERT_EQ(heading->type, NAMUMARK_NODE_HEADING);
  EXPECT_STREQ(reinterpret_cast<const char *>(heading->content.ptr), "동영상");

  namumark_node_free(doc);
  parser_free(parser);
}
