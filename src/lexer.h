#pragma once
#include "token.h"

class Lexer {
private:
  std::string input;
  int position;
  int readPosition;
  char ch;

  void ReadChar();
  void SkipWhiteSpace();
  void SkipLineComment();
  std::string ReadIdentifier();
  std::string ReadNumber();
  char PeekChar();

public:
  explicit Lexer(const std::string &input);
  Token NextToken();
};
