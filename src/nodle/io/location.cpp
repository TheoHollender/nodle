
#include "nodle/io/location.hpp"

using namespace nodle::io;

SourceLocation::SourceLocation(SourceFile::Ptr file, std::uint32_t offset,
                               std::uint32_t length) {
  if (!file) {
    throw std::invalid_argument("SourceLocation requires a non-null file");
  }

  file_ = std::move(file);
  offset_ = offset;
  length_ = length;
}

SourceFile::Ptr SourceLocation::file() const { return file_; }
std::uint32_t SourceLocation::offset() const { return offset_; }
std::uint32_t SourceLocation::length() const { return length_; }
std::uint32_t SourceLocation::end() const { return offset_ + length_; }

std::string_view SourceLocation::text() const {
  return file_->text(offset_, length_);
}

SourceLocation SourceLocation::operator|(const SourceLocation &other) const {
  if (file_ != other.file_) {
    throw std::runtime_error("files differ in union of source locations: '" +
                             file_->path().string() +
                             "' != " + other.file_->path().string());
  }

  std::uint32_t span_start = std::min(offset_, other.offset_);
  std::uint32_t span_end = std::max(end(), other.end());

  return SourceLocation(file_, span_start, span_end - span_start);
}