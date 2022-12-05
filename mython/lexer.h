#pragma once

#include <iosfwd>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include <typeinfo>

namespace parse {

namespace token_type {
    struct Number {  // Лексема «число»
        int value;   // число
    };

    struct Id {             // Лексема «идентификатор»
        std::string value;  // Имя идентификатора
    };

    struct Char {    // Лексема «символ»
        char value;  // код символа
    };

    struct String {  // Лексема «строковая константа»
        std::string value;
    };

    struct Class {};    // Лексема «class»
    struct Return {};   // Лексема «return»
    struct If {};       // Лексема «if»
    struct Else {};     // Лексема «else»
    struct Def {};      // Лексема «def»
    struct Newline {};  // Лексема «конец строки»
    struct Print {};    // Лексема «print»
    struct Indent {};  // Лексема «увеличение отступа», соответствует двум пробелам
    struct Dedent {};  // Лексема «уменьшение отступа»
    struct Eof {};     // Лексема «конец файла»
    struct And {};     // Лексема «and»
    struct Or {};      // Лексема «or»
    struct Not {};     // Лексема «not»
    struct Eq {};      // Лексема «==»
    struct NotEq {};   // Лексема «!=»
    struct LessOrEq {};     // Лексема «<=»
    struct GreaterOrEq {};  // Лексема «>=»
    struct None {};         // Лексема «None»
    struct True {};         // Лексема «True»
    struct False {};        // Лексема «False»
}  // namespace token_type

using TokenBase
    = std::variant<token_type::Number, token_type::Id, token_type::Char, token_type::String,
                   token_type::Class, token_type::Return, token_type::If, token_type::Else,
                   token_type::Def, token_type::Newline, token_type::Print, token_type::Indent,
                   token_type::Dedent, token_type::And, token_type::Or, token_type::Not,
                   token_type::Eq, token_type::NotEq, token_type::LessOrEq, token_type::GreaterOrEq,
                   token_type::None, token_type::True, token_type::False, token_type::Eof>;

struct Token : TokenBase {
    using TokenBase::TokenBase;

    template <typename T>
    [[nodiscard]] bool Is() const {
        return std::holds_alternative<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T& As() const {
        return std::get<T>(*this);
    }

    template <typename T>
    [[nodiscard]] const T* TryAs() const {
        return std::get_if<T>(this);
    }
};

bool operator==(const Token& lhs, const Token& rhs);
bool operator!=(const Token& lhs, const Token& rhs);

std::ostream& operator<<(std::ostream& os, const Token& rhs);

namespace detail {
    std::string ReadWord(std::istream& input);
} // namespace detail

class LexerError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Lexer {
public:
    explicit Lexer(std::istream& input);

    // Возвращает ссылку на текущий токен или token_type::Eof, если поток токенов закончился
    [[nodiscard]] const Token& CurrentToken() const;

    // Возвращает следующий токен, либо token_type::Eof, если поток токенов закончился
    Token NextToken();

    // Если текущий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& Expect() const;

    // Метод проверяет, что текущий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void Expect(const U& value) const;

    // Если следующий токен имеет тип T, метод возвращает ссылку на него.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T>
    const T& ExpectNext();

    // Метод проверяет, что следующий токен имеет тип T, а сам токен содержит значение value.
    // В противном случае метод выбрасывает исключение LexerError
    template <typename T, typename U>
    void ExpectNext(const U& value);

private:
    // Парсинг всех токенов, содержащихся в std::istream
    void ParseTokens();

    // Удаление пробелов перед началом токенов
    void RemovingSpacesBeforeTokens();

    // Проверка входного потока на пустоту
    bool IsEof() const;

    // Если извлеченное слово не является ключевым, то оно - идентификатор
    void ExtractKeywordOrId();

    // Если слово является ключевым, то оно будет добавлено в вектор токенов
    bool AddKeyword(const std::string& word);

    // Извлечение строковой константы
    void ExtractString();

    // Извлечение целого числа
    void ExtractIntValue();

    // Извлечение оператора сравнения, состоящего из 2 символов.
    // Если второй символ не "=", то получаем отдельный символ (например, '(', '=' и т.д.)
    void ExtractOperatorEqOrSymbol();

    // Определяет символ "\n"
    void ExtractNewLine();

    // Обработка комментариев
    void ExtractComments();

    // Извлекает увеличение или уменьшение отступа (2 пробела)
    void ExtractDent();

private:
    std::istream& input_tokens_;
    std::vector<Token> tokens_; // распарсенные токены
    size_t index_current_token_ = 0;
    const uint16_t num_space_dent_ = 2u; // количество пробелов у отступа
    uint16_t current_dent_ = 0; // текущее количество оступов
};

template <typename T>
const T& Lexer::Expect() const {
    using namespace std::literals;
    if (CurrentToken().Is<T>()) {
        return CurrentToken().As<T>();
    }
    std::string name_type_data = typeid(T).name();
    throw LexerError("The current token does not have the \"" + name_type_data + "\" data type"s);
}

template <typename T, typename U>
void Lexer::Expect(const U& value) const {
    using namespace std::literals;
    if (Expect<T>().value != value) {
        throw LexerError("The value of the current token is not equal to the expected"s);
    }
}

template <typename T>
const T& Lexer::ExpectNext() {
    NextToken();
    return Expect<T>();
}

template <typename T, typename U>
void Lexer::ExpectNext(const U& value) {
    NextToken();
    Expect<T>(value);
}

}  // namespace parse
