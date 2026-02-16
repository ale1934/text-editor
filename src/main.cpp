#include "lexer.h"
#include "token.h"
#include <algorithm>
#include <ext/rope>
#include <fcntl.h>
#include <fstream>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/string.hpp>
#include <mutex>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <vector>

#define NORMAL false
#define INSERT true

#define NONE 0
#define OPENFILE 1
#define RENAME 2

using namespace __gnu_cxx;
using namespace ftxui;

struct LiveProcess {
  pid_t pid = -1;
  int fd_in = -1;
  int fd_out = -1;
  std::thread reader;
  std::mutex mtx;
  std::queue<std::string> output_queue;
  bool running = false;

  void Stop() {
    // Stop the reader first
    {
      std::lock_guard<std::mutex> lock(mtx);
      running = false;
    }

    // Close FDs safely
    if (fd_in >= 0) {
      close(fd_in);
      fd_in = -1;
    }
    if (fd_out >= 0) {
      close(fd_out);
      fd_out = -1;
    }

    if (reader.joinable())
      reader.join();

    if (pid > 0) {
      int status;
      waitpid(pid, &status, WNOHANG); // non-blocking
      pid = -1;
    }
    std::lock_guard<std::mutex> lock(mtx);
    std::queue<std::string> empty;
    std::swap(output_queue, empty);
  }

  void Start(const std::string &cmd, const std::vector<std::string> &args,
             std::function<void()> on_output = nullptr) {
    Stop();
    int pipe_in[2], pipe_out[2];
    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1)
      return;

    pid = fork();
    if (pid == 0) {
      dup2(pipe_in[0], STDIN_FILENO);
      dup2(pipe_out[1], STDOUT_FILENO);
      dup2(pipe_out[1], STDERR_FILENO);
      close(pipe_in[0]);
      close(pipe_in[1]);
      close(pipe_out[0]);
      close(pipe_out[1]);

      std::vector<char *> argv;
      argv.push_back((char *)cmd.c_str());
      for (auto &a : args)
        argv.push_back((char *)a.c_str());
      argv.push_back(nullptr);
      execvp(cmd.c_str(), argv.data());
      _exit(1);
    }

    fd_in = pipe_in[1];
    close(pipe_in[0]);
    fd_out = pipe_out[0];
    close(pipe_out[1]);

    fcntl(fd_out, F_SETFL, O_NONBLOCK);

    {
      std::lock_guard<std::mutex> lock(mtx);
      running = true;
    }

    reader = std::thread([this, on_output]() {
      char buffer[256];
      while (true) {
        bool still_running;
        {
          std::lock_guard<std::mutex> lock(mtx);
          still_running = running;
        }
        if (!still_running)
          break;

        ssize_t n = read(fd_out, buffer, sizeof(buffer));
        if (n > 0) {
          {
            std::lock_guard<std::mutex> lock(mtx);
            output_queue.push(std::string(buffer, n));
          }
          if (on_output)
            on_output();
        } else if (n == 0) {
          std::lock_guard<std::mutex> lock(mtx);
          running = false;
          break;
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
      }
    });
  }

  void WriteInput(const std::string &input) {
    if (fd_in >= 0)
      write(fd_in, input.c_str(), input.size());
  }

  std::vector<std::string> FetchOutput() {
    std::vector<std::string> out;
    std::lock_guard<std::mutex> lock(mtx);
    while (!output_queue.empty()) {
      out.push_back(output_queue.front());
      output_queue.pop();
    }
    return out;
  }
};

Color ColorForToken(TokenType type) {
  switch (type) {
  case TokenType::VAR:
  case TokenType::RETURN:
  case TokenType::IF:
  case TokenType::ELSE:
  case TokenType::FUNCTION:
    return Color::DeepPink1;

  case TokenType::EXIT:
    return Color::DarkSeaGreen;

  case TokenType::WHILE:
  case TokenType::FOR:
    return Color::Red;

  case TokenType::STRING:
    return Color::Green;

  case TokenType::TRUE:
  case TokenType::FALSE:
    return Color::LightGreen;

  case TokenType::INT:
    return Color::Yellow;

  case TokenType::IDENT:
    return Color::Orange1;

  case TokenType::PLUS:
  case TokenType::MINUS:
  case TokenType::SLASH:
  case TokenType::ASTERISK:
  case TokenType::EQ:
  case TokenType::NEQ:
    return Color::Cyan;

  case TokenType::COMMENT:
    return Color::DarkSlateGray1;

  default:
    return Color::LightSkyBlue1;
  }
}

