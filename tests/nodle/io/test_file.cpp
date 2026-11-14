#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <limits>
#include <vector>

#include "nodle/io/file.hpp"

using nodle::io::LineColumn;
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
  SourceFile::Ptr f = SourceFile::from_string("a.nodle", "let x = 1");
  EXPECT_EQ(f->path(), std::filesystem::path("a.nodle"));
  EXPECT_EQ(f->content(), "let x = 1");
}

TEST_F(SourceFileTest, TextReturnsSubstring) {
  SourceFile::Ptr f = SourceFile::from_string("a.nodle", "hello world");
  EXPECT_EQ(f->text(6, 5), "world");
  EXPECT_EQ(f->text(0, 5), "hello");
}

TEST_F(SourceFileTest, TextClampsLengthPastEnd) {
  SourceFile::Ptr f = SourceFile::from_string("a.nodle", "abc");
  EXPECT_EQ(f->text(1, 100), "bc");
}

TEST_F(SourceFileTest, TextThrowsWhenOffsetPastEnd) {
  SourceFile::Ptr f = SourceFile::from_string("a.nodle", "abc");
  EXPECT_THROW(f->text(10, 1), std::out_of_range);
}

TEST_F(SourceFileTest, LoadReadsFileContents) {
  const auto p = write("a.nodle", "line1\nline2\n");
  const SourceFile::Ptr f = SourceFile::load(p);
  EXPECT_EQ(f->path(), p);
  EXPECT_EQ(f->content(), "line1\nline2\n");
}

TEST_F(SourceFileTest, LoadPreservesBytesExactly) {
  const std::string bytes("a\r\nb\0c", 6); // CRLF and an embedded NUL
  const SourceFile::Ptr f = SourceFile::load(write("bin.nodle", bytes));
  EXPECT_EQ(f->content(), std::string_view(bytes));
}

