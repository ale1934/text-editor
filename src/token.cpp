#include "token.h"
#include <unordered_map>

static const std::unordered_map<std::string, TokenType> keywords = {
    {"@", TokenType::FUNCTION},    {"var", TokenType::VAR},
    {"return", TokenType::RETURN}, {"if", TokenType::IF},
    {"else", TokenType::ELSE},     {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},   {"for", TokenType::FOR},
    {"while", TokenType::WHILE},   {"exit", TokenType::EXIT}};

TokenType LookupIdent(const std::string &ident) {
  auto it = keywords.find(ident);

  if (it != keywords.end()) {
    return it->second;
  }

  return TokenType::IDENT;
}
