#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"

using nodle::io::SourceFile;
using nodle::io::SourceLocation;

namespace {

SourceFile::Ptr make(const std::string &content,
                     const char *name = "file.nodle") {
  return SourceFile::from_string(name, content);
}

std::string debug(const SourceFile::Ptr &f, std::uint32_t offset,
                  std::uint32_t length, std::uint32_t context) {
  return SourceLocation(f, offset, length).as_debug_string(context);
}

// Joins lines with '\n' (no trailing newline).
std::string lines(std::initializer_list<std::string> ls) {
  std::string out;
  for (const auto &l : ls) {
    if (!out.empty() || &l != ls.begin()) {
      out += '\n';
    }
    out += l;
  }
  return out;
}

std::string repeat(char c, std::size_t n) { return std::string(n, c); }

bool contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

// A five-line file; line 3 is `let x: Integer = "abc";` (23 characters,
// offsets 18..40).
const std::string kProgram =
    "line one\nline two\nlet x: Integer = \"abc\";\nline four\nline five";

// Rows look like  " 3 | code"  and  "   | ^^^^"  (the gutter is as wide as the
// largest line number shown), framed by a header and a footer of dashes.
} // namespace

// -------------------------------------------------------------- single line

TEST(DebugString, SingleLineWithContext) {
  auto f = make(kProgram);
  EXPECT_EQ(debug(f, 18, 23, 1), lines({
                                     "   -------> file.nodle:3:1",
                                     " 2 | line two",
                                     " 3 | let x: Integer = \"abc\";",
                                     "   | " + repeat('^', 23),
                                     " 4 | line four",
                                     "   " + repeat('-', 20),
                                 }));
}

TEST(DebugString, NoContext) {
  auto f = make(kProgram);
  EXPECT_EQ(debug(f, 18, 23, 0), lines({
                                     "   -------> file.nodle:3:1",
                                     " 3 | let x: Integer = \"abc\";",
                                     "   | " + repeat('^', 23),
                                     "   " + repeat('-', 20),
                                 }));
}

TEST(DebugString, UnderlineCoversOnlyTheLocation) {
  auto f = make(kProgram);
  // "Integer" starts at offset 25 = column 8 of line 3.
  EXPECT_EQ(debug(f, 25, 7, 0),
            lines({
                "   -------> file.nodle:3:8",
                " 3 | let x: Integer = \"abc\";",
                "   | " + repeat(' ', 7) + repeat('^', 7) + repeat(' ', 9),
                "   " + repeat('-', 20),
            }));
}

TEST(DebugString, ContextIsClampedToTheFile) {
  auto f = make("one\ntwo");
  EXPECT_EQ(debug(f, 0, 3, 5), lines({
                                   "   -------> file.nodle:1:1",
                                   " 1 | one",
                                   "   | ^^^",
                                   " 2 | two",
                                   "   " + repeat('-', 20),
                               }));
}

TEST(DebugString, DefaultContextIsThreeLines) {
  std::string content;
  for (int i = 1; i <= 10; ++i) {
    content += "row" + std::to_string(i) + (i < 10 ? "\n" : "");
  }
  auto f = make(content);
  const SourceLocation loc(f, 20, 4); // "row5": rows 1-4 are 5 bytes each
  ASSERT_EQ(loc.text(), "row5");
  const std::string s = loc.as_debug_string();
  EXPECT_FALSE(contains(s, "row1"));
  EXPECT_TRUE(contains(s, "row2"));
  EXPECT_TRUE(contains(s, "row8"));
  EXPECT_FALSE(contains(s, "row9"));
  EXPECT_EQ(s, loc.as_debug_string(3));
}

TEST(DebugString, GutterIsAsWideAsTheLargestLineNumber) {
  std::string content;
  for (int i = 1; i <= 12; ++i) {
    content += "r" + std::to_string(i) + (i < 12 ? "\n" : "");
  }
  auto f = make(content);
  // r9 starts at 8 * 3 = 24 ("r1\n" .. "r8\n" are 3 bytes each).
  const SourceLocation loc(f, 24, 2);
  ASSERT_EQ(loc.text(), "r9");
  EXPECT_EQ(loc.as_debug_string(1), lines({
                                        "    -------> file.nodle:9:1",
                                        "  8 | r8",
                                        "  9 | r9",
                                        "    | ^^",
                                        " 10 | r10",
                                        "    " + repeat('-', 20),
                                    }));
}

TEST(DebugString, ShowsThePathAsGiven) {
  auto f = make("x", "dir/sub/file.nodle");
  EXPECT_TRUE(contains(debug(f, 0, 1, 0), "-------> dir/sub/file.nodle:1:1"));
}

TEST(DebugString, HasNoTrailingNewline) {
  auto f = make(kProgram);
  const std::string s = debug(f, 18, 23, 1);
  EXPECT_EQ(s.back(), '-');
  EXPECT_NE(s.back(), '\n');
}

// --------------------------------------------------------------- multi-line

TEST(DebugString, MultiLineLocationUnderlinesEveryCoveredLine) {
  // line 1 "a" | 2 "bb ccc" | 3 "dd" | 4 "eee fff" | 5 "g"
  auto f = make("a\nbb ccc\ndd\neee fff\ng");
  // From "ccc" (offset 5) to the end of "eee" (offset 15, exclusive).
  EXPECT_EQ(debug(f, 5, 10, 1), lines({
                                    "   -------> file.nodle:2:4",
                                    " 1 | a",
                                    " 2 | bb ccc",
                                    "   |    ^^^",
                                    " 3 | dd",
                                    "   | ^^",
                                    " 4 | eee fff",
                                    "   | ^^^    ",
                                    " 5 | g",
                                    "   " + repeat('-', 20),
                                }));
}

