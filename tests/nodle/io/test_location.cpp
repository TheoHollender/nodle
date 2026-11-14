#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"
#include <gtest/gtest.h>

using nodle::io::SourceFile;
using nodle::io::SourceLocation;

namespace {

SourceFile::Ptr make_file(const std::string &content,
                          const char *name = "t.nodle") {
  return SourceFile::from_string(name, content);
}

} // namespace

TEST(SourceLocation, StoresFileOffsetAndLength) {
  auto file = make_file("hello world");
  SourceLocation loc(file, 6, 5);
  EXPECT_EQ(loc.file(), file);
  EXPECT_EQ(loc.offset(), 6u);
  EXPECT_EQ(loc.length(), 5u);
  EXPECT_EQ(loc.end(), 11u);
}

TEST(SourceLocation, TextReturnsCoveredSlice) {
  SourceLocation loc(make_file("hello world"), 6, 5);
  EXPECT_EQ(loc.text(), "world");
}

TEST(SourceLocation, ZeroLengthLocationHasEmptyText) {
  SourceLocation loc(make_file("abc"), 3, 0);
  EXPECT_EQ(loc.end(), 3u);
  EXPECT_TRUE(loc.text().empty());
}

TEST(SourceLocation, NullFileIsRejected) {
  EXPECT_THROW(SourceLocation(nullptr, 0, 0), std::invalid_argument);
}

TEST(SourceLocation, KeepsFileAliveAfterOriginalPointerIsReleased) {
  std::weak_ptr<const SourceFile> weak;
  {
    auto file = make_file("hello world");
    weak = file;
    SourceLocation loc(std::move(file), 0, 5);
    EXPECT_FALSE(weak.expired());
    EXPECT_EQ(loc.text(), "hello");
  }
  EXPECT_TRUE(weak.expired());
}

TEST(SourceLocation, CopiesShareTheSameFile) {
  auto file = make_file("abc");
  SourceLocation a(file, 0, 1);
  SourceLocation b = a;
  EXPECT_EQ(a.file(), b.file());
  EXPECT_EQ(file.use_count(), 3);
}

TEST(SourceLocation, TextThrowsWhenOffsetIsPastTheEndOfTheFile) {
  SourceLocation loc(make_file("abc"), 10, 1);
  EXPECT_THROW(loc.text(), std::out_of_range);
}

TEST(SourceLocationUnion, DisjointRangesCoverTheGap) {
  auto file = make_file("hello world");
  SourceLocation a(file, 0, 5);
  SourceLocation b(file, 6, 5);
  SourceLocation u = a | b;
  EXPECT_EQ(u.offset(), 0u);
  EXPECT_EQ(u.end(), 11u);
  EXPECT_EQ(u.text(), "hello world");
}

TEST(SourceLocationUnion, IsOrderIndependent) {
  auto file = make_file("hello world");
  SourceLocation a(file, 0, 5);
  SourceLocation b(file, 6, 5);
  SourceLocation ab = a | b;
  SourceLocation ba = b | a;
  EXPECT_EQ(ab.offset(), ba.offset());
  EXPECT_EQ(ab.length(), ba.length());
}

TEST(SourceLocationUnion, OverlappingRanges) {
  auto file = make_file("abcdefghij");
  SourceLocation u = SourceLocation(file, 2, 5) | SourceLocation(file, 4, 4);
  EXPECT_EQ(u.offset(), 2u);
  EXPECT_EQ(u.end(), 8u);
  EXPECT_EQ(u.text(), "cdefgh");
}

TEST(SourceLocationUnion, NestedRangeYieldsTheOuterOne) {
  auto file = make_file("abcdefghij");
  SourceLocation outer(file, 1, 8);
  SourceLocation inner(file, 3, 2);
  for (const SourceLocation &u : {outer | inner, inner | outer}) {
    EXPECT_EQ(u.offset(), 1u);
    EXPECT_EQ(u.end(), 9u);
  }
}

TEST(SourceLocationUnion, AdjacentRanges) {
  auto file = make_file("abcdef");
  SourceLocation u = SourceLocation(file, 0, 3) | SourceLocation(file, 3, 3);
  EXPECT_EQ(u.offset(), 0u);
  EXPECT_EQ(u.length(), 6u);
}

