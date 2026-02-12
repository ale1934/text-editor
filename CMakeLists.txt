cmake_minimum_required(VERSION 3.23)
project(editor)

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG main
)
FetchContent_MakeAvailable(ftxui)

add_executable(sedit src/main.cpp src/lexer.cpp src/token.cpp)
target_link_libraries(sedit
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
)
