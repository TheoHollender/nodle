
#include "nodle/parser/context.hpp"

using namespace nodle::parser;

ParserContext::ParserContext (std::vector<nodle::lexer::Token> tokens)
    : tokens_(std::move(tokens)) {}

bool ParserContext::remaining () const {
    return offset < tokens_.size();
}

nodle::lexer::Token ParserContext::peek () const {
    return tokens_[offset];
}
void ParserContext::pop () {
    offset ++;
}