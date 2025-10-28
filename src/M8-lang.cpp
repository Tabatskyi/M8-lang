#include "M8-lang.hpp"

constexpr const int ONE_CHAR_BYTES = 3;

using std::string;

static std::unordered_map<string, TokenType> doubleOperatorMap = 
{
    {"᛬᛬", TokenType::Equals},
    {"ᛅ᛬", TokenType::NotEqual},
    {"᛭᛬", TokenType::AddAssign},
    {"ᛧ᛬", TokenType::SubAssign},
    {"᛫᛬", TokenType::MulAssign},
    {"ᛇ᛬", TokenType::DivAssign},
};

static std::unordered_map<string, TokenType> singleOperatorMap = 
{
    {"ᛵ", TokenType::StmtSep},
    {"ᛜ", TokenType::Then},
    {"᛬", TokenType::Assign},
    {"᛭", TokenType::Add},
    {"ᛧ", TokenType::Sub},
    {"᛫", TokenType::Mul},
    {"ᛇ", TokenType::Div},
    {"ᛅ", TokenType::Not},
    {"ᚮ", TokenType::LParen},
    {"ᚭ", TokenType::RParen},
};

static std::unordered_map<string, TokenType> keywordMap = 
{
    {"ᛗ", TokenType::If},
    {"ᛎ", TokenType::Else},
    {"ᚷ", TokenType::Return},
    {"ᚡ", TokenType::Var},
    {"ᛍ", TokenType::Const},
    {"ᛉ", TokenType::True},
    {"ᛣ", TokenType::False},
};

static bool matchUTF(size_t i, const char* lit, const string& source)
{
    size_t len = std::char_traits<char>::length(lit);
    return i + len <= source.size() && source.compare(i, len, lit) == 0;
};

static std::vector<Token> lexSource(const string& source)
{
    enum class State { Start, Identifier, Number };

    std::vector<Token> out;
    State state = State::Start;
    size_t i = 0;
    size_t tokenStart = 0;

    auto emit_identifier = [&](size_t begin, size_t end) {
        if (end > begin)
            out.push_back({ source.substr(begin, end - begin), TokenType::Identifier });
    };

    auto emit_number = [&](size_t begin, size_t end) {
        if (end > begin)
            out.push_back({ source.substr(begin, end - begin), TokenType::Number });
    };

    while (i < source.size())
    {
        unsigned char uc = static_cast<unsigned char>(source[i]);
        switch (state)
        {
        case State::Start:
        {
            if (std::isspace(uc)) { ++i; break; }

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
            emit_identifier(tokenStart, i);
            state = State::Start;
            break;
        }
        case State::Number:
        {
            while (i < source.size() && std::isdigit(static_cast<unsigned char>(source[i])))
                ++i;
            if (matchUTF(i, "ᛰ", source) || matchUTF(i, "ᛯ", source))
                i += 3;
            emit_number(tokenStart, i);
            state = State::Start;
            break;
        }
        }
    }

    if (state == State::Identifier) emit_identifier(tokenStart, i);
    else if (state == State::Number) emit_number(tokenStart, i);

    out.push_back(Token{ "", TokenType::EndOfFile });
    return out;
}

static string typeToString(ValueType type)
{
    switch (type)
    {
    case ValueType::I32: return "i32";
    case ValueType::I64: return "i64";
    case ValueType::Bool: return "bool";
    default: return "<invalid>";
    }
}

static bool isNumeric(ValueType type)
{
    return type == ValueType::I32 || type == ValueType::I64;
}

static ValueType widerType(ValueType lhs, ValueType rhs)
{
    if (!isNumeric(lhs) || !isNumeric(rhs))
        return ValueType::Invalid;
    if (lhs == ValueType::I64 || rhs == ValueType::I64)
        return ValueType::I64;
    return ValueType::I32;
}

static ValueType comparisonOperandType(ValueType lhs, ValueType rhs)
{
    if (lhs == ValueType::Bool && rhs == ValueType::Bool)
        return ValueType::Bool;
    return widerType(lhs, rhs);
}

static bool isAssignable(ValueType target, ValueType source)
{
    if (target == ValueType::Invalid || source == ValueType::Invalid)
        return false;
    if (target == source)
        return true;
    if (target == ValueType::I64 && source == ValueType::I32)
        return true;
    return false;
}

static bool canConvertToI32(ValueType type)
{
    return type == ValueType::I32 || type == ValueType::I64 || type == ValueType::Bool;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: compiler <source> <output>" << std::endl;
        return 1;
    }

    std::ifstream fin(argv[1]);
    if (!fin)
    {
        std::cerr << "Cannot open file" << std::endl;
        return 1;
    }

    std::ostringstream buffer;
    buffer << fin.rdbuf();
    string source = buffer.str();
    fin.close();

    auto tokens = lexSource(source);
    SyntaxParser parser(tokens);
    auto program = parser.parseProgram();

    if (!program || parser.hasErrors())
    {
        const auto& errs = parser.errors();
        if (errs.empty())
        {
            std::cerr << "Parse error: unable to build AST" << std::endl;
        }
        else
        {
            for (const auto& err : errs)
                std::cerr << "Parse error: " << err << std::endl;
        }
        return 1;
    }

    SemanticAnalyzer semantic;
    bool semanticOk = semantic.analyze(*program);

    for (const auto& warning : semantic.warnings())
        std::cerr << "Warning: " << warning << std::endl;

    if (!semanticOk)
    {
        for (const auto& err : semantic.errors())
            std::cerr << "Semantic error: " << err << std::endl;
        return 1;
    }

    IRContext ctx;
    ctx.ir << "declare i32 @printf(i8*, ...)\n\n";
    ctx.ir << "@fmt = private constant [29 x i8] c\"Program exit with result %d\\0A\\00\"\n\n";
    ctx.ir << "define i32 @main() {\n";

    CodeGenerator generator(ctx, semantic.symbols());
    generator.generate(*program);

    ctx.ir << "}\n";

    string filename;
    if (argc >= 3)
    {
        filename = argv[2];
    }
    else
    {
        filename = argv[1];
        size_t dot = filename.find_last_of('.');
        if (dot != string::npos)
            filename = filename.substr(0, dot);
        filename += ".ll";
    }

    std::ofstream fout(filename);
    fout << ctx.ir.str();
    fout.close();

    return 0;
}