TEST(DebugString, BlankLinesInsideALocationHaveNoUnderline) {
  auto f = make("ab\n\ncd");
  EXPECT_EQ(debug(f, 0, 6, 0), lines({
                                   "   -------> file.nodle:1:1",
                                   " 1 | ab",
                                   "   | ^^",
                                   " 2 | ",
                                   " 3 | cd",
                                   "   | ^^",
                                   "   " + repeat('-', 20),
                               }));
}

TEST(DebugString, EmptyFile) {
  auto f = make("");
  EXPECT_EQ(debug(f, 0, 0, 3), lines({
                                   "   -------> file.nodle:1:1",
                                   " 1 | ",
                                   "   " + repeat('-', 20),
                               }));
}

TEST(DebugString, EmptyLocationAtEndOfAFileEndingWithANewline) {
  auto f = make("ab\n");
  EXPECT_EQ(debug(f, 3, 0, 1), lines({
                                   "   -------> file.nodle:2:1",
                                   " 1 | ab",
                                   "   " + repeat('-', 20),
                               }));
}

TEST(DebugString, HugeContextShowsTheWholeFileWithoutOverflowing) {
  auto f = make(kProgram);
  const std::string s =
      debug(f, 18, 23, std::numeric_limits<std::uint32_t>::max());
  for (const char *row :
       {"line one", "line two", "let x", "line four", "line five"}) {
    EXPECT_TRUE(contains(s, row)) << row;
  }
}

// --------------------------------------------------- characters that need care

TEST(DebugString, CarriageReturnsAreNotDisplayed) {
  auto f = make("a\r\nb");
  EXPECT_EQ(debug(f, 0, 2, 0), lines({
                                   // the location even covers the '\r'
                                   "   -------> file.nodle:1:1",
                                   " 1 | a",
                                   "   | ^",
                                   "   " + repeat('-', 20),
                               }));
}

TEST(DebugString, TabsAreExpandedAndTheUnderlineLinesUp) {
  auto f = make("\tx = 1");
  EXPECT_EQ(debug(f, 1, 1, 0), lines({
                                   "   -------> file.nodle:1:2",
                                   " 1 |     x = 1",
                                   "   | " + repeat(' ', 4) + "^    ",
                                   "   " + repeat('-', 20),
                               }));
}

TEST(DebugString, TabsAdvanceToTheNextTabStop) {
  auto f = make("ab\tc");
  // 'c' is at byte 3; "ab" + tab reaches column 4.
  EXPECT_EQ(debug(f, 3, 1, 0), lines({
                                   "   -------> file.nodle:1:4",
                                   " 1 | ab    c",
                                   "   | " + repeat(' ', 6) + "^",
                                   "   " + repeat('-', 20),
                               }));
}

TEST(DebugString, LocationEndingWithANewlineDoesNotUnderlineTheNextLine) {
  auto f = make("ab\ncd");
  const std::string s = debug(f, 0, 3, 0); // covers "ab\n"
  EXPECT_TRUE(contains(s, "| ab"));
  EXPECT_FALSE(contains(s, "| cd"));
}

TEST(DebugString, ContinuationLinesAreNotUnderlinedOverTheirIndentation) {
  auto f = make("x(\n    a,\n    b\n)");
  EXPECT_EQ(debug(f, 0, 17, 0), lines({
                                    "   -------> file.nodle:1:1",
                                    " 1 | x(",
                                    "   | ^^",
                                    " 2 |     a,",
                                    "   | ^^^^^^",
                                    " 3 |     b",
                                    "   | ^^^^^",
                                    " 4 | )",
                                    "   | ^",
                                    "   " + repeat('-', 20),
                                }));
}

TEST(DebugString, TrailingBlanksBeforeALineBreakAreNotUnderlined) {
  auto f = make("a  \nb");
  EXPECT_EQ(debug(f, 0, 5, 0),
            lines({
                "   -------> file.nodle:1:1",
                " 1 | a  ", // the source row is shown as written; only the
                            // underline skips the blanks
                "   | ^^^",
                " 2 | b",
                "   | ^",
                "   " + repeat('-', 20),
            }));
}

TEST(DebugString, WhitespaceOnlyLinesInsideALocationHaveNoUnderline) {
  auto f = make("a\n   \nb");
  const std::string s = debug(f, 0, 7, 0);
  EXPECT_TRUE(contains(s, " 2 |    \n"));
  EXPECT_EQ(s, lines({
                   "   -------> file.nodle:1:1",
                   " 1 | a",
                   "   | ^",
                   " 2 |    ",
                   "   | ^^^",
                   " 3 | b",
                   "   | ^",
                   "   " + repeat('-', 20),
               }));
}

TEST(DebugString, ASingleLineLocationKeepsItsOwnBlanks) {
  auto f = make("a b c");
  // Covers "a b c" entirely; nothing is trimmed on a one-line location.
  EXPECT_EQ(debug(f, 1, 3, 0), lines({
                                   "   -------> file.nodle:1:2",
                                   " 1 | a b c",
                                   "   |  ^^^ ",
                                   "   " + repeat('-', 20),
                               }));
}