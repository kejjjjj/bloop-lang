#include "lexer/lexer.hpp"
#include "lexer/exception.hpp"
#include "parser/parser.hpp"

#include <iostream>

int main() {
	auto lex = bloop::lexer::CLexer("1 + 1 * 3;");
	try {
		lex.Parse();
			
		bloop::parser::CLexParser parser(lex);

		if (parser.Parse() == bloop::EStatus::success) {
			std::cout << "yippee!\n";
		}

	}
	catch (std::runtime_error& ex) {
		std::cout << ex.what() << '\n';
	}

	return 0;
}
