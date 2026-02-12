#include "lexer.h"
#include <cassert>
#include <iostream>
#include <vector>

bool isLetter(char b) {
  return 'a' <= b && b <= 'z' || 'A' <= b && b <= 'Z' || b == '_';
}

bool isDigit(char b) { return '0' <= b && b <= '9'; }

Lexer::Lexer(const std::string &input)
    : input(input), position(0), readPosition(0), ch(0) {
  ReadChar();
}

Token Lexer::NextToken() {
  if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
    std::string literal;
    while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
      literal += ch;
      ReadChar();
    }
    return Token(TokenType::WHITESPACE, literal);
  }

  switch (ch) {
  case '"': {
    std::string literal = ReadString();
    ReadChar();
    return Token(TokenType::STRING, literal);
  }
  case '=':
    if (PeekChar() == '=') {
      char c = ch;
      ReadChar();
      ReadChar();
      return Token(TokenType::EQ, "==");
    } else {
      ReadChar();
      return Token(TokenType::ASSIGN, "=");
    }
  case '!':
    if (PeekChar() == '=') {
      char c = ch;
      ReadChar();
      ReadChar();
      return Token(TokenType::NEQ, "!=");
    } else {
      ReadChar();
      return Token(TokenType::BANG, "!");
    }
  case ';':
    ReadChar();
    return Token(TokenType::SEMICOLON, ";");
  case '(':
    ReadChar();
    return Token(TokenType::LPAREN, "(");
  case ')':
    ReadChar();
    return Token(TokenType::RPAREN, ")");
  case ',':
    ReadChar();
    return Token(TokenType::COMMA, ",");
  case '+':
    ReadChar();
    return Token(TokenType::PLUS, "+");
  case '-':
    ReadChar();
    return Token(TokenType::MINUS, "-");
  case '/':
    if (PeekChar() == '/') {
      SkipLineComment();
      return NextToken();
    } else {
      ReadChar();
      return Token(TokenType::SLASH, "/");
    }
  case '*':
    ReadChar();
    return Token(TokenType::ASTERISK, "*");
  case '<':
    if (PeekChar() == '=') {
      char c = ch;
      ReadChar();
      ReadChar();
      return Token(TokenType::LTE, "<=");
    } else {
      ReadChar();
      return Token(TokenType::LT, "<");
    }
  case '>':
    if (PeekChar() == '=') {
      char c = ch;
      ReadChar();
      ReadChar();
      return Token(TokenType::GTE, ">=");
    } else {
      ReadChar();
      return Token(TokenType::GT, ">");
    }
  case '{':
    ReadChar();
    return Token(TokenType::LBRACE, "{");
  case '}':
    ReadChar();
    return Token(TokenType::RBRACE, "}");
  case '@':
    ReadChar();
    return Token(TokenType::FUNCTION, "@");
  case 0:
    return Token(TokenType::END_OF_FILE, "");
  default:
    if (isLetter(ch)) {
      std::string literal = ReadIdentifier();
      TokenType type = LookupIdent(literal);
      return Token(type, literal);
    } else if (isDigit(ch)) {
      return Token(TokenType::INT, ReadNumber());
    } else {
      Token tok(TokenType::ILLEGAL, std::string(1, ch));
      ReadChar();
      return tok;
    }
  }
}

std::string Lexer::ReadString() {
  std::string result = "\"";

  while (true) {
    ReadChar();

    if (ch == '"' || ch == 0) {
      break;
    }

    if (ch == '\\') {
      ReadChar();
      if (ch == 'n')
        result += "\\n"; // keep as literal \n
      else if (ch == 't')
        result += "\\t"; // keep as literal \t
      else if (ch == '\\')
        result += "\\\\"; // escaped backslash
      else if (ch == '"')
        result += "\\\""; // escaped quote
      else {
        result += '\\';
        result += ch;
      } // unknown escape, preserve both chars
    } else {
      result += ch;
    }
  }

  result += "\"";
  return result;
}

void Lexer::SkipWhiteSpace() {
  while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
    ReadChar();
}

char Lexer::PeekChar() {
  if (readPosition >= input.length())
    return 0;
  else
    return input[readPosition];
}

std::string Lexer::ReadIdentifier() {
  size_t startPos = position;
  while (isLetter(ch)) {
    ReadChar();
  }
  return input.substr(startPos, position - startPos);
}

std::string Lexer::ReadNumber() {
  size_t startPos = position;
  while (isDigit(ch)) {
    ReadChar();
  }
  return input.substr(startPos, position - startPos);
}

void Lexer::ReadChar() {
  if (readPosition >= input.size()) {
    ch = 0;
  } else {
    ch = input[readPosition];
  }

  position = readPosition;
  readPosition++;
}

void Lexer::SkipLineComment() {
  while (ch != '\n' && ch != 0) {
    ReadChar();
  }
}
