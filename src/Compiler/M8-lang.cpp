#include "M8-lang.hpp"
#include "../AST/ProgramNode.hpp"

using std::string;

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

	Lexer lexer;
    std::vector<Token> tokens = lexer.tokenize(source);
    SyntaxParser parser(tokens);
    std::unique_ptr<ProgramNode> program = parser.parseProgram();

    if (!program || parser.hasErrors())
    {
        const std::vector<string>& errs = parser.errors();
        if (errs.empty())
        {
            std::cerr << "Parse error: unable to build AST" << std::endl;
        }
        else
        {
            for (const string& err : errs)
                std::cerr << "Parse error: " << err << std::endl;
        }
        return 1;
    }

    SemanticAnalyzer semantic;
    bool semanticOk = semantic.analyze(*program);

    for (const string& warning : semantic.warnings())
        std::cerr << "Warning: " << warning << std::endl;

    if (!semanticOk)
    {
        for (const string& err : semantic.errors())
            std::cerr << "Semantic error: " << err << std::endl;
        return 1;
    }

    IRContext ctx;
    ctx.ir << "declare i32 @printf(i8*, ...)\n";
    ctx.ir << "declare i32 @scanf(i8*, ...)\n\n";
    ctx.ir << "@fmtw = private constant [4 x i8] c\"%d\\0A\\00\"\n";
    ctx.ir << "@fmtr = private constant [3 x i8] c\"%d\\00\"\n\n";

    CodeGenerator generator(ctx, semantic.symbols());
    generator.generate(*program);

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
