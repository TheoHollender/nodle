
#include "nodle/io/file.hpp"

#include <fstream>
#include <stdexcept>

using namespace nodle::io;

SourceFile::SourceFile(std::filesystem::path path, std::string content) {
  path_ = std::move(path);
  content_ = std::move(content);

  // TODO compute line_starts_ here.
}

SourceFile SourceFile::load(const std::filesystem::path &path) {
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

  return SourceFile(path, content);
}

const std::filesystem::path &SourceFile::path() const { return path_; }

std::string_view SourceFile::content() const { return content_; }
std::string_view SourceFile::text(uint32_t offset, uint32_t length) const {
  return std::string_view(content_).substr(offset, length);
}
