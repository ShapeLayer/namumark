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

TEST(RendererTest, AstIncludesDocumentCategories) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input = "[[분류:문법 도움말]]\n[[분류:나무위키#blur]]\n본문\n";
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

  EXPECT_NE(ast.find("\"categories\": [\"문법 도움말\", \"나무위키#blur\"]"),
            std::string::npos);
  EXPECT_EQ(ast.find("\"type\": \"category\""), std::string::npos);

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

  EXPECT_NE(html.find("<a id=\"object-fit\"></a>"), std::string::npos);
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

TEST(RendererTest, StyleAdvancedAndWikiAttributesRender) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!style\n"
      ".blur {display: inline; text-shadow: 0 0 6px #0275d8; color: transparent}\n"
      ".blur:hover {text-shadow: 0 0 0px #0275d8}\n"
      "}}}\n"
      "{{{#!wiki style=\"background-color: #fff; border: 1px solid #dfe1e2; border-radius: 4px; font-size: .9rem; margin: 0 0 1em; padding: .2rem .5rem\" dark-style=\"background-color: #1c1d1f; border-color: #5c5c5c\"\n"
      "분류: [[:분류:분류|분류]]{{{#!wiki style=\"border-left: 1px solid #888; display: inline-block; height: .8rem; margin: 0 .4rem -.1rem\"\n"
      "}}}[[:분류:나무위키|{{{#!wiki class=\"blur\"\n"
      "나무위키}}}]]}}}\n";
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

  EXPECT_NE(html.find("<style>.blur {display: inline; text-shadow: 0 0 6px #0275d8; color: transparent}"), std::string::npos);
  EXPECT_NE(html.find(".blur:hover {text-shadow: 0 0 0px #0275d8}</style>"), std::string::npos);
  EXPECT_EQ(html.find("<p><style>"), std::string::npos);
  EXPECT_NE(html.find("<div class=\"nm-wiki-block\" style=\"background-color: #fff; border: 1px solid #dfe1e2; border-radius: 4px; font-size: .9rem; margin: 0 0 1em; padding: .2rem .5rem\" data-dark-style=\"background-color: #1c1d1f; border-color: #5c5c5c\""), std::string::npos);
  EXPECT_NE(html.find("<div class=\"nm-wiki-block\" style=\"border-left: 1px solid #888; display: inline-block; height: .8rem; margin: 0 .4rem -.1rem\""), std::string::npos);
  EXPECT_NE(html.find("<div class=\"nm-wiki-block blur\">"), std::string::npos);
  EXPECT_NE(html.find("분류: <a href=\":분류:분류\">분류</a><div class=\"nm-wiki-block\""), std::string::npos);
  EXPECT_NE(html.find("<a href=\":분류:나무위키\"><div class=\"nm-wiki-block blur\">나무위키</div></a>"), std::string::npos);
  EXPECT_EQ(html.find("<p>분류:"), std::string::npos);
  EXPECT_EQ(html.find("<p></p>"), std::string::npos);
  EXPECT_EQ(html.find("{{{#!wiki"), std::string::npos);
  EXPECT_EQ(html.find("{{{#!style"), std::string::npos);

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

