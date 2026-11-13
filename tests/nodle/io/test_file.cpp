#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "nodle/io/file.hpp"

using nodle::io::SourceFile;

class SourceFileTest : public ::testing::Test {
protected:
  void SetUp() override {
    const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
    dir_ = std::filesystem::temp_directory_path() /
           (std::string("nodle-") + info->name());
    std::filesystem::remove_all(dir_);
    std::filesystem::create_directories(dir_);
  }

  void TearDown() override { std::filesystem::remove_all(dir_); }

  std::filesystem::path write(const std::string &name,
                              const std::string &bytes) {
    const auto p = dir_ / name;
    std::ofstream out(p, std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return p;
  }

  std::filesystem::path dir_;
};

TEST_F(SourceFileTest, ConstructorStoresPathAndContent) {
  SourceFile f("a.nodle", "let x = 1");
  EXPECT_EQ(f.path(), std::filesystem::path("a.nodle"));
  EXPECT_EQ(f.content(), "let x = 1");
}

TEST_F(SourceFileTest, TextReturnsSubstring) {
  SourceFile f("a.nodle", "hello world");
  EXPECT_EQ(f.text(6, 5), "world");
  EXPECT_EQ(f.text(0, 5), "hello");
}

TEST_F(SourceFileTest, TextClampsLengthPastEnd) {
  SourceFile f("a.nodle", "abc");
  EXPECT_EQ(f.text(1, 100), "bc");
}

TEST_F(SourceFileTest, TextThrowsWhenOffsetPastEnd) {
  SourceFile f("a.nodle", "abc");
  EXPECT_THROW(f.text(10, 1), std::out_of_range);
}

TEST_F(SourceFileTest, LoadReadsFileContents) {
  const auto p = write("a.nodle", "line1\nline2\n");
  const SourceFile f = SourceFile::load(p);
  EXPECT_EQ(f.path(), p);
  EXPECT_EQ(f.content(), "line1\nline2\n");
}

TEST_F(SourceFileTest, LoadPreservesBytesExactly) {
  const std::string bytes("a\r\nb\0c", 6); // CRLF and an embedded NUL
  const SourceFile f = SourceFile::load(write("bin.nodle", bytes));
  EXPECT_EQ(f.content(), std::string_view(bytes));
}

TEST_F(SourceFileTest, LoadEmptyFile) {
  const SourceFile f = SourceFile::load(write("empty.nodle", ""));
  EXPECT_TRUE(f.content().empty());
}

TEST_F(SourceFileTest, LoadMissingFileThrows) {
  EXPECT_THROW(SourceFile::load(dir_ / "does-not-exist.nodle"),
               std::runtime_error);
}

TEST_F(SourceFileTest, LoadDirectoryThrows) {
  EXPECT_THROW(SourceFile::load(dir_), std::runtime_error);
}

TEST_F(SourceFileTest, LoadErrorMentionsPath) {
  const auto missing = dir_ / "nope.nodle";
  try {
    SourceFile::load(missing);
    FAIL() << "expected an exception";
  } catch (const std::runtime_error &e) {
    EXPECT_NE(std::string(e.what()).find("nope.nodle"), std::string::npos);
  }
}