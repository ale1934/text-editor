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

// Custom Button
ButtonOption Style() {
  auto option = ButtonOption::Animated();
  option.transform = [](const EntryState& s) {
    auto element = text(s.label);
    if (s.focused) {
      element |= bold;
    }
    return element | center | borderEmpty | flex;
  };
  return option;
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
  int selection_start_line = 0;
  int selection_start_col = 0;
  int selection_end_line = 0;
  int selection_end_col = 0;

  int side_bar_page = 0;

  bool file_saved = false;
  bool current_mode = NORMAL;
  bool side_bar_open = false;
  bool is_dragging = false;

  // Load file if filepath is passed in
  if (argc > 1) {
    LoadFile(document, argv[1]);
    current_file = argv[1];
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
        std::string line = document[i].c_str();int line_num = i + 1;  
        std::string line_num_str = std::to_string(line_num);
        std::string padded = std::string(line_number_width - 
            line_num_str.length(), ' ') + line_num_str + " ";
        
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
                          ? selection_start_col : selection_end_col;
              sel_end = line.length();
            } else if (i == max_line) {
              sel_start = 0;
              sel_end = (selection_start_line < selection_end_line) 
                        ? selection_end_col : selection_start_col;
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
          std::string after = (sel_end < line.length()) ? line.substr(sel_end) : "";
          
          visible.push_back(
            hbox({
              text(padded) | color(Color::GrayDark),
              text(before) | color(Color::Blue),
              text(selected) | inverted,  // Highlight selection
              text(after) | color(Color::Blue)
            })
          );
        } else {
          visible.push_back(
            hbox({
              text(padded) | color(Color::GrayDark),
              text(line) | color(Color::Blue)
            })
          );
        }
      }
    } else {
      for (int i = start; i < end; i++) {
        int line_num = i + 1;  
        std::string line_num_str = std::to_string(line_num);
        std::string padded = std::string(line_number_width - 
            line_num_str.length(), ' ') + line_num_str + " ";

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
              text(cursor_char) | (current_mode ? inverted : bgcolor(Color::White)),
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

    }
  
    // Layout
    return vbox({
      // Top Bar
      hbox({
        text((std::string)"Editing " + current_file + "...") | bold,
        filler(),
        text("File(1) | Edit(2) | Theme(3)"),
      }),
      
      vbox({
        separatorLight(),
        hbox({
          vbox(visible) | flex, 
          filler(),
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

  // File sidebar
  auto file_buttons = Container::Vertical({
    Button("Save (Ctrl+S)", [&] { 
      SaveFile(document, current_file); 
      file_saved = true;
    }),
    Button("Open (Ctrl+O)", [&] { 
    }),
    Button("Rename (Ctrl+R)", [&] { 
    }),
  });

  auto sidebar_switcher = Container::Tab(
    {file_buttons}, &side_bar_page  
  );

  auto sidebar_component = Renderer(sidebar_switcher, [&] {
    if (!side_bar_open) return emptyElement();
    
    std::string page_title;
    if (side_bar_page == 0) page_title = "File";
    else if (side_bar_page == 1) page_title = "Edit";
    else if (side_bar_page == 2) page_title = "Theme";
    
    return vbox({
      text(page_title) | bold | center,
      separator(),
      sidebar_switcher->Render()
    }) | border | size(WIDTH, EQUAL, 20);
  });

  auto editor_with_prio = Container::Vertical({
    editor_area
  });

  editor_with_prio |= CatchEvent([&](Event event) {
    // In insert mode, editor gets ALL keyboard input first
    if (current_mode == INSERT) {
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
        file_saved = false;
        return true;  // Consumed - won't reach sidebar
      }
      
      if (event.is_character()) {
        document[current_line].insert(current_col, event.character().c_str());
        current_col++;
        file_saved = false;
        return true;  // Consumed
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
      
      if (event == Event::Escape) {
        current_mode = NORMAL;
        return true;
      }
    }

    return false;
  });

  auto main_screen = Container::Horizontal({
    editor_with_prio | flex,
    sidebar_component
  });

  main_screen |= CatchEvent([&](Event event) {
    if (current_mode == NORMAL) {
      if (event == Event::Character('i')) {
        current_mode = INSERT;
        return true;
      }

      if (event == Event::v) {
        selection_start_line = selection_end_line = current_line;
        selection_start_col = selection_end_col = current_col;
        is_dragging = true;
      }

      if (event == Event::Character('1') ||
          event == Event::Character('2') ||
          event == Event::Character('3')) {
        side_bar_open = true;
        side_bar_page = stoi(event.character());
      }

      if (event == Event::Escape) {
        side_bar_open = false;
        is_dragging = false;
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

      if (current_mode == NORMAL) {
        selection_end_line = current_line;
        selection_end_col = current_col;
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
    }  // File sidebar

    if (event == Event::ArrowLeft) {
      if (current_col > 0) {
        current_col--;
      }

      if (current_mode == NORMAL) {
        selection_end_col = current_col;
      }
      return true;
    }

    if (event == Event::CtrlS) {
      SaveFile(document, current_file);
      file_saved = true;
    }

    return false;
  });
  
  screen.Loop(main_screen);
  return 0;
}
