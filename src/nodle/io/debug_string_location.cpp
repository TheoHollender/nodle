
#include "nodle/io/location.hpp"

#include <functional>
#include <string>

using namespace nodle::io;

namespace {

struct DisplayLine {
  std::string line;
  std::string underlined;

  bool has_underline = false;
};

std::vector<DisplayLine> create_display_lines(std::string_view content,
                                              std::uint32_t start_offset,
                                              std::uint32_t end_offset,
                                              std::uint32_t start_underline,
                                              std::uint32_t end_underline) {
  std::vector<DisplayLine> lines;

  DisplayLine current_line;
  for (std::uint32_t offset = start_offset; offset < end_offset; offset++) {
    if (content[offset] == '\n') {
      lines.push_back(current_line);
      current_line = DisplayLine();
      continue;
    }
    if (content[offset] == '\r') {
      continue;
    }

    bool is_underline = start_underline <= offset && offset < end_underline;

    std::string resulting;
    resulting.push_back(content[offset]);

    if (content[offset] == '\t') {
      resulting = "    ";
    }

    std::string underline_resulting(resulting.size(), ' ');
    if (is_underline) {
      std::fill(underline_resulting.begin(), underline_resulting.end(), '^');
      current_line.has_underline = true;
    }

    current_line.line.insert(current_line.line.end(), resulting.begin(),
                             resulting.end());
    current_line.underlined.insert(current_line.underlined.end(),
                                   underline_resulting.begin(),
                                   underline_resulting.end());
  }

  if (current_line.line.size() != 0) {
    lines.push_back(current_line);
  }

  return lines;
}

}; // namespace

std::string SourceLocation::as_debug_string(bool note_location,
                                            uint32_t context) const {
  // Format the string as
  //     -----> file.nodle:line:column
  //   8 | before code
  //   9 | before code
  //  10 |
  //     |
  //  11 | let x: Integer = "abc";
  //     |        ~~~~~~~   ~~~~~
  //  13 |
  //  14 | rest of the code
  //  15 | rest of the code
  //     ---------------------------------------------

  SourceLocation total_location = expand_lines(context);

  const auto line_column = file_->get_line_information(offset_);

  const uint32_t first_line =
      file_->get_line_information(total_location.offset_).line;
  const uint32_t last_covered = total_location.length_ == 0
                                    ? total_location.offset_
                                    : total_location.end() - 1;
  const uint32_t last_covered_line =
      file_->get_line_information(last_covered).line;

  std::vector<DisplayLine> display_lines = create_display_lines(
      file_->content(), total_location.offset(), total_location.end(), offset(),
      std::max(offset() + 1, end()));

  uint32_t padding = std::to_string(last_covered_line).size();
  std::stringstream buffer;

  std::function<void(std::string)> print_padded = [&](std::string value) {
    buffer << " ";
    for (uint32_t offset = 0; offset + value.size() < padding; offset++)
      buffer << " ";
    buffer << value << " ";
  };

  print_padded("");
  buffer << "-------> " << file_->path().string() << ":" << line_column.line
         << ":" << line_column.column;

  for (uint32_t line = first_line; line <= last_covered_line; line++) {
    uint32_t index = line - first_line;
    if (index >= display_lines.size()) {
      buffer << "\n";
      print_padded(std::to_string(line));
      buffer << "| ";

      continue;
    }

    buffer << "\n";
    print_padded(std::to_string(line));
    buffer << "| " << display_lines[index].line;
    if (display_lines[index].has_underline) {
      buffer << "\n";
      print_padded("");
      buffer << "| " << display_lines[index].underlined;
    }
  }

  buffer << "\n";
  print_padded("");
  buffer << "--------------------";

  if (note_location) {
    buffer << "\n";
    print_padded("");
    buffer << "> ";
  }

  *ref_debug_string_padding_ = padding + 2;

  return buffer.str();
}

int SourceLocation::debug_string_padding() const {
  const auto &opt = ref_debug_string_padding_.get();
  if (!opt->has_value()) {
    throw std::runtime_error(
        "debug_string_padding() should be called after as_debug_string()");
  }

  return opt->value();
}