TEST_F(SourceFileTest, LoadEmptyFile) {
  const SourceFile::Ptr f = SourceFile::load(write("empty.nodle", ""));
  EXPECT_TRUE(f->content().empty());
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

namespace {

SourceFile::Ptr make(const std::string &content) {
    return SourceFile::from_string("t.nodle", content);
}

void expect_line_column(const SourceFile::Ptr &file, std::uint32_t offset, std::uint32_t line,
                        std::uint32_t column) {
    SCOPED_TRACE("offset " + std::to_string(offset));
    const LineColumn lc = file->get_line_information(offset);
    EXPECT_EQ(lc.line, line);
    EXPECT_EQ(lc.column, column);
}

std::uint32_t offset_of(const SourceFile::Ptr &file, std::uint32_t line, std::uint32_t column) {
    return file->offset_of_line(LineColumn{.line = line, .column = column});
}

} // namespace

TEST(SourceFileLines, EmptyFile) {
    auto f = make("");
    expect_line_column(f, 0, 1, 1);
}

TEST(SourceFileLines, SingleLine) {
    auto f = make("abc");
    expect_line_column(f, 0, 1, 1);
    expect_line_column(f, 1, 1, 2);
    expect_line_column(f, 2, 1, 3);
    expect_line_column(f, 3, 1, 4); // end of file
}

TEST(SourceFileLines, EveryOffsetOfAMultiLineFile) {
    //            0123 4 5 6 78
    auto f = make("ab\ncd\n\nef");
    struct Expected {
        std::uint32_t offset, line, column;
    };
    const Expected table[] = {
        {0, 1, 1}, {1, 1, 2}, {2, 1, 3}, // "ab" and its '\n'
        {3, 2, 1}, {4, 2, 2}, {5, 2, 3}, // "cd" and its '\n'
        {6, 3, 1},                       // blank line: just its '\n'
        {7, 4, 1}, {8, 4, 2}, {9, 4, 3}, // "ef" and end of file
    };
    for (const auto &e : table) {
        expect_line_column(f, e.offset, e.line, e.column);
    }
}

TEST(SourceFileLines, TrailingNewlineStartsAnEmptyLastLine) {
    auto f = make("ab\n");
    expect_line_column(f, 2, 1, 3); // the '\n'
    expect_line_column(f, 3, 2, 1); // end of file, on the new empty line
}

TEST(SourceFileLines, OnlyNewlines) {
    auto f = make("\n\n");
    expect_line_column(f, 0, 1, 1);
    expect_line_column(f, 1, 2, 1);
    expect_line_column(f, 2, 3, 1);
}

TEST(SourceFileLines, CrlfCountsCarriageReturnAsPartOfTheLine) {
    auto f = make("a\r\nb");
    expect_line_column(f, 1, 1, 2); // '\r'
    expect_line_column(f, 2, 1, 3); // '\n'
    expect_line_column(f, 3, 2, 1); // 'b'
}

TEST(SourceFileLines, OffsetsPastTheEndAreClampedToTheEndOfTheFile) {
    auto one_line = make("abc");
    expect_line_column(one_line, 4, 1, 4);
    expect_line_column(one_line, 1000, 1, 4);
    expect_line_column(one_line, std::numeric_limits<std::uint32_t>::max(), 1, 4);

    auto two_lines = make("ab\ncd");
    expect_line_column(two_lines, 1000, 2, 3);

    expect_line_column(make(""), 5, 1, 1);
    expect_line_column(make("ab\n"), 5, 2, 1);
}

TEST(SourceFileLines, LoadedFilesHaveTheirLineTable) {
    const auto path = std::filesystem::temp_directory_path() / "nodle-source-lines-test.nodle";
    {
        std::ofstream out(path, std::ios::binary);
        out << "a\nbb\nccc";
    }
    auto f = SourceFile::load(path);
    std::filesystem::remove(path);
    expect_line_column(f, 7, 3, 3); // the last 'c'
    expect_line_column(f, 2, 2, 1);
}

TEST(SourceFileOffsetOfLine, BasicPositions) {
    auto f = make("ab\ncd\nef");
    EXPECT_EQ(offset_of(f, 1, 1), 0u);
    EXPECT_EQ(offset_of(f, 1, 2), 1u);
    EXPECT_EQ(offset_of(f, 2, 1), 3u);
    EXPECT_EQ(offset_of(f, 2, 2), 4u);
    EXPECT_EQ(offset_of(f, 3, 2), 7u);
}

TEST(SourceFileOffsetOfLine, EmptyFile) {
    auto f = make("");
    EXPECT_EQ(offset_of(f, 1, 1), 0u);
    EXPECT_EQ(offset_of(f, 1, 5), 0u);
    EXPECT_EQ(offset_of(f, 3, 1), 0u);
}

TEST(SourceFileOffsetOfLine, EmptyLastLineAfterTrailingNewline) {
    auto f = make("abc\n");
    EXPECT_EQ(offset_of(f, 2, 1), 4u); // end of file, not the '\n' of line 1
}

TEST(SourceFileOffsetOfLine, BlankLineInTheMiddle) {
    auto f = make("a\n\nb");
    EXPECT_EQ(offset_of(f, 2, 1), 2u); // the blank line's '\n'
    EXPECT_EQ(offset_of(f, 3, 1), 3u);
}

TEST(SourceFileOffsetOfLine, LineBeyondTheLastGivesEndOfFile) {
    auto f = make("ab\ncd");
    EXPECT_EQ(offset_of(f, 3, 1), 5u);
    EXPECT_EQ(offset_of(f, 100, 1), 5u);
}

TEST(SourceFileOffsetOfLine, LineZeroGivesStartOfFile) {
    auto f = make("ab\ncd");
    EXPECT_EQ(offset_of(f, 0, 1), 0u);
    EXPECT_EQ(offset_of(f, 0, 7), 0u);
}

TEST(SourceFileOffsetOfLine, ColumnZeroGivesStartOfTheLine) {
    auto f = make("ab\ncd");
    EXPECT_EQ(offset_of(f, 2, 0), 3u);
}

TEST(SourceFileOffsetOfLine, ColumnPastTheEndOfALineIsClampedToItsNewline) {
    auto f = make("ab\ncd\nef");
    EXPECT_EQ(offset_of(f, 1, 3), 2u); // the '\n' itself
    EXPECT_EQ(offset_of(f, 1, 4), 2u); // past it: clamped, does not spill into line 2
    EXPECT_EQ(offset_of(f, 1, 100), 2u);
}

TEST(SourceFileOffsetOfLine, ColumnPastTheEndOfTheLastLineIsClampedToEndOfFile) {
    auto f = make("ab\ncd");
    EXPECT_EQ(offset_of(f, 2, 3), 5u); // end of file
    EXPECT_EQ(offset_of(f, 2, 100), 5u);
}

TEST(SourceFileOffsetOfLine, HugeColumnDoesNotOverflow) {
    auto f = make("ab\ncd");
    const auto huge = std::numeric_limits<std::uint32_t>::max();
    EXPECT_EQ(offset_of(f, 2, huge), 5u);
    EXPECT_EQ(offset_of(f, 1, huge), 2u);
}

TEST(SourceFileOffsetOfLine, RoundTripsWithGetLineInformation) {
    const std::vector<std::string> contents = {
        "", "\n", "abc", "abc\n", "\n\n", "a\n\nb", "a\r\nb\r\n", "ab\ncd\n\nef", "\n\nabc", "x",
    };
    for (const auto &content : contents) {
        SCOPED_TRACE("content: " + ::testing::PrintToString(content));
        auto f = make(content);
        for (std::uint32_t offset = 0; offset <= content.size(); ++offset) {
            SCOPED_TRACE("offset " + std::to_string(offset));
            const LineColumn lc = f->get_line_information(offset);
            EXPECT_EQ(f->offset_of_line(lc), offset);
        }
    }
}