std::vector<std::pair<std::string, Color>>
TokenizeWithColors(const std::string &line) {
  Lexer lexer(line);
  std::vector<std::pair<std::string, Color>> parts;

  Token tok = lexer.NextToken();
  while (tok.type != TokenType::END_OF_FILE) {
    parts.push_back({tok.literal, ColorForToken(tok.type)});
    tok = lexer.NextToken();
  }

  return parts;
}

Elements BuildLineWithCursor(const std::string &line, int cursor_col,
                             bool insert_mode) {
  auto tokens = TokenizeWithColors(line);
  Elements result;
  int char_pos = 0;

  for (auto &[literal, col] : tokens) {
    int token_start = char_pos;
    int token_end = char_pos + literal.size();

    if (cursor_col >= token_start && cursor_col < token_end) {
      // Cursor lands inside this token — split it
      int local = cursor_col - token_start;
      std::string before = literal.substr(0, local);
      std::string cursor_char = std::string(1, literal[local]);
      std::string after = literal.substr(local + 1);

      if (!before.empty())
        result.push_back(text(before) | color(col));

      result.push_back(text(cursor_char) |
                       (insert_mode ? inverted : bgcolor(Color::White)));

      if (!after.empty())
        result.push_back(text(after) | color(col));
    } else {
      result.push_back(text(literal) | color(col));
    }

    char_pos = token_end;
  }

  // Cursor is past the end of all tokens (empty line or end of line)
  if (cursor_col >= char_pos) {
    result.push_back(text(" ") |
                     (insert_mode ? inverted : bgcolor(Color::White)));
  }

  return result;
}

Element HighlightLine(const std::string &line) {
  auto tokens = TokenizeWithColors(line);
  Elements parts;
  for (auto &[literal, col] : tokens)
    parts.push_back(text(literal) | color(col));
  return hbox(std::move(parts));
}

// Save file to file name
void SaveFile(std::vector<crope> &doc, const std::string &filepath) {
  if (filepath == "")
    return;

  std::ofstream file(filepath);

  if (!file.is_open())
    return;

  for (const crope &line : doc) {
    file << line.c_str() << '\n';
  }

  file.close();
}

void LoadFile(std::vector<crope> &doc, const std::string &filepath) {
  doc.clear();

  std::ifstream file(filepath);
  if (!file.is_open()) {
    doc.push_back("");
    return;
  }

  std::string line;
  while (getline(file, line)) {
    doc.push_back(line.c_str());
  }

  if (doc.empty())
    doc.push_back("");

  file.close();
}