TEST(SourceLocationUnion, WithItselfIsUnchanged) {
  SourceLocation a(make_file("abcdef"), 2, 3);
  SourceLocation u = a | a;
  EXPECT_EQ(u.offset(), 2u);
  EXPECT_EQ(u.length(), 3u);
}

TEST(SourceLocationUnion, ZeroLengthLocationExtendsTheRange) {
  auto file = make_file("abcdef");
  SourceLocation u = SourceLocation(file, 1, 2) | SourceLocation(file, 6, 0);
  EXPECT_EQ(u.offset(), 1u);
  EXPECT_EQ(u.end(), 6u);
}

TEST(SourceLocationUnion, ResultKeepsTheSameFile) {
  auto file = make_file("abcdef");
  SourceLocation u = SourceLocation(file, 0, 1) | SourceLocation(file, 4, 1);
  EXPECT_EQ(u.file(), file);
}

TEST(SourceLocationUnion, DifferentFilesThrow) {
  auto f1 = make_file("abc", "one.nodle");
  auto f2 = make_file("abc", "two.nodle");
  EXPECT_THROW(SourceLocation(f1, 0, 1) | SourceLocation(f2, 0, 1),
               std::runtime_error);
}

TEST(SourceLocationUnion,
     DifferentFileObjectsWithIdenticalPathAndContentStillThrow) {
  auto f1 = make_file("abc");
  auto f2 = make_file("abc");
  EXPECT_THROW(SourceLocation(f1, 0, 1) | SourceLocation(f2, 0, 1),
               std::runtime_error);
}

TEST(SourceLocationUnion, ErrorMessageNamesBothFiles) {
  auto f1 = make_file("abc", "one.nodle");
  auto f2 = make_file("abc", "two.nodle");
  try {
    (void)(SourceLocation(f1, 0, 1) | SourceLocation(f2, 0, 1));
    FAIL() << "expected an exception";
  } catch (const std::runtime_error &e) {
    const std::string msg = e.what();
    EXPECT_NE(msg.find("one.nodle"), std::string::npos);
    EXPECT_NE(msg.find("two.nodle"), std::string::npos);
  }
}

namespace {

constexpr std::uint32_t kMax = std::numeric_limits<std::uint32_t>::max();

SourceFile::Ptr make(const std::string &content) {
  return SourceFile::from_string("t.nodle", content);
}

// Offsets of kFive:  l1 = 0..1 ('\n' 2)  l2 = 3..4 ('\n' 5)  l3 = 6..7 ('\n' 8)
//                    l4 = 9..10 ('\n' 11)  l5 = 12..13 (end of file = 14)
const std::string kFive = "l1\nl2\nl3\nl4\nl5";

SourceLocation expand(const SourceFile::Ptr &file, std::uint32_t offset,
                      std::uint32_t length, std::uint32_t count) {
  return SourceLocation(file, offset, length).expand_lines(count);
}

void expect_span(const SourceLocation &loc, std::uint32_t offset,
                 std::uint32_t length) {
  EXPECT_EQ(loc.offset(), offset);
  EXPECT_EQ(loc.length(), length);
}

} // namespace

TEST(ExpandLines, DefaultCountIsZero) {
  auto f = make(kFive);
  SourceLocation loc(f, 7, 1);
  const SourceLocation by_default = loc.expand_lines();
  const SourceLocation explicit_zero = loc.expand_lines(0);
  expect_span(by_default, explicit_zero.offset(), explicit_zero.length());
  EXPECT_EQ(by_default.text(), "l3");
}

TEST(ExpandLines, LocationInsideALineExpandsToTheWholeLine) {
  auto f = make(kFive);
  auto r = expand(f, 6, 1, 0); // the 'l' of l3
  expect_span(r, 6, 2);
  EXPECT_EQ(r.text(), "l3");

  EXPECT_EQ(expand(f, 7, 1, 0).text(), "l3"); // the '3'
}

TEST(ExpandLines, WholeLineStaysTheSame) {
  auto f = make(kFive);
  expect_span(expand(f, 6, 2, 0), 6, 2);
}

