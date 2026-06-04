#include <gtest/gtest.h>

#include <cstring>
#include <string>

extern "C" {
#include "lib/namumark.h"
}

TEST(ApiTest, RendersHtmlThroughPublicApi) {
  const char *input = "== 제목 ==\n본문 [[문서|링크]]\n";
  namumark_buffer output = {nullptr, 0};

  namumark_status status = namumark_render_html(input, strlen(input), &output);
  EXPECT_EQ(status, NAMUMARK_OK);
  ASSERT_NE(output.data, nullptr);

  std::string html(output.data, output.size);
  EXPECT_NE(html.find("<h2>제목</h2>"), std::string::npos);
  EXPECT_NE(html.find("<a href=\"문서\">링크</a>"), std::string::npos);

  namumark_buffer_free(&output);
  EXPECT_EQ(output.data, nullptr);
  EXPECT_EQ(output.size, 0u);
}

TEST(ApiTest, RendersAstJsonThroughPublicApi) {
  const char *input = "본문\n";
  namumark_buffer output = {nullptr, 0};

  namumark_status status = namumark_render_ast_json(input, strlen(input), &output);
  EXPECT_EQ(status, NAMUMARK_OK);
  ASSERT_NE(output.data, nullptr);

  std::string json(output.data, output.size);
  EXPECT_NE(json.find("\"type\": \"document\""), std::string::npos);
  EXPECT_NE(json.find("\"content\": \"본문\""), std::string::npos);

  namumark_buffer_free(&output);
}

TEST(ApiTest, RejectsInvalidArguments) {
  namumark_buffer output = {nullptr, 0};
  EXPECT_EQ(namumark_render_html(nullptr, 0, &output), NAMUMARK_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(namumark_render_html("", 0, nullptr), NAMUMARK_ERROR_INVALID_ARGUMENT);
  EXPECT_STREQ(namumark_version(), NAMUMARK_VERSION);
}
