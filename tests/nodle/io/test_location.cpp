#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "nodle/io/file.hpp"
#include "nodle/io/location.hpp"

using nodle::io::SourceFile;
using nodle::io::SourceLocation;

namespace {

SourceFile::Ptr make_file(const std::string& content, const char* name = "t.nodle") {
    return SourceFile::from_string(name, content);
}

}

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
    for (const SourceLocation& u : {outer | inner, inner | outer}) {
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
    EXPECT_THROW(SourceLocation(f1, 0, 1) | SourceLocation(f2, 0, 1), std::runtime_error);
}

TEST(SourceLocationUnion, DifferentFileObjectsWithIdenticalPathAndContentStillThrow) {
    auto f1 = make_file("abc");
    auto f2 = make_file("abc");
    EXPECT_THROW(SourceLocation(f1, 0, 1) | SourceLocation(f2, 0, 1), std::runtime_error);
}

TEST(SourceLocationUnion, ErrorMessageNamesBothFiles) {
    auto f1 = make_file("abc", "one.nodle");
    auto f2 = make_file("abc", "two.nodle");
    try {
        (void)(SourceLocation(f1, 0, 1) | SourceLocation(f2, 0, 1));
        FAIL() << "expected an exception";
    } catch (const std::runtime_error& e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("one.nodle"), std::string::npos);
        EXPECT_NE(msg.find("two.nodle"), std::string::npos);
    }
}