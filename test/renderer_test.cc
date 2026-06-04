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

TEST(RendererTest, FootnotesRenderReferencesAndMacroList) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "기본 [* 텍스트 1]\n"
      "이름 [*A 텍스트 2] 재참조 [*A]\n"
      "한글 [*예시 텍스트 3]\n"
      "[각주]\n";
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

  EXPECT_NE(html.find("<a class=\"nm-footnote-ref\" href=\"#fn-1\"><span id=\"rfn-1\"></span>[1]</a>"),
            std::string::npos);
  EXPECT_NE(html.find("<a class=\"nm-footnote-ref\" href=\"#fn-A\"><span id=\"rfn-2\"></span>[A]</a>"),
            std::string::npos);
  EXPECT_NE(html.find("<a class=\"nm-footnote-ref\" href=\"#fn-A\"><span id=\"rfn-3\"></span>[A]</a>"),
            std::string::npos);
  EXPECT_NE(html.find("<a class=\"nm-footnote-ref\" href=\"#fn-%ec%98%88%ec%8b%9c\"><span id=\"rfn-4\"></span>[예시]</a>"),
            std::string::npos);
  EXPECT_NE(html.find("<span class=\"nm-footnote\"><span id=\"fn-1\"></span><a href=\"#rfn-1\">[1]</a> 텍스트 1</span>"),
            std::string::npos);
  EXPECT_NE(html.find("<span class=\"nm-footnote\"><span id=\"fn-A\"></span>[A] <a href=\"#rfn-2\"><sup>2.1</sup></a> <a href=\"#rfn-3\"><sup>2.2</sup></a> 텍스트 2</span>"),
            std::string::npos);
  EXPECT_NE(html.find("<span class=\"nm-footnote\"><span id=\"fn-예시\"></span><a href=\"#rfn-4\">[예시]</a> 텍스트 3</span>"),
            std::string::npos);
  EXPECT_EQ(html.find("<p><div class=\"nm-footnotes\">"), std::string::npos);
  EXPECT_EQ(html.find("<span class=\"nm-macro\" data-name=\"각주\">"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, EscapedFootnoteMarkupInWikiBlockRendersAsExampleText) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"font-family: monospace\"\n"
      "\\[*{{{#red ○}}}텍스트 1]}}}\n";
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

  EXPECT_NE(html.find("<div class=\"nm-wiki-block\" style=\"font-family: monospace\">"),
            std::string::npos);
  EXPECT_NE(html.find("[*<span class=\"nm-advanced\" data-advanced=\"#red\" style=\"color:red;\">○</span>텍스트 1]"),
            std::string::npos);
  EXPECT_EQ(html.find("nm-footnote-ref"), std::string::npos);
  EXPECT_EQ(html.find("\\<a class=\"nm-footnote-ref\""), std::string::npos);

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
  EXPECT_EQ(html.find("<a href=\"나무위키\"><a href=\"파일:나무위키:로고2.png\">"),
            std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, MobileImageSectionPreBlocksDoNotLeakClosersOrSwallowHeading) {
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

  char *buf = nullptr;
  size_t len = 0;
  FILE *fp = open_memstream(&buf, &len);
  ASSERT_NE(fp, nullptr);
  EXPECT_TRUE(print_document_html(doc, fp));
  fclose(fp);

  std::string html(buf, len);
  free(buf);

  EXPECT_NE(html.find("위의 방식보다는 아래 방식이 권장됩니다."), std::string::npos);
  EXPECT_NE(html.find("<h2>동영상</h2>"), std::string::npos);
  EXPECT_EQ(html.find("|| 설명 1 || 설명 2 ||}}}위의 방식보다는"), std::string::npos);
  EXPECT_EQ(html.find("||<width=50%> 설명 1 ||<width=50%> 설명 2 ||}}}[각주]"),
            std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, NestedListRendersHierarchically) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki\n"
      " * 리스트 1\n"
      "  * 리스트 1.1\n"
      " * 리스트 2\n"
      "  * 리스트 2.1\n"
      "   * 리스트 2.1.1}}}\n";
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

  EXPECT_NE(html.find("<li>리스트 1<ul>"), std::string::npos);
  EXPECT_NE(html.find("<li>리스트 1.1</li></ul>"), std::string::npos);
  EXPECT_NE(html.find("<li>리스트 2.1<ul>"), std::string::npos);
  EXPECT_NE(html.find("<li>리스트 2.1.1</li></ul>"), std::string::npos);
  EXPECT_EQ(html.find("</ul>\n<ul>"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, WikiBlockListInTableCellPreservesIndentation) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "||<tablebgcolor=transparent><rowbgcolor=#00a495,#00a495><rowcolor=#fff> 입력 || 출력 ||\n"
      "||{{{#!wiki style=\"font-family: monospace\"\n"
      "{{{#red ○}}}* 리스트 1\n"
      "{{{#red ○○}}}* 리스트 1.1\n"
      "{{{#red ○}}}* 리스트 2\n"
      "{{{#red ○○}}}* 리스트 2.1\n"
      "{{{#red ○○○}}}* 리스트 2.1.1}}}\n"
      "||{{{#!wiki\n"
      " * 리스트 1\n"
      "  * 리스트 1.1\n"
      " * 리스트 2\n"
      "  * 리스트 2.1\n"
      "   * 리스트 2.1.1}}}\n"
      "||\n";
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
  EXPECT_NE(html.find("</div></td><td style=\"\"><div><div class=\"nm-wiki-block\"><ul>"),
            std::string::npos);
  EXPECT_NE(html.find("<li>리스트 1<ul>"), std::string::npos);
  EXPECT_NE(html.find("<li>리스트 1.1</li></ul>"), std::string::npos);
  EXPECT_NE(html.find("<li>리스트 2.1<ul>"), std::string::npos);
  EXPECT_NE(html.find("<li>리스트 2.1.1</li></ul>"), std::string::npos);
  EXPECT_EQ(html.find("{{{#!wiki"), std::string::npos);
  EXPECT_EQ(html.find("</ul>\n<ul>"), std::string::npos);

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

TEST(RendererTest, WikiBlockListContinuationTextStaysInListItem) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki\n"
      " * 리스트\n"
      " 내에서의 개행\n"
      " * 매크로를 이용한[br]리스트 내의 개행}}}\n"
      "{{{#!wiki\n"
      " A. list 1\n"
      "  A. list 1-1\n"
      "  \n"
      "  A. list 1-2\n"
      " \n"
      " A. list 2\n"
      " \n"
      " A. list 3}}}\n";
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

  EXPECT_NE(html.find("<li>리스트<br />내에서의 개행</li>"), std::string::npos);
  EXPECT_NE(html.find("<li>list 1-1<br /></li>"), std::string::npos);
  EXPECT_NE(html.find("<li>list 2<br /></li>"), std::string::npos);
  EXPECT_EQ(html.find("<p> 내에서의 개행</p>"), std::string::npos);
  EXPECT_EQ(html.find("</ol>\n<ol>\n<li>list 1-2"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, BlockquotesInTableCellsPreserveLinesAndLists) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "||<width=30%> 출력 ||\n"
      "||<|2><width=150px>\n"
      ">{{{#!wiki style=\"margin: 1em calc(1em + 25px) 1em 1em\"\n"
      "인용문}}}||\n"
      "|| 다음 ||\n"
      "||<|2>\n"
      ">{{{#!wiki style=\"margin: 1em calc(1em + 25px) 1em 1em\"\n"
      "문장1[br]문장2}}}||\n"
      "||<|2>\n"
      ">{{{#!wiki style=\"margin: 1em calc(1em + 25px) 1em 1em\"\n"
      "문장1[br][br]문장2}}}||\n"
      "||<|2><width=150px>\n"
      ">{{{#!wiki style=\"margin: 1em calc(1em + 25px) 1em 1em\"\n"
      " * 목록이 담긴 인용문\n"
      "  * 목록1\n"
      "  * 목록2}}}||\n";
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

  EXPECT_NE(html.find("<blockquote><div><div class=\"nm-wiki-block\" style=\"margin: 1em calc(1em + 25px) 1em 1em\">"),
            std::string::npos);
  EXPECT_NE(html.find("문장1<br />문장2"), std::string::npos);
  EXPECT_NE(html.find("문장1<br /><br />문장2"), std::string::npos);
  EXPECT_NE(html.find("<li>목록이 담긴 인용문<ul>"), std::string::npos);
  EXPECT_NE(html.find("<li>목록1</li>"), std::string::npos);
  EXPECT_NE(html.find("<li>목록2</li></ul>"), std::string::npos);
  EXPECT_EQ(html.find("&gt;{{{#!wiki"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, TableRowWithLiteralAndRenderedBlockquoteList) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "|| 리스트와의 결합 ||{{{> * 목록이 담긴 인용문\n"
      ">  * 목록1\n"
      ">  * 목록2}}}||<|2><width=150px>\n"
      ">{{{#!wiki style=\"margin: 1em calc(1em + 25px) 1em 1em\"\n"
      " * 목록이 담긴 인용문\n"
      "  * 목록1\n"
      "  * 목록2}}}||<|2>인용문 내에서도 리스트를 만들 수 있습니다. 매 줄마다 부등호-띄어쓰기-별표 후 내용을 쓰면 됩니다.[br][br]부등호와 별표 사이를 붙이면 정상 출력되지 않으며, 두 칸 이상 띄어 쓰면 띄어 쓴 만큼의 깊이에 해당하는 하위 리스트의 인용문 내 구현이 가능합니다. ||\n";
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

  EXPECT_NE(html.find("<pre><code>&gt; * 목록이 담긴 인용문\n&gt;  * 목록1\n&gt;  * 목록2</code></pre>"),
            std::string::npos);
  EXPECT_NE(html.find("<td rowspan=\"2\" style=\"width:150px;\"><div><blockquote><div><div class=\"nm-wiki-block\" style=\"margin: 1em calc(1em + 25px) 1em 1em\">"),
            std::string::npos);
  EXPECT_NE(html.find("<li>목록이 담긴 인용문<ul>"), std::string::npos);
  EXPECT_NE(html.find("<li>목록1</li>"), std::string::npos);
  EXPECT_NE(html.find("<li>목록2</li></ul>"), std::string::npos);
  EXPECT_EQ(html.find("{{{&gt; * 목록이 담긴 인용문"), std::string::npos);
  EXPECT_EQ(html.find("<blockquote><div><ul><li></li>"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, WikiBlockTableCellLiteralAndRenderedTableList) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"word-break: keep-all\"\n"
      "||<rowbgcolor=#00a495,#00a495><rowcolor=#fff><table bgcolor=transparent> 문법 || 출력 ||\n"
      "||{{{||엔터 키를 이용해 개행하면\n"
      "----\n"
      " * 표 안에도 리스트와 수평줄을 삽입할 수 있습니다.||}}}||{{{#!wiki\n"
      "||엔터 키를 이용해 개행하면\n"
      "----\n"
      " * 표 안에도 리스트와 수평줄을 삽입할 수 있습니다.||}}}||}}}\n";
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

  EXPECT_NE(html.find("<pre><code>||엔터 키를 이용해 개행하면\n----\n * 표 안에도 리스트와 수평줄을 삽입할 수 있습니다.||</code></pre>"),
            std::string::npos);
  EXPECT_NE(html.find("<div class=\"nm-wiki-block\"><table class=\"nm-table\""),
            std::string::npos);
  EXPECT_NE(html.find("엔터 키를 이용해 개행하면<hr><ul><li>표 안에도 리스트와 수평줄을 삽입할 수 있습니다.</li></ul>"),
            std::string::npos);
  EXPECT_EQ(html.find("표 안에도 리스트와 수평줄을 삽입할 수 있습니다.||}}}||{{{#!wiki"),
            std::string::npos);
  EXPECT_EQ(html.find("<ul><li></li>"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, WikiBlockTableListDocumentationCases) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki style=\"word-break: keep-all\"\n"
      "||<rowbgcolor=#00a495,#00a495><rowcolor=#fff><table bgcolor=transparent> 문법 || 출력 ||\n"
      "||{{{||\n"
      " * 한칸 표에서 아무것도\n"
      " * 입력하지 않음||}}}||{{{#!wiki\n"
      "||\n"
      " * 한칸 표에서 아무것도\n"
      " * 입력하지 않음||}}}||\n"
      "||{{{|| 여러 칸 ||\n"
      " * 여러 칸에서는\n"
      " * 오류가 생김||}}}||{{{#!wiki\n"
      "|| 여러 칸 ||\n"
      " * 여러 칸에서는\n"
      " * 오류가 생김||}}}||\n"
      "||{{{|| 여러 칸 ||{{{#!wiki\n"
      " * #!wiki 문법으로\n"
      " * 내용 있는 칸임을 알려줌}}}||}}}||{{{#!wiki\n"
      "|| 여러 칸 ||{{{#!wiki\n"
      " * #!wiki 문법으로\n"
      " * 내용 있는 칸임을 알려줌}}}||}}}||\n"
      "||{{{|| 여러 칸 ||<-1>\n"
      " * <테이블 및 셀 속성> 문법으로\n"
      " * 내용 있는 칸임을 알려줌||}}}||{{{#!wiki\n"
      "|| 여러 칸 ||<-1>\n"
      " * <테이블 및 셀 속성> 문법으로\n"
      " * 내용 있는 칸임을 알려줌||}}}||}}}\n"
      "표는 한 칸당 한 줄을 차지하게 작성할 수 있습니다.\n";
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

  EXPECT_NE(html.find("<pre><code>||\n * 한칸 표에서 아무것도\n * 입력하지 않음||</code></pre>"),
            std::string::npos);
  EXPECT_NE(html.find("<td style=\"\"><div><ul><li>한칸 표에서 아무것도</li>\n<li>입력하지 않음</li></ul></div></td>"),
            std::string::npos);
  EXPECT_NE(html.find("<pre><code>|| 여러 칸 ||{{{#!wiki\n * #!wiki 문법으로\n * 내용 있는 칸임을 알려줌}}}||</code></pre>"),
            std::string::npos);
  EXPECT_NE(html.find("<div class=\"nm-wiki-block\"><ul>\n<li>#!wiki 문법으로</li>\n<li>내용 있는 칸임을 알려줌</li></ul>"),
            std::string::npos);
  EXPECT_NE(html.find("<pre><code>|| 여러 칸 ||&lt;-1&gt;\n * &lt;테이블 및 셀 속성&gt; 문법으로\n * 내용 있는 칸임을 알려줌||</code></pre>"),
            std::string::npos);
  EXPECT_NE(html.find("<li>&lt;테이블 및 셀 속성&gt; 문법으로</li>\n<li>내용 있는 칸임을 알려줌</li>"),
            std::string::npos);
  EXPECT_NE(html.find("<p>표는 한 칸당 한 줄을 차지하게 작성할 수 있습니다.</p>"),
            std::string::npos);
  EXPECT_EQ(html.find("{{{||"), std::string::npos);
  EXPECT_EQ(html.find("||}}}||{{{#!wiki"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, OneCellPerLineTableDocumentationCase) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "||<colbgcolor=#00a495,#00a495><colcolor=#fff><table bgcolor=transparent> 문법 ||{{{+1 '''기본 문법'''}}}\n"
      "{{{|| 조지 워싱턴 || 존 애덤스 || 토머스 제퍼슨 ||\n"
      "|| 존 퀸시 애덤스 || 윌리엄 헨리 해리슨 || 존 타일러 ||}}}\n"
      "----\n"
      "{{{+1 '''한 칸마다 개행하는 문법'''}}}\n"
      "가운데 정렬이 필요하면 2가지 방법이 있습니다.\n"
      " 1. 셀 하나하나 {{{<:>}}} 옵션을 넣는 방법\n"
      " 1. 각 칸 내용 뒤쪽의 공백을 개행 후 {{{||}}} 앞에 띄어쓰기를 입력하는 방법\n"
      "{{{#!wiki style=\"float: left; margin-right: 10px\"\n"
      "{{{||<:>조지 워싱턴\n"
      "||<:>존 애덤스\n"
      "||<:>토머스 제퍼슨\n"
      "||\n"
      "||<:>존 퀸시 애덤스\n"
      "||<:>윌리엄 헨리 해리슨\n"
      "||<:>존 타일러\n"
      "||}}}}}}{{{#!wiki style=\"float: left\"\n"
      "{{{|| 조지 워싱턴\n"
      " || 존 애덤스\n"
      " || 토머스 제퍼슨\n"
      " ||\n"
      "|| 존 퀸시 애덤스\n"
      " || 윌리엄 헨리 해리슨\n"
      " || 존 타일러\n"
      " ||}}}}}} ||\n"
      "|| 출력 ||{{{#!wiki\n"
      "||<:>조지 워싱턴\n"
      "||<:>존 애덤스\n"
      "||<:>토머스 제퍼슨\n"
      "||\n"
      "||<:>존 퀸시 애덤스\n"
      "||<:>윌리엄 헨리 해리슨\n"
      "||<:>존 타일러\n"
      "||}}}||\n";
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

  EXPECT_NE(html.find("<table class=\"nm-table\" style=\"background-color:transparent;\">"),
            std::string::npos);
  EXPECT_NE(html.find("<td style=\"background-color:#00a495;color:#fff;\"><div>문법</div></td>"),
            std::string::npos);
  EXPECT_NE(html.find("<span class=\"nm-advanced\" data-advanced=\"+1\"><strong>기본 문법</strong></span>"),
            std::string::npos);
  EXPECT_NE(html.find("<ol><li>셀 하나하나 <code>&lt;:&gt;</code> 옵션을 넣는 방법</li>"),
            std::string::npos);
  EXPECT_NE(html.find("<div class=\"nm-wiki-block\" style=\"float: left; margin-right: 10px\"><pre><code>||&lt;:&gt;조지 워싱턴"),
            std::string::npos);
  EXPECT_NE(html.find("<div class=\"nm-wiki-block\" style=\"float: left\"><pre><code>|| 조지 워싱턴"),
            std::string::npos);
  EXPECT_NE(html.find("<td style=\"text-align:center;\"><div>조지 워싱턴</div></td><td style=\"text-align:center;\"><div>존 애덤스</div></td>"),
            std::string::npos);
  EXPECT_EQ(html.find("}}}{{{#!wiki style=&quot;float: left&quot;"), std::string::npos);
  EXPECT_EQ(html.find("<hr />\n<pre><code>+1"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, ClosedMultilineWikiTableRowDoesNotSwallowFollowingBlocks) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "||<tablebgcolor=transparent><rowbgcolor=#00a495,#00a495><rowcolor=#fff> 입력 || 출력 ||\n"
      "||{{{#!wiki\n"
      " * 잘못된 방식\n"
      "{{{#!wiki style=\"margin-top: -21px; margin-bottom: 1rem\"\n"
      "띄어쓰지 않으면}}}\n"
      " * 리스트가 이어지지 않음}}}||\n"
      "리스트를 유지하면서 개행을 넣고자 한다면 설명 문단입니다.\n"
      "||<tablebgcolor=transparent><rowbgcolor=#00a495,#00a495><rowcolor=#fff> 다음 입력 || 다음 출력 ||\n"
      "|| A || B ||\n";
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

  EXPECT_NE(html.find("</tbody></table>\n<p>리스트를 유지하면서 개행을 넣고자 한다면 설명 문단입니다.</p>\n<table"),
            std::string::npos);
  EXPECT_EQ(html.find("리스트를 유지하면서 개행을 넣고자 한다면 설명 문단입니다.</div></td>"),
            std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}
