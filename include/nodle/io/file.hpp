
#pragma once

#include <filesystem>
#include <vector>

namespace nodle::io {

struct SourceFile : public std::enable_shared_from_this<SourceFile> {
private:
  /* Path to the source file */
  std::filesystem::path path_;
  /* Cached content of the source file */
  std::string content_;
  // TODO compute line_starts_ and write line utils.
  // /* Indices of line starts */
  // std::vector<uint32_t> line_starts_;

  SourceFile(std::filesystem::path path, std::string content);

public:
  using Ptr = std::shared_ptr<SourceFile>;

  /**
   * Create a source file from disk.
   *
   * @param path the path of the file
   * @warning this function may throw an std::runtime_error if the file could
   * not be read.
   */
  static Ptr load(const std::filesystem::path &path);
  /**
   * Create a source file
   *
   * @param path the path of the file
   * @param content the content stored on the file on disk
   */
  static Ptr from_string(std::filesystem::path path, std::string content);

  /**
   * Return the path of the source file.
   */
  const std::filesystem::path &path() const;

  /**
   * Returns a string view of the content of the file.
   */
  std::string_view content() const;
  /**
   * Returns a substring of the content of the file.
   *
   * @param offset the starting offset
   * @param length the length of the substring to use
   */
  std::string_view text(uint32_t offset, uint32_t length) const;
};

}; // namespace nodle::io