TEST(ExpandLines, MultiLineLocationExpandsToTheFirstAndLastLine) {
  auto f = make(kFive);
  auto r = expand(f, 4, 4, 0); // "2\nl3"
  expect_span(r, 3, 5);
  EXPECT_EQ(r.text(), "l2\nl3");
}

TEST(ExpandLines, FirstAndLastLineOfTheFile) {
  auto f = make(kFive);
  EXPECT_EQ(expand(f, 0, 1, 0).text(), "l1");
  EXPECT_EQ(expand(f, 13, 1, 0).text(), "l5");
}

TEST(ExpandLines, OneLineAboveAndBelow) {
  auto f = make(kFive);
  auto r = expand(f, 6, 1, 1);
  expect_span(r, 3, 8);
  EXPECT_EQ(r.text(), "l2\nl3\nl4");
}

TEST(ExpandLines, TwoLinesAboveAndBelow) {
  auto f = make(kFive);
  auto r = expand(f, 6, 1, 2);
  expect_span(r, 0, 14);
  EXPECT_EQ(r.text(), kFive);
}

TEST(ExpandLines, MultiLineLocationWithContext) {
  auto f = make(kFive);
  EXPECT_EQ(expand(f, 4, 4, 1).text(), "l1\nl2\nl3\nl4");
}

TEST(ExpandLines, ContextIsClampedAtTheTop) {
  auto f = make(kFive);
  EXPECT_EQ(expand(f, 0, 1, 1).text(), "l1\nl2");
  EXPECT_EQ(expand(f, 0, 1, 2).text(), "l1\nl2\nl3");
  EXPECT_EQ(expand(f, 3, 1, 1).text(),
            "l1\nl2\nl3"); // line 2, count 1: exactly reaches line 1
  EXPECT_EQ(expand(f, 3, 1, 5).text(), kFive); // line 2, count > line number
}

TEST(ExpandLines, ContextIsClampedAtTheBottom) {
  auto f = make(kFive);
  EXPECT_EQ(expand(f, 12, 1, 1).text(), "l4\nl5");
  EXPECT_EQ(expand(f, 12, 1, 9).text(), kFive);
  expect_span(expand(f, 12, 1, 9), 0,
              14); // length must not run past the end of the file
}

TEST(ExpandLines, CountLargerThanTheFileGivesTheWholeFile) {
  auto f = make(kFive);
  expect_span(expand(f, 6, 1, 100), 0, 14);
}

TEST(ExpandLines, MaximumCountDoesNotOverflow) {
  auto f = make(kFive);
  expect_span(expand(f, 0, 1, kMax), 0, 14);
  expect_span(expand(f, 6, 1, kMax), 0, 14);
  expect_span(expand(f, 13, 1, kMax), 0, 14);
}

TEST(ExpandLines, ZeroLengthLocationExpandsToItsLine) {
  auto f = make(kFive);
  EXPECT_EQ(expand(f, 7, 0, 0).text(), "l3");
  EXPECT_EQ(expand(f, 0, 0, 0).text(), "l1");
  EXPECT_EQ(expand(f, 6, 0, 1).text(), "l2\nl3\nl4");
}

TEST(ExpandLines, ZeroLengthLocationAtEndOfFile) {
  auto f = make("ab\ncd");
  EXPECT_EQ(expand(f, 5, 0, 0).text(), "cd");
  EXPECT_EQ(expand(f, 5, 0, 1).text(), "ab\ncd");
}

TEST(ExpandLines, EmptyFile) {
  auto f = make("");
  expect_span(expand(f, 0, 0, 0), 0, 0);
  expect_span(expand(f, 0, 0, 3), 0, 0);
}

TEST(ExpandLines, FileWithTrailingNewline) {
  auto f = make("ab\ncd\n");
  EXPECT_EQ(expand(f, 3, 2, 0).text(),
            "cd");                       // the '\n' is not part of the line
  expect_span(expand(f, 6, 0, 0), 6, 0); // end of file is on an empty last line
  EXPECT_EQ(expand(f, 6, 0, 1).text(), "cd\n"); // line 2 and the empty line 3
}

