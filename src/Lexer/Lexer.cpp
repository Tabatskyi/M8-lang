#include "Lexer.hpp"

static bool matchUTF(size_t i, const char* lit, const string& source)
{
    size_t len = std::char_traits<char>::length(lit);
    return i + len <= source.size() && source.compare(i, len, lit) == 0;
};

static size_t utf8CharLength(unsigned char lead)
{
    if ((lead & 0x80) == 0) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

static bool appendEscapeSequence(const string& source, size_t& cursor, std::string& out)
{
    if (cursor >= source.size())
        return false;

    if (matchUTF(cursor, "ᛌ", source))
    {
        out.append("ᛌ");
        cursor += std::char_traits<char>::length("ᛌ");
        return true;
    }

    unsigned char c = static_cast<unsigned char>(source[cursor]);
    switch (c)
    {
    case 'n': out.push_back('\n'); ++cursor; return true;
    case 't': out.push_back('\t'); ++cursor; return true;
    case 'r': out.push_back('\r'); ++cursor; return true;
    case '\\': out.push_back('\\'); ++cursor; return true;
    case '0': out.push_back('\0'); ++cursor; return true;
    default:
        out.push_back(static_cast<char>(c));
        ++cursor;
        return true;
    }
}

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

            if (matchUTF(i, "ᛌ", source))
            {
                size_t cursor = i + std::char_traits<char>::length("ᛌ");
                std::string literalValue;
                bool closed = false;
                while (cursor < source.size())
                {
                    if (matchUTF(cursor, "ᛌ", source))
                    {
                        closed = true;
                        cursor += std::char_traits<char>::length("ᛌ");
                        break;
                    }

                    unsigned char innerChar = static_cast<unsigned char>(source[cursor]);
                    if (innerChar == '\\')
                    {
                        ++cursor;
                        if (!appendEscapeSequence(source, cursor, literalValue))
                        {
                            std::cerr << "Lex error: Unterminated escape sequence in string literal\n";
                            break;
                        }
                        continue;
                    }
                    if (innerChar == '\n' || innerChar == '\r')
                    {
                        std::cerr << "Lex error: newline inside string literal\n";
                        break;
                    }

                    size_t len = utf8CharLength(innerChar);
                    if (cursor + len > source.size())
                    {
                        std::cerr << "Lex error: truncated UTF-8 sequence inside string literal\n";
                        break;
                    }
                    literalValue.append(source, cursor, len);
                    cursor += len;
                }

                if (!closed)
                {
                    std::cerr << "Lex error: Unterminated string literal\n";
                }
                else
                {
                    out.push_back({ literalValue, TokenType::StringLiteral });
                }
                i = cursor;
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