#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "resolver/resolver.hpp"
#include "ast/ast.hpp"

#include <iostream>

int main() {

	constexpr auto _code = 
#include "code.def"
	;

	try {
		auto lex = bloop::lexer::CLexer(_code);
		lex.Parse();
			
		bloop::parser::CLexParser parser(lex);

		if (const auto code = parser.Parse()) {
			bloop::resolver::Resolve(code.get());
			std::cout << "yippee!\n";
		}

	}
	catch (std::runtime_error& ex) {
		std::cout << ex.what() << '\n';
	}

	return 0;
}
