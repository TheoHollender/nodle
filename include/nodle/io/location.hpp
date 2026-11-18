
#pragma once
#include "nodle/io/file.hpp"
#include <memory>
#include <optional>

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

  /**
   * Takes the smallest source location that contains the two source locations
   */
  SourceLocation operator|(const SourceLocation &other) const;

  /**
   * Returns a source location that spans the old one, plus some
   * surrounding context.
   *
   * @param count the number of lines taken above and below
   */
  SourceLocation expand_lines(uint32_t count = 0) const;

  /**
   * Returns the location in a printable format for debug output.
   *
   * @param context the number of lines taken above and below the line to print
   * the string.
   * @param note_location if enabled, adds an empty line padded for having a
   * note below the code.
   */
  std::string as_debug_string(bool note_location = false,
                              uint32_t context = 3) const;

  /**
   * Returns the padding of the debug string.
   *
   * @warning as_debug_string should be called before debug_string_padding
   */
  int debug_string_padding() const;

private:
  SourceFile::Ptr file_;
  std::uint32_t offset_, length_;

  std::shared_ptr<std::optional<int>> ref_debug_string_padding_;
};

}; // namespace nodle::io