TEST(ExpandLines, BlankLines) {
  auto f = make("a\n\nb");
  expect_span(expand(f, 2, 1, 0), 2, 0); // a blank line has no characters
  EXPECT_EQ(expand(f, 2, 1, 1).text(), "a\n\nb");
  EXPECT_EQ(expand(f, 0, 1, 1).text(), "a\n");
}

TEST(ExpandLines, CrlfKeepsTheCarriageReturn) {
  auto f = make("a\r\nb\r\nc");
  EXPECT_EQ(expand(f, 3, 1, 0).text(), "b\r");
  EXPECT_EQ(expand(f, 3, 1, 1).text(), "a\r\nb\r\nc");
}

TEST(ExpandLines, LocationEndingWithANewlineDoesNotPullInTheNextLine) {
  auto f = make("ab\ncd");
  EXPECT_EQ(expand(f, 0, 3, 0).text(),
            "ab"); // covers "ab\n": only line 1 is touched
}

TEST(ExpandLines, KeepsTheSameFileAndLeavesTheOriginalUntouched) {
  auto f = make(kFive);
  SourceLocation original(f, 6, 1);
  SourceLocation r = original.expand_lines(1);
  EXPECT_EQ(r.file(), f);
  EXPECT_EQ(original.offset(), 6u);
  EXPECT_EQ(original.length(), 1u);
}

TEST(ExpandLines, ExpandingTwiceByZeroIsIdempotent) {
  auto f = make(kFive);
  auto once = expand(f, 7, 1, 0);
  auto twice = once.expand_lines(0);
  expect_span(twice, once.offset(), once.length());
}

TEST(ExpandLines, ExpandingByOneTwiceEqualsExpandingByTwo) {
  auto f = make(kFive);
  auto twice = SourceLocation(f, 6, 1).expand_lines(1).expand_lines(1);
  auto direct = SourceLocation(f, 6, 1).expand_lines(2);
  expect_span(twice, direct.offset(), direct.length());
}

TEST(ExpandLines, PropertiesHoldForEveryLocationOfSmallFiles) {
  const std::vector<std::string> contents = {
      "",       "\n",         "abc",         "abc\n",  "a\n\nb",
      "\n\nab", "ab\ncd\nef", "a\r\nb\r\nc", "x\ny\n",
  };
  for (const auto &content : contents) {
    SCOPED_TRACE("content: " + ::testing::PrintToString(content));
    auto f = make(content);
    const auto size = static_cast<std::uint32_t>(content.size());
    const auto last_line = f->get_line_information(size).line;

    for (std::uint32_t offset = 0; offset <= size; ++offset) {
      for (std::uint32_t length = 0; offset + length <= size; ++length) {
        for (std::uint32_t count : {0u, 1u, 2u, 5u, kMax}) {
          SCOPED_TRACE("offset " + std::to_string(offset) + " length " +
                       std::to_string(length) + " count " +
                       std::to_string(count));
          const SourceLocation original(f, offset, length);
          const SourceLocation r = original.expand_lines(count);

          // contained in the file
          ASSERT_LE(r.end(), size);
          // contains the original
          // contains the original, except for a trailing '\n' (never part of a
          // line)
          const bool ends_with_newline =
              length > 0 && content[original.end() - 1] == '\n';
          EXPECT_LE(r.offset(), original.offset());
          EXPECT_GE(r.end(), original.end() - (ends_with_newline ? 1 : 0));
          // starts at a line start and ends at a line end
          EXPECT_TRUE(r.offset() == 0 || content[r.offset() - 1] == '\n');
          EXPECT_TRUE(r.end() == size || content[r.end()] == '\n');

          // spans exactly the right lines
          const auto first = f->get_line_information(offset).line;
          const auto last_char = length == 0 ? offset : offset + length - 1;
          const auto last = f->get_line_information(last_char).line;
          const std::uint64_t want_first = count >= first ? 1 : first - count;
          const std::uint64_t want_last =
              std::min<std::uint64_t>(std::uint64_t{last} + count, last_line);
          EXPECT_EQ(f->get_line_information(r.offset()).line, want_first);
          EXPECT_EQ(f->get_line_information(r.end()).line, want_last);
        }
      }
    }
  }
}