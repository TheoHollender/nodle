
#include "nodle/io/file.hpp"

#include <fstream>
#include <stdexcept>
#include <algorithm>

using namespace nodle::io;

SourceFile::SourceFile(std::filesystem::path path, std::string content) {
  path_ = std::move(path);
  content_ = std::move(content);

  line_starts_ = { 0 };

  for (uint32_t offset = 0; offset < content_.size(); offset ++) {
    if (content_[offset] == '\n') {
      line_starts_.push_back(offset + 1);
    }
  }
}

SourceFile::Ptr SourceFile::load(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    throw std::runtime_error("cannot open file: " + path.string());
  }

  const auto size = input.tellg();
  input.seekg(0);

  if (size < 0) {
    throw std::runtime_error("error reading file: " + path.string());
  }
  if (static_cast<std::uintmax_t>(size) >
      std::numeric_limits<std::uint32_t>::max()) {
    throw std::runtime_error("file too large: " + path.string());
  }

  std::string content(static_cast<std::size_t>(size), '\0');
  if (!input.read(content.data(), size)) {
    throw std::runtime_error("error reading file: " + path.string());
  }

  return std::shared_ptr<SourceFile>(new SourceFile(path, content));
}
SourceFile::Ptr SourceFile::from_string(std::filesystem::path path,
                                        std::string content) {
  return std::shared_ptr<SourceFile>(new SourceFile(path, content));
}

const std::filesystem::path &SourceFile::path() const { return path_; }

std::string_view SourceFile::content() const { return content_; }
std::string_view SourceFile::text(uint32_t offset, uint32_t length) const {
  return std::string_view(content_).substr(offset, length);
}

LineColumn SourceFile::get_line_information(uint32_t offset) const {
    offset = std::min<std::size_t>(offset, content_.size());

    const auto next = std::upper_bound(line_starts_.begin(), line_starts_.end(), offset);
    const auto line = static_cast<uint32_t>(next - line_starts_.begin());

    return LineColumn{.line = line, .column = offset - line_starts_[line - 1] + 1};
}

uint32_t SourceFile::offset_of_line (LineColumn line_col) const {
  if (line_col.line < 1) {
    return 0;
  }
  if (line_col.line > line_starts_.size()) {
    return static_cast<uint32_t>(content_.size());
  }

  const uint64_t start = line_starts_[line_col.line - 1];
  if (line_col.column < 1) {
    return static_cast<uint32_t>(start);
  }

  const uint64_t last =
    line_col.line == line_starts_.size() ? content_.size() : line_starts_[line_col.line] - 1;

  return static_cast<uint32_t>(std::min<uint64_t>(start + (line_col.column - 1), last));
}