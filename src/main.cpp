#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "resolver/resolver.hpp"
#include "ast/ast.hpp"
#include "bytecode/build.hpp"
#include "bytecode/function/bc_function.hpp"
#include "vm/vm.hpp"

#include <iostream>
#include <thread>
#include <chrono>
using namespace std::chrono_literals;

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
			bloop::vm::VM vm(bloop::bytecode::BuildByteCode(code.get()));

			vm.Run("main");

			//std::this_thread::sleep_for(5s); // just to see the memory usage drop

			std::cout << "\n\nfinished!\n";
		}

	}
	catch (std::runtime_error& ex) {
		std::cout << ex.what() << '\n';
	}

	return 0;
}
