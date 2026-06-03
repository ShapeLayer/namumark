#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include "lib/node.h"
#include "lib/parser.h"
#include "lib/renderer.h"
}

TEST(RendererTest, HtmlIsProduced) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input = "== 제목 ==\n본문 [[문서|링크]]\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);

  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<article class=\"namumark\">"), std::string::npos);
  EXPECT_NE(html.find("<h2>"), std::string::npos);
  EXPECT_NE(html.find("<a href=\"문서\">링크</a>"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, MultiLineTableRendersAsTable) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"word-break: keep-all\"\n"
      "||<tablebgcolor=transparent><rowbgcolor=#00a495,#00a495><rowcolor=#fff><width=30%> 입력 ||<width=25%> 출력 ||<width=45%> 비고 ||\n"
      "|| {{{'''굵게'''}}} || '''굵게 ''' ||설명 ||\n"
      "|| {{{''기울임''}}} || ''기울임'' ||설명 ||\n"
      "}}}\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);

  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<table class=\"nm-table\""), std::string::npos);
  EXPECT_NE(html.find("<td"), std::string::npos);
  EXPECT_NE(html.find("<strong>굵게 </strong>"), std::string::npos);
  EXPECT_NE(html.find("<em>기울임</em>"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, AstIsProduced) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input = "#redirect 대상\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);

  EXPECT_TRUE(print_document_ast(doc, fp));
  fclose(fp);

  std::string ast(buf, len);
  free(buf);

  EXPECT_NE(ast.find("\"type\": \"document\""), std::string::npos);
  EXPECT_NE(ast.find("\"type\": \"redirect\""), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, WikiBlockRendersNestedBlocks) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"word-break: keep-all\"\n"
      "||<rowbgcolor=#00a495><rowcolor=#fff> 헤더 ||\n"
      "|| 내용 ||\n"
      "}}}\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);

  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<div class=\"nm-wiki-block\" style=\"word-break: keep-all\">"), std::string::npos);
  EXPECT_NE(html.find("<table class=\"nm-table\""), std::string::npos);
  EXPECT_NE(html.find("헤더"), std::string::npos);
  EXPECT_NE(html.find("내용"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, NestedWikiBlocksRenderWithStyles) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"outer-css\"\n"
      "{{{#!wiki style=\"inner-css\"\n"
      "== 제목 ==\n"
      "}}}\n"
      "}}}\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);

  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<div class=\"nm-wiki-block\" style=\"outer-css\">"), std::string::npos);
  EXPECT_NE(html.find("<div class=\"nm-wiki-block\" style=\"inner-css\">"), std::string::npos);
  EXPECT_NE(html.find("<h2>제목</h2>"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, TableCellRendersNestedAdvancedLiteral) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"word-break: keep-all\"\n"
      "|| {{{{{{+1 +1단계}}}}}} || {{{+1 +1단계}}} ||\n"
      "}}}\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);

  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<code>{{{+1 +1단계}}}</code>"), std::string::npos);
  EXPECT_NE(html.find("data-advanced=\"+1\">+1단계</span>"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, InlineWikiAdvancedCreatesChildBlocks) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "|| {{{#!wiki style=\"min-width: 200px\"\n"
      "헥스 코드와 CSS 색상명 모두 대소문자 구별없이 입력 가능합니다.\n"
      "}}} ||\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);
  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<div class=\"nm-wiki-block\" style=\"min-width: 200px\">"), std::string::npos);
  EXPECT_NE(html.find("헥스 코드와 CSS 색상명 모두 대소문자 구별없이 입력 가능합니다."), std::string::npos);
  EXPECT_EQ(html.find("{{{#!wiki style=\"min-width: 200px\""), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, UnderlineAndColorNestingOrderIsPreserved) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "|| {{{{{{#red __밑줄 포함__}}}}}} || {{{#red __밑줄 포함__}}} ||\n"
      "|| {{{__{{{#red 밑줄 제외}}}__}}} || __{{{#red 밑줄 제외}}}__ ||\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);
  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<span class=\"nm-advanced\" data-advanced=\"#red\" style=\"color:red;\"><u>밑줄 포함</u></span>"), std::string::npos);
  EXPECT_NE(html.find("<u><span class=\"nm-advanced\" data-advanced=\"#red\" style=\"color:red;\">밑줄 제외</span></u>"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, ColorAdvancedSetsStyleAndDarkStyle) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#ff0000 빨강}}}\n"
      "{{{#f00 축약}}}\n"
      "{{{#red 별칭}}}\n"
      "{{{#888,#ff0 다크텍스트}}}\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);
  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("style=\"color:#ff0000;\""), std::string::npos);
  EXPECT_NE(html.find("style=\"color:#f00;\""), std::string::npos);
  EXPECT_NE(html.find("style=\"color:red;\""), std::string::npos);
  EXPECT_NE(html.find("data-dark-style=\"color:#ff0;\""), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, ExternalLinkLabelRendersInlineAdvancedHtml) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "[[https://www.google.com/|{{{#!html &#8203}}}https://{{{#!html &#8203}}}www{{{#!html &#8203}}}.{{{#!html &#8203}}}google{{{#!html &#8203}}}.{{{#!html &#8203}}}com{{{#!html &#8203}}}/]]\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);
  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<a href=\"https://www.google.com/\">"), std::string::npos);
  EXPECT_NE(html.find("&#8203https://&#8203www&#8203.&#8203google&#8203.&#8203com&#8203/"), std::string::npos);
  EXPECT_EQ(html.find("{{{#!html"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, BareTripleBraceRendersPreBlockInTableCell) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "|| {{{object-fit=scale-down}}} ||{{{#!wiki style=\"display: flex\"\n"
      "{{{#!wiki style=\"margin-top: -15px\"\n"
      "{{{\n"
      "원본: 200x100\n"
      "매개변수: width=120&height=120&object-fit=scale-down\n"
      "}}} none과 contain 중 크기가 작은 쪽으로 선택됩니다.\n"
      "}}}}}} ||\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);
  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<pre><code>원본: 200x100"), std::string::npos);
  EXPECT_NE(html.find("매개변수: width=120&amp;height=120&amp;object-fit=scale-down"), std::string::npos);
  EXPECT_NE(html.find("none과 contain 중 크기가 작은 쪽으로 선택됩니다."), std::string::npos);
  EXPECT_EQ(html.find("{{{#!wiki"), std::string::npos);
  EXPECT_EQ(html.find("}}}"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, InlineWikiAdvancedAfterPrefixTextInTableCell) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "|| {{{object-fit=fill}}} ||[anchor(object-fit)]{{{#!wiki style=\"display: flex\"\n"
      "{{{#!wiki style=\"margin-top: -15px\"\n"
      "{{{원본: 200x100\n"
      "매개변수: width=120&height=120&object-fit=fill\n"
      "}}} 설명 텍스트입니다.}}}}}} ||\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);
  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<span class=\"nm-macro\" data-name=\"anchor\">anchor(object-fit)</span>"),
            std::string::npos);
  EXPECT_NE(html.find("<div class=\"nm-wiki-block\" style=\"display: flex\""), std::string::npos);
  EXPECT_EQ(html.find("{{{#!wiki"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, InlineDisplayWikiAdvancedRendersAsSpan) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "각주 번호({{{#!wiki style=\"display: inline; color: #0275d8; font-size: .8em\"\n"
      "[1]}}})를 클릭\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);
  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<span class=\"nm-wiki-block\" style=\"display: inline; color: #0275d8; font-size: .8em\">"),
            std::string::npos);
  EXPECT_NE(html.find("<span class=\"nm-macro\" data-name=\"1\">1</span>"), std::string::npos);
  EXPECT_EQ(html.find("<div class=\"nm-wiki-block\" style=\"display: inline"), std::string::npos);
  EXPECT_EQ(html.find("{{{#!wiki"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, NestedFileLinkLabelRendersWithoutBrokenAnchor) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input = "[[나무위키|[[파일:나무위키:로고2.png|width=25]]]]\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);
  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<a href=\"나무위키\">[[파일:나무위키:로고2.png|width=25]]</a>"),
            std::string::npos);
  EXPECT_EQ(html.find("<a href=\"나무위키\">[[파일:나무위키:로고2.png|width=25</a>]]"),
            std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, TableRowDoesNotAddTrailingEmptyCell) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "||<tablebgcolor=transparent><rowbgcolor=#00a495,#00a495><rowcolor=#fff><width=60> 유형 || 기본 문단 || 접힌 문단 ||\n"
      "|| 1단계 || {{{= 문단 1 =}}} || {{{=# 문단 1 #=}}} ||\n";
  parser_feed(parser, reinterpret_cast<const unsigned char *>(input), strlen(input));

  namumark_node *doc = parser_finish(parser);
  ASSERT_NE(doc, nullptr);

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);
  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("<td style=\"width:60;background-color:#00a495;color:#fff;\"><div>유형</div></td><td"), std::string::npos);
  EXPECT_EQ(html.find("<td style=\"background-color:#00a495;color:#fff;\"><div></div></td>"), std::string::npos);
  EXPECT_EQ(html.find("<td style=\"\"><div></div></td></tr>"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}
