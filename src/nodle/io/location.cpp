
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

/**
 * Returns a source location that spans the old one, plus some
 * surrounding amount of lines.
 *
 * @param count the number of lines taken above and below
 */
SourceLocation SourceLocation::expand_lines(uint32_t count) const {
  const uint32_t file_size = static_cast<uint32_t>(file_->content().size());
  const uint32_t last_line = file_->get_line_information(file_size).line;

  // Lines of the first and last character covered (an empty location covers its
  // offset).
  const uint32_t first_line = file_->get_line_information(offset_).line;
  const uint32_t last_covered = length_ == 0 ? offset_ : end() - 1;
  const uint32_t last_covered_line =
      file_->get_line_information(last_covered).line;

  const uint32_t start_line = count >= first_line ? 1 : first_line - count;
  const uint32_t end_line = static_cast<uint32_t>(
      std::min<uint64_t>(uint64_t{last_covered_line} + count, last_line));

  const uint32_t start_offset =
      file_->offset_of_line(LineColumn{.line = start_line, .column = 1});
  const uint32_t end_offset =
      file_->offset_of_line(LineColumn{.line = end_line, .column = 1}) +
      file_->line_size(end_line);

  return SourceLocation(file_, start_offset, end_offset - start_offset);
}
