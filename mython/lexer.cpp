#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <cassert>
#include <iostream>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

namespace detail {
    std::string ReadWord(std::istream& input) {
        char symbol;
        string parsed_word;
        while(input.get(symbol)) {
            if (isalnum(symbol) || symbol == '_') {
                parsed_word += symbol;
            } else {
                input.putback(symbol);
                break;
            }
        }
        return parsed_word;
    }
} // namespace detail

Lexer::Lexer(std::istream& input)
    : input_tokens_(input) {
    ParseTokens();
}

const Token& Lexer::CurrentToken() const {
    return tokens_[index_current_token_];
}

Token Lexer::NextToken() {
    if (index_current_token_ + 1 != tokens_.size()) {
        return tokens_[++index_current_token_];
    }
    return tokens_[index_current_token_];
}

// ========================= Begin Extracting Tokens =========================

void Lexer::ParseTokens() {
    RemovingSpacesBeforeTokens();
    while (input_tokens_) {
        ExtractKeywordOrId();
        ExtractOperatorEqOrSymbol();
        ExtractIntValue();
        ExtractString();
        RemovingSpacesBeforeTokens();
        ExtractComments();
        ExtractNewLine();
        ExtractDent();
    }
    // т.к. перед Eof всегда NewLine либо Dedent
    if (!tokens_.empty() && !tokens_.back().Is<token_type::Newline>() && !tokens_.back().Is<token_type::Dedent>()) {
        tokens_.emplace_back(token_type::Newline{});
    }
    tokens_.emplace_back(token_type::Eof{});
}

bool Lexer::IsEof() const {
    return input_tokens_.peek() == char_traits<char>::eof();
}

void Lexer::RemovingSpacesBeforeTokens() {
    while(input_tokens_.peek() == ' ') {
        input_tokens_.get();
    }
}

void Lexer::ExtractDent() {
    if (!tokens_.empty() && !tokens_.back().Is<token_type::Newline>()) {
        return;
    }
    // игнорируем пустую строку
    if (input_tokens_.peek() == '\n') {
        return;
    }

    uint16_t count_space = 0;
    while(input_tokens_.peek() == ' ') {
        input_tokens_.get();
        ++count_space;
    }
    // нечетное кол-во пробелов недопустимо
    if (count_space % num_space_dent_) {
        string num_space = to_string(count_space);
        throw LexerError("One of the indents contains an odd number of spaces. Num space = "s + num_space);
    }

    // разница в отступах
    int16_t delta_dent = count_space / num_space_dent_ - current_dent_;
    while (delta_dent > 0) {
        tokens_.emplace_back(token_type::Indent{});
        ++current_dent_;
        --delta_dent;
    }
    while (delta_dent < 0) {
        tokens_.emplace_back(token_type::Dedent{});
        assert(current_dent_ != 0);
        --current_dent_;
        delta_dent++;
    }
}

void Lexer::ExtractNewLine() {
    if (IsEof()) {
        return;
    }
    if (input_tokens_.peek() != '\n') {
        return;
    }
    input_tokens_.get();
    // если не было добавлено токенов или последний токен - "Newline", то
    // переход на следующую строку не учитываются
    if (!tokens_.empty() && !tokens_.back().Is<token_type::Newline>()) {
        tokens_.emplace_back(token_type::Newline{});
    }
}

bool Lexer::AddKeyword(const std::string& word) {
    size_t old_size = tokens_.size();

    if (word == "class"s) {
        tokens_.emplace_back(token_type::Class{});
    }
    if (word == "return"s) {
        tokens_.emplace_back(token_type::Return{});
    }
    if (word == "if"s) {
        tokens_.emplace_back(token_type::If{});
    }
    if (word == "else"s) {
        tokens_.emplace_back(token_type::Else{});
    }
    if (word == "def"s) {
        tokens_.emplace_back(token_type::Def{});
    }
    if (word == "print"s) {
        tokens_.emplace_back(token_type::Print{});
    }
    if (word == "or"s) {
        tokens_.emplace_back(token_type::Or{});
    }
    if (word == "None"s) {
        tokens_.emplace_back(token_type::None{});
    }
    if (word == "and"s) {
        tokens_.emplace_back(token_type::And{});
    }
    if (word == "not"s) {
        tokens_.emplace_back(token_type::Not{});
    }
    if (word == "True"s) {
        tokens_.emplace_back(token_type::True{});
    }
    if (word == "False"s) {
        tokens_.emplace_back(token_type::False{});
    }

    return old_size == tokens_.size() ? false : true;
}

