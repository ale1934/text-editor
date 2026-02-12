cmake_minimum_required(VERSION 3.23)
project(editor)

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG main
)
FetchContent_MakeAvailable(ftxui)

add_executable(soph-editor src/main.cpp src/lexer.cpp src/token.cpp)
set_target_properties(soph-editor PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../bin/")
target_link_libraries(soph-editor
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component
)
