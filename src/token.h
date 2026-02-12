#pragma once
#include <string>

enum class TokenType {
  // Special
  ILLEGAL,
  END_OF_FILE,
  COMMENT,
  WHITESPACE,

  // Identifiers + literals
  IDENT,
  INT,

  // Operators
  ASSIGN,
  PLUS,
  MINUS,
  BANG,
  ASTERISK,
  SLASH,

  EQ,
  NEQ,
  LT,
  GT,
  LTE,
  GTE,

  // Delimiters
  COMMA,
  SEMICOLON,
  LPAREN,
  RPAREN,
  LBRACE,
  RBRACE,

  // Keywords
  FUNCTION,
  RETURN,
  VAR,
  IF,
  ELSE,
  TRUE,
  FALSE,
  FOR,
  WHILE
};

inline std::string TokenTypeToString(TokenType type) {
  switch (type) {
  // Special
  case TokenType::ILLEGAL:
    return "ILLEGAL";
  case TokenType::END_OF_FILE:
    return "EOF";
  case TokenType::COMMENT:
    return "COMMENT";

  // Identifiers + literals
  case TokenType::IDENT:
    return "IDENT";
  case TokenType::INT:
    return "INT";

  // Operators
  case TokenType::ASSIGN:
    return "ASSIGN";
  case TokenType::PLUS:
    return "PLUS";
  case TokenType::MINUS:
    return "MINUS";
  case TokenType::BANG:
    return "BANG";
  case TokenType::ASTERISK:
    return "ASTERISK";
  case TokenType::SLASH:
    return "SLASH";

  case TokenType::EQ:
    return "EQ";
  case TokenType::NEQ:
    return "NEQ";
  case TokenType::LT:
    return "LT";
  case TokenType::GT:
    return "GT";
  case TokenType::LTE:
    return "LTE";
  case TokenType::GTE:
    return "GTE";

  // Delimiters
  case TokenType::COMMA:
    return "COMMA";
  case TokenType::SEMICOLON:
    return "SEMICOLON";
  case TokenType::LPAREN:
    return "LPAREN";
  case TokenType::RPAREN:
    return "RPAREN";
  case TokenType::LBRACE:
    return "LBRACE";
  case TokenType::RBRACE:
    return "RBRACE";

  // Keywords
  case TokenType::FUNCTION:
    return "FUNCTION";
  case TokenType::RETURN:
    return "RETURN";
  case TokenType::VAR:
    return "VAR";
  case TokenType::IF:
    return "IF";
  case TokenType::ELSE:
    return "ELSE";
  case TokenType::TRUE:
    return "TRUE";
  case TokenType::FALSE:
    return "FALSE";
  case TokenType::FOR:
    return "FOR";
  case TokenType::WHILE:
    return "WHILE";
  }
  return "UNKNOWN";
}

struct Token {
  TokenType type;
  std::string literal;

  Token(TokenType t, const std::string &lit) : type(t), literal(lit) {}
};

TokenType LookupIdent(const std::string &ident);
