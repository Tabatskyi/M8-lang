#include "Lexer.hpp"

static bool matchUTF(size_t i, const char* lit, const string& source)
{
    size_t len = std::char_traits<char>::length(lit);
    return i + len <= source.size() && source.compare(i, len, lit) == 0;
};

static void emitIdentifier(size_t begin, size_t end, const string& source, std::vector<Token>& out)
{
    if (end > begin)
        out.push_back({ source.substr(begin, end - begin), TokenType::Identifier });
};

static void emitNumber(size_t begin, size_t end, const string& source, std::vector<Token>& out)
{
    if (end > begin)
        out.push_back({ source.substr(begin, end - begin), TokenType::Number });
};

std::vector<Token> Lexer::tokenize(const string& source) const
{
    enum class State { Start, Identifier, Number };

    std::vector<Token> out;
    State state = State::Start;
    size_t i = 0;
    size_t tokenStart = 0;

    while (i < source.size())
    {
        unsigned char uc = static_cast<unsigned char>(source[i]);
        switch (state)
        {
        case State::Start:
        {
            if (std::isspace(uc))
            { 
                ++i; 
                break; 
            }

            string doubleLexeme = source.substr(i, ONE_CHAR_BYTES * 2);
            if (doubleOperatorMap.count(doubleLexeme))
            {
                TokenType type = doubleOperatorMap[doubleLexeme];
                out.push_back({ doubleLexeme, type });
                i += ONE_CHAR_BYTES * 2;
                break;
            }

            string lexeme = source.substr(i, ONE_CHAR_BYTES);
            if (singleOperatorMap.count(lexeme))
            {
                TokenType type = singleOperatorMap[lexeme];
                out.push_back({ lexeme, type });
                i += ONE_CHAR_BYTES;
                break;
            }
            if (keywordMap.count(lexeme))
            {
                TokenType type = keywordMap[lexeme];
                out.push_back({ lexeme, type });
                i += 3;
                break;
            }

            if (std::isalpha(uc)) {
                state = State::Identifier;
                tokenStart = i;
                ++i;
                break;
            }
            if (std::isdigit(uc)) {
                state = State::Number;
                tokenStart = i;
                ++i;
                break;
            }

            std::cerr << "Lex warning: skipping unknown character near byte " << i << std::endl;
            ++i;
            break;
        }
        case State::Identifier:
        {
            while (i < source.size() && std::isalnum(static_cast<unsigned char>(source[i])))
                ++i;
			emitIdentifier(tokenStart, i, source, out);
            state = State::Start;
            break;
        }
        case State::Number:
        {
            while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i])))
                ++i;
            if (matchUTF(i, "ᛰ", source) || matchUTF(i, "ᛯ", source))
                i += 3;
			emitNumber(tokenStart, i, source, out);
            state = State::Start;
            break;
        }
        }
    }

	if (state == State::Identifier) emitIdentifier(tokenStart, i, source, out);
	else if (state == State::Number) emitNumber(tokenStart, i, source, out);

    out.push_back(Token{ "", TokenType::EndOfFile });
    return out;
}