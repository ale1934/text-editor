#include <algorithm>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/screen/string.hpp>
#include <vector>
#include <ext/rope>
#include <fstream>

#define NORMAL false
#define INSERT true

using namespace __gnu_cxx;
using namespace ftxui;

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

// Load file from file path and add each string to document
void LoadFile(std::vector<crope> &doc, const std::string &filepath) {
  doc.clear(); // Clear past document first

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
  std::string current_file = "untitled.txt";
  std::vector<crope> document;
  document.push_back(""); // Make sure document isnt empty
                          
  // Variables
  int scroll_speed = 2;
  int scroll_offset = 0;
  int visible_lines = 30;

  int current_line = 0;
  int current_col = 0;

  int side_bar_page = 0;

  bool file_saved = false;
  bool current_mode = NORMAL;
  bool side_bar_open = false;

  // Load file if filepath is passed in
  if (argc > 1) {
    LoadFile(document, argv[1]);
    current_file = argv[1];
    file_saved = true;
  }

  // Rendering
  auto screen = ScreenInteractive::Fullscreen();
  auto renderer = Renderer([&] {
    // Get the right number of visible lines for a 
    // certain term size and when resizing
    int terminal_height = Terminal::Size().dimy;
    visible_lines = terminal_height - 6;

    Elements visible;
    int start = scroll_offset;
    int end = std::min(scroll_offset + visible_lines, (int)document.size());

    int max_line_number = document.size();
    int line_number_width = std::to_string(max_line_number).length();
    
    for (int i = start; i < end; i++) {
      int line_num = i + 1;  
      std::string line_num_str = std::to_string(line_num);
      std::string padded = std::string(line_number_width - 
          line_num_str.length(), ' ') + line_num_str + ". ";

      if (i == current_line) {
        // This is the line with the cursor
        std::string line = document[i].c_str();
        std::string before = line.substr(0, current_col);
        std::string cursor_char = current_col < line.length() 
                              ? std::string(1, line[current_col]) 
                              : " ";
        std::string after = current_col < line.length() 
                       ? line.substr(current_col + 1) 
                       : "";
    
        visible.push_back(
          hbox({
            text(padded) | color(Color::GrayLight),
            text(before) | color(Color::LightSkyBlue1),
            text(cursor_char) | inverted,
            text(after) | color(Color::LightSkyBlue1)
          })
        );
      } else {
        // Regular line without cursor
        visible.push_back(
          hbox({
            text(padded) | color(Color::GrayDark),
            text(document[i].c_str()) | color(Color::Blue)
          })
        );
      }
    }

    Elements sidebar;
    if (side_bar_open) {
      sidebar.push_back(
        vbox({
          text(std::to_string(side_bar_page))
        }) | flex
      );
    } else 
      sidebar.clear();

    // Layout
    return vbox({
      // Top Bar
      hbox({
        text((std::string)"Editing " + current_file + "...") | bold,
        filler(),
        text("File(1) | Edit(2) | View(3)"),
      }),
      
      vbox({
        separatorLight(),
        hbox({
          vbox(visible) | flex, 
          filler(),
          side_bar_open ? separatorLight() : emptyElement(),
          side_bar_open ? vbox(sidebar) | flex : emptyElement()
        }),
        separatorLight()
      }) | flex, 
      
      hbox({
        text("Status: " + std::string(file_saved ? "Saved!" : "Not Saved!")) 
         | size(WIDTH, EQUAL, 20),
        text("Mode:" + std::string(current_mode ? 
          "Insert" : "Normal")) 
          | center | flex,
        filler() | size(WIDTH, EQUAL, 5),
        text("Ln " + std::to_string(current_line + 1) + 
          " Col " + std::to_string(current_col + 1)) 
          | size(WIDTH, EQUAL, 15) | align_right,
      })
    }) | border;
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
    current_line = std::max(0, std::min(current_line, 
          (int)document.size() - 1));
  };

  renderer |= CatchEvent([&](Event event) {
    if (current_mode == INSERT) {
      if (event.is_character()) {
        document[current_line].insert(current_col, event.character().c_str());
        current_col++;
        file_saved = false;
        return true;
      }

      if (event == Event::Return) {
        if (document[current_line].length() == current_col)
          document.insert(document.begin() + current_line+1, "");
        else {
          document.insert(document.begin() + current_line+1, 
              document[current_line].substr(current_col,
                document[current_line].length() - current_col));
          document[current_line].erase(current_col, 
              document[current_line].length());
        }

        current_col = 0;
        current_line++;
      }
    
      if (event == Event::Backspace) {
        if (current_col > 0) {
          // Delete character in current line
          document[current_line].erase(current_col - 1, 1);
          current_col--;
        } else if (current_line > 0) {
          // At beginning of line - merge with previous line
          current_col = document[current_line - 1].length();
          
          // Append current line to previous line
          document[current_line - 1].append(document[current_line].c_str());
          
          // Delete current line
          document.erase(document.begin() + current_line);
          current_line--;
        }
        
        file_saved = false;
        return true;
      }

      if (event == Event::Escape) {
        current_mode = NORMAL;
      }
    } else {
      if (event == Event::i) {
        current_mode = INSERT;
      }

      if (event == Event::Character('1') ||
          event == Event::Character('2') ||
          event == Event::Character('3')) {
        side_bar_open = true;
        side_bar_page = stoi(event.character());
      }

      if (event == Event::Escape) {
        side_bar_open = false;
      }
    }
    
    if (event.is_mouse()) {
      if (event.mouse().button == Mouse::WheelDown) {
        scroll_offset = std::min(scroll_offset + scroll_speed, 
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
        
        // Account for top bar (assuming 3 line for top bar)
        int clicked_line = mouse_y - 3 + scroll_offset;
        
        // Clamp to valid range
        if (clicked_line >= 0 && clicked_line < document.size()) {
          current_line = clicked_line;
          current_col = mouse_x - 6; // 6 for idk bordering i guess
        }
        return true;
      }
    }

    if (event == Event::ArrowUp) {
      if (current_line > 0) {
        current_line--;
        if (current_col > document[current_line].length())
          current_col = document[current_line].length();

        if (current_line < scroll_offset) {
          scroll_offset--;
        }
      }
      return true;
    }

    if (event == Event::ArrowDown) {
      // Update Scroll Position
      if (current_line < document.size()-1) {
        current_line++;        
        if (current_col > document[current_line].length())
          current_col = document[current_line].length();

        if (current_line >= scroll_offset + visible_lines) {
          scroll_offset++;
        }
      }
      return true;
    }
  
    if (event == Event::ArrowRight) {
      if (current_col < document[current_line].length()) {
        current_col++;
      }
      return true;
    }

    if (event == Event::ArrowLeft) {
      if (current_col > 0) {
        current_col--;
      }
      return true;
    }

    if (event == Event::CtrlS) {
      SaveFile(document, current_file);
      file_saved = true;
    }

    return false;
  });
  
  screen.Loop(renderer);
  return 0;
}