TEST(RendererTest, FootnotesContainNestedLinksAndMacros) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "색상 {{{#1c1d1f}}}[* [[위키/스킨|기본 스킨]] espejo의 배경색] 다음\n"
      "파일 {{{#000}}}[* [[:파일:nogray.svg|\"검은화면으로\"]] 설정을 켰을 때의 배경색] 다음\n"
      "텍스트[*B 기재된 색상은 기본값을 의미하며, [[#텍스트 색상|색상 문법]]을 이용해 변경 가능.]\n"
      "표[*B] 각주[*B]\n"
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

  EXPECT_NE(html.find("<span class=\"nm-footnote\"><span id=\"fn-1\"></span><a href=\"#rfn-1\">[1]</a> <a href=\"위키/스킨\">기본 스킨</a> espejo의 배경색</span>"), std::string::npos);
  EXPECT_NE(html.find("<span class=\"nm-footnote\"><span id=\"fn-2\"></span><a href=\"#rfn-2\">[2]</a> <a href=\":파일:nogray.svg\">&quot;검은화면으로&quot;</a> 설정을 켰을 때의 배경색</span>"), std::string::npos);
  EXPECT_NE(html.find("<span class=\"nm-footnote\"><span id=\"fn-B\"></span>[B] <a href=\"#rfn-3\"><sup>3.1</sup></a> <a href=\"#rfn-4\"><sup>3.2</sup></a> <a href=\"#rfn-5\"><sup>3.3</sup></a> 기재된 색상은 기본값을 의미하며, <a href=\"#텍스트 색상\">색상 문법</a>을 이용해 변경 가능.</span>"), std::string::npos);
  EXPECT_EQ(html.find("위키/스킨|기본 스킨"), std::string::npos);
  EXPECT_EQ(html.find("파일:nogray.svg|"), std::string::npos);
  EXPECT_EQ(html.find("] 다음"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, FootnoteMacroDoesNotNestBlockInParagraph) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "본문[* 각주]\n"
      "[각주][include(틀:문서 가져옴)]\n";
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

  EXPECT_EQ(html.find("<p><div class=\"nm-footnotes\">"), std::string::npos);
  EXPECT_NE(html.find("<div class=\"nm-footnotes\">\n<span class=\"nm-footnote\""), std::string::npos);
  EXPECT_NE(html.find("<p><span class=\"nm-macro\" data-name=\"include\">include(틀:문서 가져옴)</span></p>"), std::string::npos);

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

TEST(RendererTest, TableCellColspanAndRowspanSyntax) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "{{{#!wiki\n"
      "|| 한 || 칸 || 짜 || 리 ||\n"
      "|||| 두칸 |||| 짜리 ||\n"
      "|||||| 세칸짜 || 리 ||\n"
      "|||||||| 네칸짜리 ||}}}\n"
      "{{{#!wiki\n"
      "|| 한 || 칸 || 짜 || 리 ||\n"
      "||<-2> 두칸 ||<-2> 짜리 ||\n"
      "||<-3> 세칸짜 || 리 ||\n"
      "||<-4> 네칸짜리 ||}}}\n"
      "{{{#!wiki style=\"\"\n"
      "||<|4> 네줄 ||<|3> 세줄 ||<|2> 두줄 ||<|1> 한줄 ||\n"
      "|| 여백1 ||\n"
      "|| 여백2 || 여백3 ||\n"
      "|| 여백4 || 여백5 || 여백6 ||}}}\n"
      "{{{#!wiki style=\"\"\n"
      "|| 여백 || 여백 || 여백 || 여백 ||\n"
      "|| 여백 ||<-3><|2> 3 곱하기 2 ||\n"
      "|| 여백 ||}}}\n"
      "{{{#!wiki style=\"\"\n"
      "|| 여백 || 여백 || 여백 || 여백 ||\n"
      "|| 여백 ||||||<|2> 3 곱하기 2 ||\n"
      "|| 여백 ||}}}\n"
      "{{{#!wiki\n"
      "||<table bordercolor=red> 테이 || 블이 || 몇칸 || 이든 ||\n"
      "||<-4><height=60> 한 번에 다 합쳐집니다. ||}}}\n";
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

  EXPECT_NE(html.find("<td colspan=\"2\" style=\"\"><div>두칸</div></td><td colspan=\"2\" style=\"\"><div>짜리</div></td>"),
            std::string::npos);
  EXPECT_NE(html.find("<td colspan=\"3\" style=\"\"><div>세칸짜</div></td><td style=\"\"><div>리</div></td>"),
            std::string::npos);
  EXPECT_NE(html.find("<td colspan=\"4\" style=\"\"><div>네칸짜리</div></td>"),
            std::string::npos);
  EXPECT_NE(html.find("<td rowspan=\"4\" style=\"\"><div>네줄</div></td><td rowspan=\"3\" style=\"\"><div>세줄</div></td><td rowspan=\"2\" style=\"\"><div>두줄</div></td>"),
            std::string::npos);
  EXPECT_NE(html.find("<td colspan=\"3\" rowspan=\"2\" style=\"\"><div>3 곱하기 2</div></td>"),
            std::string::npos);
  EXPECT_NE(html.find("<table class=\"nm-table\" style=\"border:2px solid red;\">"),
            std::string::npos);
  EXPECT_NE(html.find("<td colspan=\"4\" style=\"height:60px;\"><div>한 번에 다 합쳐집니다.</div></td>"),
            std::string::npos);
  EXPECT_EQ(html.find("style=\"&quot;&quot;\""), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, RowspanTableCloseLineDoesNotSwallowNextHeading) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "셀을 복잡하게 합칠 경우에는 줄 바꿈에 특히 유의하십시오. 입력할 셀이 없는 줄에는 {{{||||}}}를 입력합니다.\n"
      "||<tablebgcolor=transparent><rowbgcolor=#00a495,#00a495><rowcolor=#fff><table bordercolor=gray><width=200px> 대상 ||<width=300px> 입력 ||<width=200px> 출력 ||\n"
      "||{{{|| 세줄1 || 두줄1 ||\n"
      "|| 세줄1 || 두줄1 ||\n"
      "|| 세줄1 || 두줄2 ||\n"
      "|| 세줄2 || 두줄2 ||\n"
      "|| 세줄2 || 두줄3 ||\n"
      "|| 세줄2 || 두줄3 ||}}}\n"
      "||{{{\n"
      "||<|3><height=60> 세줄1 ||<|2><height=40> 두줄1 ||\n"
      "||||\n"
      "||<|2><height=40> 두줄2 ||\n"
      "||<|3><height=60> 세줄2 ||\n"
      "||<|2><height=40> 두줄3 ||\n"
      "||||\n"
      "}}}||{{{#!wiki style=\"\"\n"
      "||<|3><height=60> 세줄1 ||<|2><height=40> 두줄1 ||\n"
      "||||\n"
      "||<|2><height=40> 두줄2 ||\n"
      "||<|3><height=60> 세줄2 ||\n"
      "||<|2><height=40> 두줄3 ||\n"
      "||||}}}\n"
      "||\n"
      "\n"
      "==== 가로세로 합치기 ====\n"
      "셀을 가로와 세로로 모두 합칠 수 있습니다. {{{<-수>}}}와 {{{<|수>}}} 두 가지를 같이 입력합니다.\n"
      "||<tablebgcolor=transparent><rowbgcolor=#00a495,#00a495><rowcolor=#fff><table bordercolor=gray><width=33%> 입력 1 ||<width=33%> 입력 2 ||<width=33%> 출력 ||\n"
      "||{{{|| 여백 || 여백 || 여백 || 여백 ||\n"
      "|| 여백 ||<-3><|2> 3 곱하기 2 ||\n"
      "|| 여백 ||\n"
      "}}}||{{{|| 여백 || 여백 || 여백 || 여백 ||\n"
      "|| 여백 ||||||<|2> 3 곱하기 2 ||\n"
      "|| 여백 ||\n"
      "}}}||{{{#!wiki style=\"\"\n"
      "|| 여백 || 여백 || 여백 || 여백 ||\n"
      "|| 여백 ||<-3><|2> 3 곱하기 2 ||\n"
      "|| 여백 ||}}}||\n";
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

  EXPECT_NE(html.find("</tbody></table>\n<h4>가로세로 합치기</h4>"), std::string::npos);
  EXPECT_NE(html.find("<tr></tr>\n<tr><td rowspan=\"2\" style=\"height:40px;\"><div>두줄2</div></td></tr>"),
            std::string::npos);
  EXPECT_NE(html.find("<td colspan=\"3\" rowspan=\"2\" style=\"\"><div>3 곱하기 2</div></td>"),
            std::string::npos);
  EXPECT_EQ(html.find("<td style=\"\"><div>==== 가로세로 합치기 ===="), std::string::npos);
  EXPECT_EQ(html.find("</td><td style=\"width:33%;background-color:#00a495;color:#fff;\"><div>입력 1"),
            std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, IndentedTablesRemainInsideListItems) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      " * '''셀 텍스트 가로정렬'''\n"
      "   * {{{<(>}}} 또는 셀에서 ||(텍스트) ||: 텍스트 왼쪽 정렬 (기본값)\n"
      "   * {{{<:>}}} 또는 셀에서 || (텍스트) ||: 텍스트 가운데 정렬 \n"
      "   * {{{<)>}}} 또는 셀에서 || (텍스트)||: 텍스트 오른쪽 정렬\n"
      "   ||<tablebgcolor=transparent>{{{||<(>텍스트[br]왼쪽[br]정렬||}}} || {{{||<:>텍스트[br]가운데[br]정렬||}}} || {{{||<)>텍스트[br]오른쪽[br]정렬||}}}||\n"
      "   ||{{{||텍스트[br]왼쪽[br]정렬 ||}}} || {{{|| 텍스트[br]가운데[br]정렬 ||}}} || {{{|| 텍스트[br]오른쪽[br]정렬||}}}||\n"
      "   ||<(>텍스트[br]왼쪽[br]정렬||<:>텍스트[br]가운데[br]정렬||<)>텍스트[br]오른쪽[br]정렬||\n"
      " * '''셀 텍스트 세로 정렬'''\n"
      "  * {{{<^|숫자>}}}: 텍스트 수직 위 정렬\n"
      "  * {{{<|숫자>}}}: 텍스트 수직 가운데 정렬 (기본값)\n"
      "  * {{{<v|숫자>}}}: 텍스트 수직 아래 정렬\n"
      "   숫자는 해당 셀에 세로로 합쳐진 셀의 개수로, 세로로 합쳐지지 않은 셀에 적용하려면 숫자에 '1'을 넣으면 됩니다.\n"
      "   ||<tablebgcolor=transparent>{{{||<^|1> 수직 위 정렬 ||}}}||{{{||<|1> 수직 가운데 정렬 ||}}}||{{{||<v|1> 수직 아래 정렬 ||}}}||\n"
      "   ||<height=100><^|1> 수직 위 정렬 ||<|1> 수직 가운데 정렬 ||<v|1> 수직 아래 정렬 ||\n";
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

  EXPECT_NE(html.find("텍스트 오른쪽 정렬<br /><table class=\"nm-table\" style=\"background-color:transparent;\">"),
            std::string::npos);
  EXPECT_NE(html.find("<td style=\"text-align:left;\"><div>텍스트<br />왼쪽<br />정렬</div></td>"),
            std::string::npos);
  EXPECT_NE(html.find("<td style=\"height:100px;vertical-align:top;\"><div>수직 위 정렬</div></td>"),
            std::string::npos);
  EXPECT_NE(html.find("<td style=\"vertical-align:bottom;\"><div>수직 아래 정렬</div></td>"),
            std::string::npos);
  EXPECT_EQ(html.find("</ul>\n<table class=\"nm-table\" style=\"background-color:transparent;\">"),
            std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, TheadSortableTableStaysInsideListItem) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      " * '''표 머리글 행 설정, 정렬''': {{{<thead>, <sortable>}}}[anchor(thead)][anchor(sortable)]\n"
      " {{{<thead>}}}는 해당 행을 표의 머리글 행으로 설정합니다. 머리글 행으로 설정한 행의 텍스트는 기본적으로 '''굵게''' 표시됩니다. {{{<sortable>}}}은 표의 머리글 행 내에 있는 셀에 기본/오름차순/내림차순 정렬 버튼을 추가합니다.\n"
      " {{{#!wiki style=\"word-break: normal\"\n"
      "||<tablebgcolor=transparent><table bordercolor=gray><width=4%> '''입력''' ||<width=98%><(>{{{#!wiki style=\"font-family: monospace\"\n"
      "||<thead> 머 ||<sortable> 리 ||<sortable> 글 ||\n"
      "|| 행1 || B || 나 ||\n"
      "|| 행2 || A || 다 ||\n"
      "|| 행3 || C || 가 ||}}}||\n"
      "|| '''출력''' ||{{{#!wiki style=\"width: max-content; margin: auto\"\n"
      "||<thead> 머 ||<sortable> 리 ||<sortable> 글 ||\n"
      "|| 행1 || B || 나 ||\n"
      "|| 행2 || A || 다 ||\n"
      "|| 행3 || C || 가 ||}}}||}}}\n";
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

  EXPECT_NE(html.find("<a id=\"thead\"></a><a id=\"sortable\"></a><br /><code>&lt;thead&gt;</code>는"),
            std::string::npos);
  EXPECT_NE(html.find("<li><strong>표 머리글 행 설정, 정렬</strong>"), std::string::npos);
  EXPECT_NE(html.find("<br /><div class=\"nm-wiki-block\" style=\"word-break: normal\">"),
            std::string::npos);
  EXPECT_NE(html.find("<thead><tr><th style=\"\"><div>머</div></th><th class=\"nm-sortable\""),
            std::string::npos);
  EXPECT_NE(html.find("</thead><tbody>\n<tr><td style=\"\"><div>행1</div></td>"),
            std::string::npos);
  EXPECT_EQ(html.find("</li></ul>\n<div class=\"nm-wiki-block\" style=\"word-break: normal\">"),
            std::string::npos);
  EXPECT_EQ(html.find("<span class=\"nm-macro\" data-name=\"anchor\">"), std::string::npos);
  EXPECT_EQ(html.find("<tr><thead>"), std::string::npos);
  EXPECT_EQ(html.find("<tbody>\n</tbody></table>"), std::string::npos);

  namumark_node_free(doc);
  parser_free(parser);
}