void Lexer::ExtractKeywordOrId() {
    if (IsEof()) {
        return;
    }
    char symbol = input_tokens_.peek();
    if (!isalpha(symbol) && symbol != '_') {
        return;
    }

    string parsed_word = detail::ReadWord(input_tokens_);
    // Если не удалось добавить, то полученное слово - идентификатор
    if(!AddKeyword(parsed_word)) {
        tokens_.emplace_back(token_type::Id{ parsed_word });
    }
}

void Lexer::ExtractOperatorEqOrSymbol() {
    if (IsEof()) {
        return;
    }
    if (input_tokens_.peek() == '#') {
        return;
    }

    char first_symbol = input_tokens_.get();
    if (!ispunct(first_symbol) || first_symbol == '\"' || first_symbol == '\'') {
        input_tokens_.putback(first_symbol);
        return;
    }

    if (first_symbol == '!' && input_tokens_.peek() == '=') {
        input_tokens_.get();
        tokens_.emplace_back(token_type::NotEq{});
    } else if (first_symbol == '=' && input_tokens_.peek() == '=') {
        input_tokens_.get();
        tokens_.emplace_back(token_type::Eq{});
    } else if (first_symbol == '>' && input_tokens_.peek() == '=') {
        input_tokens_.get();
        tokens_.emplace_back(token_type::GreaterOrEq{});
    } else if (first_symbol == '<' && input_tokens_.peek() == '=') {
        input_tokens_.get();
        tokens_.emplace_back(token_type::LessOrEq{});
    } else {
        tokens_.emplace_back(token_type::Char{first_symbol});
    }
}

void Lexer::ExtractIntValue() {
    if (IsEof()) {
        return;
    }

    char symbol = input_tokens_.get();

    if (isdigit(symbol)) {
        string parsed_num{ symbol };
        while (input_tokens_.get(symbol)) {
            if (isdigit(symbol)) {
                parsed_num += symbol;
            } else {
                input_tokens_.putback(symbol);
                break;
            }
        }
        int num = stoi(parsed_num);
        tokens_.emplace_back(token_type::Number{ num });
    } else {
        input_tokens_.putback(symbol);
    }
}

void Lexer::ExtractString() {
    if (IsEof()) {
        return;
    }
    char begin_symbol = input_tokens_.get();
    if (begin_symbol != '\'' && begin_symbol != '\"') {
        input_tokens_.putback(begin_symbol);
        return;
    }

    char symbol;
    string str;
    while (input_tokens_.get(symbol)) {
        if (symbol == begin_symbol) {
            break;
        } else if (symbol == '\\') {
            char special_symbol;
            if (!input_tokens_.get(special_symbol)) {
                throw LexerError("The line was not closed"s);
            }
            switch (special_symbol) {
                case 'n':
                    str.push_back('\n');
                    break;
                case 't':
                    str.push_back('\t');
                    break;
                case 'r':
                    str.push_back('\r');
                    break;
                case '"':
                    str.push_back('"');
                    break;
                case '\\':
                    str.push_back('\\');
                    break;
                case '\'':
                    str.push_back('\'');
                    break;
                default:
                    throw LexerError("Unrecognized escape sequence \\"s + special_symbol);
                }
        } else if (symbol == '\n' || symbol == '\r') {
            // переход на следующу строку кода без закрытия текущего токена String
            throw LexerError("Unexpected end of line"s);
        } else {
            str.push_back(symbol);
        }
    }
    tokens_.emplace_back(token_type::String{str});
}

void Lexer::ExtractComments() {
    if (input_tokens_.peek() != '#') {
        return;
    }

    string comments;
    getline(input_tokens_, comments);
    // т.к. после комментария идет переход на новую строку
    if (!tokens_.empty() && !tokens_.back().Is<token_type::Newline>() && !tokens_.back().Is<token_type::Dedent>()) {
        tokens_.emplace_back(token_type::Newline{});
    }
}

// ========================= End Extracting Tokens =========================


}  // namespace parse
