
#include "nodle/io/file.hpp"

namespace nodle::io {

struct SourceLocation {
public:
  /**
   * Create a source location.
   *
   * @param file the input file of the source.
   * @param offset the offset for the content.
   * @param length the length of the location.
   */
  SourceLocation(SourceFile::Ptr file, std::uint32_t offset,
                 std::uint32_t length);

  /**
   * Returns the file of the location.
   */
  SourceFile::Ptr file() const;
  /**
   * Returns the starting position of the location.
   */
  std::uint32_t offset() const;
  /**
   * Returns the length of the location.
   */
  std::uint32_t length() const;
  /**
   * Returns the ending position of the location.
   * @return the minimal offset such that it is no longer part of the location.
   */
  std::uint32_t end() const;

  /**
   * Returns a view of the string location.
   */
  std::string_view text() const;

  /* Takes the smallest source location that contains the two source locations
   */
  SourceLocation operator|(const SourceLocation &other) const;

private:
  SourceFile::Ptr file_;
  std::uint32_t offset_, length_;
};

}; // namespace nodle::io