TEST(RendererTest, FoldingAdvancedRendersDetailsInTables) {
  namumark_parser *parser = parser_new();
  ASSERT_NE(parser, nullptr);

  const char *input =
      "||<tablebgcolor=transparent><tablewidth=550><colcolor=#fff><colbgcolor=#00a495,#00a495> 예시 ||{{{{{{#!folding [ 펼치기 · 접기 ]\n"
      "내용\n"
      "'''내용'''\n"
      "__내용__}}}}}}||\n"
      "|| 결과 ||{{{#!folding [ 펼치기 · 접기 ]\n"
      "내용\n"
      "'''내용'''\n"
      "__내용__}}}||\n"
      "|| 결과 ||{{{#!folding '''[ 펼치기 · 접기 ]'''\n"
      "접기 안내 문구에는 위키문법 적용 불가}}}||{{{#!folding {{{#red [ 펼치기 · 접기 ]}}}\n"
      "접기 안내 문구에는 위키문법 적용 불가}}}||\n"
      "|| 결과 ||'''{{{#!folding [ 펼치기 · 접기 ]\n"
      "접기 문법 자체에는 위키문법 적용 가능}}}'''||{{{#red {{{#!folding [ 펼치기 · 접기 ]\n"
      "접기 문법 자체에는 위키문법 적용 가능}}}}}}||\n";
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

  EXPECT_NE(html.find("<pre><code>{{{#!folding [ 펼치기 · 접기 ]\n내용\n&#39;&#39;&#39;내용&#39;&#39;&#39;\n__내용__}}}</code></pre>"),
            std::string::npos);
  EXPECT_NE(html.find("<details class=\"nm-folding\"><summary>[ 펼치기 · 접기 ]</summary><div>내용<br /><strong>내용</strong><br /><u>내용</u></div></details>"),
            std::string::npos);
  EXPECT_NE(html.find("<summary>&#39;&#39;&#39;[ 펼치기 · 접기 ]&#39;&#39;&#39;</summary><div>접기 안내 문구에는 위키문법 적용 불가</div>"),
            std::string::npos);
  EXPECT_NE(html.find("<summary>{{{#red [ 펼치기 · 접기 ]}}}</summary><div>접기 안내 문구에는 위키문법 적용 불가</div>"),
            std::string::npos);
  EXPECT_NE(html.find("<strong><details class=\"nm-folding\"><summary>[ 펼치기 · 접기 ]</summary><div>접기 문법 자체에는 위키문법 적용 가능</div></details></strong>"),
            std::string::npos);
  EXPECT_NE(html.find("<span class=\"nm-advanced\" data-advanced=\"#red\" style=\"color:red;\"><details class=\"nm-folding\"><summary>[ 펼치기 · 접기 ]</summary><div>접기 문법 자체에는 위키문법 적용 가능</div></details></span>"),
            std::string::npos);
  EXPECT_EQ(html.find("{{{#!folding [ 펼치기 · 접기 ]<br />내용"), std::string::npos);

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