int main(int argc, char *argv[]) {
  // Logic
  std::string current_file = "untitled.soph";
  std::vector<crope> document;
  std::vector<crope> copyReg;
  std::vector<std::string> terminal_doc;
  crope command_line;
  document.push_back("");

  // Terminal Proc
  LiveProcess proc;
  bool show_terminal = false;
  bool terminal_focused = false;
  int current_term_line = 0;

  // Variables
  int scroll_speed = 2;
  int scroll_offset = 0;
  int visible_lines = 30;

  int current_line = 0;
  int current_col = 0;
  int selection_start_line = 0;
  int selection_start_col = 0;
  int selection_end_line = 0;
  int selection_end_col = 0;

  int current_cmd = NONE;

  bool file_saved = false;
  bool current_mode = NORMAL;
  bool is_dragging = false;
  bool file_input = false;

  // Load file if filepath is passed in
  if (argc > 1) {
    LoadFile(document, argv[1]);
    current_file = argv[1];
    file_saved = true;
  } else {
    LoadFile(document, current_file);
    file_saved = true;
  }

  // Rendering
  auto screen = ScreenInteractive::Fullscreen();
  auto editor_area = Renderer([&] {
    // Get the right number of visible lines for a
    // certain term size and when resizing
    int terminal_height = Terminal::Size().dimy;
    visible_lines = terminal_height - 6;

    Elements visible;
    int start = scroll_offset;
    int end = std::min(scroll_offset + visible_lines, (int)document.size());

    int max_line_number = document.size();
    int line_number_width = std::to_string(max_line_number).length();

    if (is_dragging) {
      for (int i = start; i < end; i++) {
        std::string line = document[i].c_str();
        int line_num = i + 1;
        std::string line_num_str = std::to_string(line_num);
        std::string padded =
            std::string(line_number_width - line_num_str.length(), ' ') +
            line_num_str + " ";

        // Check if this line is in selection range
        bool is_selected_line = false;
        int sel_start = 0;
        int sel_end = line.length();

        if (selection_start_line >= 0 && selection_end_line >= 0) {
          int min_line = std::min(selection_start_line, selection_end_line);
          int max_line = std::max(selection_start_line, selection_end_line);

          if (i >= min_line && i <= max_line) {
            is_selected_line = true;

            // diddy blud shit
            if (i == min_line && i == max_line) {
              sel_start = std::min(selection_start_col, selection_end_col);
              sel_end = std::max(selection_start_col, selection_end_col);
            } else if (i == min_line) {
              sel_start = (selection_start_line < selection_end_line)
                              ? selection_start_col
                              : selection_end_col;
              sel_end = line.length();
            } else if (i == max_line) {
              sel_start = 0;
              sel_end = (selection_start_line < selection_end_line)
                            ? selection_end_col
                            : selection_start_col;
            } else {
              sel_start = 0;
              sel_end = line.length();
            }
          }
        }

        // Build line with selection highlighting
        if (is_selected_line && sel_start < sel_end) {
          std::string before = line.substr(0, sel_start);
          std::string selected = line.substr(sel_start, sel_end - sel_start);
          std::string after =
              (sel_end < line.length()) ? line.substr(sel_end) : "";

          Elements line_elements = {text(padded) | color(Color::GrayDark)};
          auto parts = BuildLineWithCursor(line, current_col, current_mode) |
                       bgcolor(Color::NavajoWhite1);

          line_elements.insert(line_elements.end(), parts.begin(), parts.end());

          visible.push_back(hbox(std::move(line_elements)));
        } else {
          visible.push_back(hbox({HighlightLine(padded), HighlightLine(line)}));
        }
      }
    } else {
      for (int i = start; i < end; i++) {
        int line_num = i + 1;
        std::string line_num_str = std::to_string(line_num);
        std::string padded =
            std::string(line_number_width - line_num_str.length(), ' ') +
            line_num_str + " ";

        if (i == current_line) {
          std::string line = document[i].c_str();
          Elements line_elements = {text(padded) | color(Color::GrayLight)};

          auto parts = BuildLineWithCursor(line, current_col, current_mode);
          line_elements.insert(line_elements.end(), parts.begin(), parts.end());

          visible.push_back(hbox(std::move(line_elements)));
        } else {
          // Regular line without cursor
          std::string line = document[i].c_str();

          visible.push_back(hbox(
              {text(padded) | color(Color::GrayDark), HighlightLine(line)}));
        }
      }
    }

    // Layout
    return vbox({// Top Bar
                 hbox({
                     text((std::string) "Editing " + current_file + "...") |
                         bold,
                     filler(),
                     text("File(1) | Edit(2) | Theme(3)"),
                 }),

                 vbox({separatorLight(), vbox(visible) | flex, filler(),
                       separatorLight()}) |
                     flex_grow,

                 hbox({
                     !file_input
                         ? text("Status: " + std::string(file_saved
                                                             ? "Saved!"
                                                             : "Not Saved!")) |
                               size(WIDTH, EQUAL, 20)
                         : text((std::string) ":" + command_line.c_str()) |
                               size(WIDTH, EQUAL, 20) | color(Color::Red),
                     text("Mode:" +
                          std::string(current_mode ? "Insert" : "Normal")) |
                         center | flex,
                     filler() | size(WIDTH, EQUAL, 5),
                     text("Ln " + std::to_string(current_line + 1) + " Col " +
                          std::to_string(current_col + 1)) |
                         size(WIDTH, EQUAL, 15) | align_right,
                 })}) |
           border;
  });

  auto clamp_cursor_to_visible = [&]() {
    // Cursor is above visible area - move to top line
    if (current_line < scroll_offset) {
      current_line = scroll_offset;
    }

    // Cursor is below visible area - move to bottom line
    if (current_line >= scroll_offset + visible_lines) {
      current_line = scroll_offset + visible_lines - 1;
    }

    // Also clamp to document bounds
    current_line =
        std::max(0, std::min(current_line, (int)document.size() - 1));
  };

  int terminal_scroll_offset = 0;

  auto terminal_component = Renderer([&] {
    auto new_lines = proc.FetchOutput();

    int term_height = Terminal::Size().dimy - 6;

    bool was_at_bottom =
        terminal_doc.empty() ||
        (terminal_scroll_offset + term_height >= (int)terminal_doc.size());

    if (was_at_bottom) {
      terminal_scroll_offset =
          std::max(0, (int)terminal_doc.size() - term_height);
    }

    // Split new output by '\n' and append
    for (auto &line : new_lines) {
      size_t start = 0;
      size_t pos;
      while ((pos = line.find('\n', start)) != std::string::npos) {
        terminal_doc.push_back(line.substr(start, pos - start));
        start = pos + 1;
        current_term_line++;
      }
      if (start < line.size())
        terminal_doc.push_back(line.substr(start));
    }

    if (was_at_bottom) {
      terminal_scroll_offset =
          std::max(0, (int)terminal_doc.size() - term_height);
    }

    Elements elements;
    int start_line = terminal_scroll_offset;
    int end_line = std::min((int)terminal_doc.size(), start_line + term_height);

    for (int i = start_line; i < end_line; i++)
      elements.push_back(text(terminal_doc[i].c_str()));

    return vbox({text(" Soph Terminal - Output ") | bold | center, separator(),
                 vbox(std::move(elements)) | flex, separator(),
                 text((std::string) "Terminal ID: " +
                      std::to_string(proc.pid))}) |
           border | size(WIDTH, EQUAL, term_height + 10);
  });

  Component main_screen =
      Container::Horizontal({editor_area, terminal_component});

  main_screen = Renderer(main_screen, [&] {
    Element editor_render = editor_area->Render();

    if (!show_terminal)
      return editor_render;

    return hbox({editor_render | flex, terminal_component->Render()});
  });

  main_screen |= CatchEvent([&](Event event) {
    // ── INSERT MODE ─────────────────────────────────────────────
    if (current_mode == INSERT) {
      if (event == Event::Escape) {
        current_mode = NORMAL;
        return true;
      }
      if (event == Event::Return) {
        std::string current = document[current_line].c_str();
        std::string before = current.substr(0, current_col);
        std::string after = current.substr(current_col);
        document[current_line] = before.c_str();

        std::string indentation;
        for (char c : before) {
          if (c == ' ')
            indentation += ' ';
          else
            break;
        }
        size_t last_char_pos = before.find_last_not_of(" \t\r\n");
        if (last_char_pos != std::string::npos && before[last_char_pos] == '{')
          indentation += std::string(4, ' ');

        document.insert(document.begin() + current_line + 1,
                        (indentation + after).c_str());
        current_line++;
        current_col = indentation.length();
        file_saved = false;
        return true;
      }
      if (event == Event::Tab) {
        document[current_line].insert(current_col, std::string(4, ' ').c_str());
        current_col += 4;
        file_saved = false;
        return true;
      }
      if (event == Event::Backspace) {
        if (current_col > 0) {
          document[current_line].erase(current_col - 1, 1);
          current_col--;
        } else if (current_line > 0) {
          current_col = document[current_line - 1].length();
          document[current_line - 1].append(document[current_line].c_str());
          document.erase(document.begin() + current_line);
          current_line--;
        }
        file_saved = false;
        return true;
      }
      if (!event.character().empty() &&
          (unsigned char)event.character()[0] >= 32 &&
          event.character()[0] != 127) {
        document[current_line].insert(current_col, event.character().c_str());
        current_col++;
        file_saved = false;
        return true;
      }
    }

    else if (current_mode == NORMAL) {
      if (current_cmd == NONE) {

        if (terminal_focused && proc.running) {
          if (event.is_mouse()) {
            if (event.mouse().button == Mouse::WheelDown) {
              terminal_scroll_offset =
                  std::min(terminal_scroll_offset + 1,
                           std::max(0, (int)terminal_doc.size() - 30));
              return true;
            }
            if (event.mouse().button == Mouse::WheelUp) {
              terminal_scroll_offset = std::max(0, terminal_scroll_offset - 1);
              return true;
            }
          }

          if (event == Event::ArrowUp) {
            terminal_scroll_offset = std::max(0, terminal_scroll_offset - 1);
            return true;
          }

          if (event == Event::ArrowDown) {
            terminal_scroll_offset =
                std::min(terminal_scroll_offset + 1,
                         std::max(0, (int)terminal_doc.size() - 30));
            return true;
          }

          if (event.is_character()) {
            proc.WriteInput(event.character().c_str());
            terminal_doc[current_term_line].append(event.character());
            return true;
          }

          if (event == Event::Return) {
            proc.WriteInput("\n");
            current_term_line++;
            return true;
          }
          if (event == Event::Backspace) {
            proc.WriteInput("\b");
            terminal_doc[current_term_line].pop_back();
            return true;
          }
        } else {
          if (event.is_mouse()) {
            if (event.mouse().button == Mouse::WheelDown) {
              scroll_offset =
                  std::min(scroll_offset + scroll_speed,
                           std::max(0, (int)document.size() - visible_lines));
              clamp_cursor_to_visible();
              return true;
            }

            if (event.mouse().button == Mouse::WheelUp) {
              scroll_offset = std::max(0, scroll_offset - scroll_speed);
              clamp_cursor_to_visible();
              return true;
            }

            if (event.mouse().button == Mouse::Left) {
              int mouse_y = event.mouse().y;
              int mouse_x = event.mouse().x;

              int clicked_line = mouse_y - 3 + scroll_offset;

              // Clamp to valid range
              if (clicked_line >= 0 && clicked_line < document.size()) {
                current_line = clicked_line;
                current_col = mouse_x - 6; // 6 for idk bordering i guess
              }
              return true;
            }
          }

          if (event == Event::Character('i')) {
            current_mode = INSERT;
            return true;
          }

          if (event == Event::Character('v')) {
            selection_start_line = selection_end_line = current_line;
            selection_start_col = selection_end_col = current_col;
            is_dragging = true;
            return true;
          }

          // Paste whats in the copy reg
          if (event == Event::Character('p')) {
            crope temp = document[current_line].substr(
                current_col, document[current_line].length());
            document[current_line].erase(current_col,
                                         document[current_line].length());
            for (int i = 0; i < copyReg.size(); i++) {
              if (i == 0)
                document[current_line].insert(current_col, copyReg[i]);
              else {
                document.insert(document.begin() + current_line + i,
                                copyReg[i]);
              }
            }

            document[current_line + copyReg.size() - 1].append(temp.c_str());

            file_saved = false;
            return true;
          }

          // Selection Options
          if (is_dragging) {
            int step = (selection_end_line > selection_start_line) ? 1 : -1;

            int start = selection_start_line;
            int end = selection_end_line;

            if (event == Event::Character('d')) {
              if (start > end) {
                std::swap(start, end);
                std::swap(selection_start_col, selection_end_col);
              }

              if (start == end) {
                // Single line selection
                int col_start =
                    std::min(selection_start_col, selection_end_col);
                int col_end = std::max(selection_start_col, selection_end_col);
                document[start].erase(col_start, col_end - col_start);
                current_col = col_start;
              } else {
                // Keep the part of the first line before selection
                // and the part of the last line after selection, then join them
                std::string keep_before =
                    document[start].substr(0, selection_start_col).c_str();
                std::string keep_after =
                    document[end].substr(selection_end_col).c_str();

                // Delete all lines in range except start
                document.erase(document.begin() + start + 1,
                               document.begin() + end + 1);

                // Now join the two surviving halves on the start line
                document[start] = (keep_before + keep_after).c_str();

                current_col = selection_start_col;
              }

              current_line = start;
              // Clamp in case we deleted down to fewer lines
              current_line = std::min(current_line, (int)document.size() - 1);
              current_col =
                  std::min(current_col, (int)document[current_line].length());

              file_saved = false;
              is_dragging = false;
              return true;
            }

            if (event == Event::Character('y')) {
              copyReg.clear();

              if (start > end) {
                std::swap(start, end);
                // don't swap cols — instead derive them from which line is
                // which
              }

              int top_col = (selection_start_line < selection_end_line)
                                ? selection_start_col
                                : selection_end_col;

              int bottom_col = (selection_start_line < selection_end_line)
                                   ? selection_end_col
                                   : selection_start_col;

              if (start == end) {
                int col_start =
                    std::min(selection_start_col, selection_end_col);
                int col_end = std::max(selection_start_col, selection_end_col);
                copyReg.push_back(
                    document[start].substr(col_start, col_end - col_start));
              } else {
                for (int i = start; i <= end; ++i) {
                  if (i == start)
                    copyReg.push_back(document[i].substr(top_col));
                  else if (i == end)
                    copyReg.push_back(document[i].substr(0, bottom_col));
                  else
                    copyReg.push_back(document[i]);
                }
              }

              is_dragging = false;
              return true;
            }
          }

          if (event == Event::Escape) {
            is_dragging = false;
            return true;
          }
        }

      } else if (current_cmd > NONE) {
        if (event.is_character()) {
          command_line.append(event.character().c_str());
          return true;
        }

        if (event == Event::Backspace) {
          if (command_line.length() > 0)
            command_line.pop_back();
          return true;
        }

        if (current_cmd == OPENFILE) {
          if (event == Event::Return) {
            current_cmd = NONE;
            file_input = false;
            LoadFile(document, command_line.c_str());
            current_file = command_line.c_str();
            current_line = current_col = 0;
            command_line.clear();
            return true;
          }
        } else if (current_cmd == RENAME) {
          if (event == Event::Return) {
            current_cmd = NONE;
            file_input = false;
            current_file = command_line.c_str();
            command_line.clear();
            return true;
          }
        }

        if (event == Event::Escape) {
          current_cmd = NONE;
          file_input = false;
          return true;
        }
      }
    }

    if (!terminal_focused) {
      if (event == Event::ArrowUp) {
        if (current_line > 0) {
          current_line--;
          if (current_col > document[current_line].length())
            current_col = document[current_line].length();

          if (current_line < scroll_offset) {
            scroll_offset--;
          }
        }

        if (current_mode == NORMAL) {
          selection_end_line = current_line;
          selection_end_col = current_col;
        }

        return true;
      }

      if (event == Event::ArrowDown) {
        // Update Scroll Position
        if (current_line < document.size() - 1) {
          current_line++;
          if (current_col > document[current_line].length())
            current_col = document[current_line].length();

          if (current_line >= scroll_offset + visible_lines) {
            scroll_offset++;
          }
        }

        if (current_mode == NORMAL) {
          selection_end_line = current_line;
          selection_end_col = current_col;
        }

        return true;
      }

      if (event == Event::ArrowRight) {
        if (current_col < document[current_line].length()) {
          current_col++;
        }

        if (current_mode == NORMAL) {
          selection_end_col = current_col;
        }
        return true;
      }

      if (event == Event::ArrowLeft) {
        if (current_col > 0) {
          current_col--;
        }

        if (current_mode == NORMAL) {
          selection_end_col = current_col;
        }
        return true;
      }
    }

    if (event == Event::CtrlS) {
      SaveFile(document, current_file);
      file_saved = true;
      return true;
    }

    if (event == Event::CtrlO) {
      file_input = true;
      current_cmd = OPENFILE;
      return true;
    }

    if (event == Event::CtrlR) {
      file_input = true;
      current_cmd = RENAME;
      command_line.clear();
      command_line.append(current_file.c_str());
      return true;
    }

    if (event == Event::CtrlB) {
      show_terminal = true;
      current_mode = NORMAL;
      terminal_doc.clear();
      terminal_focused = true;
      current_term_line = 0;
      proc.Start("soph", {current_file},
                 [&screen] { screen.PostEvent(Event::Custom); });
      return true;
    }

    if (event == Event::CtrlT) {
      show_terminal = !show_terminal;
      terminal_focused = !terminal_focused;
      current_mode = NORMAL;
      return true;
    }

    return false;
  });

  screen.Loop(main_screen);

  proc.Stop();
  return 0;
}
