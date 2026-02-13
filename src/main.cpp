#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "resolver/resolver.hpp"
#include "ast/ast.hpp"
#include "bytecode/build.hpp"
#include "bytecode/function/bc_function.hpp"
#include "vm/vm.hpp"
#include "metadata/metadata.hpp"

#include <iostream>
#include <thread>
#include <chrono>
using namespace std::chrono_literals;

int main() {

	constexpr auto _code = 
#include "code.def"
	;

	try {
		bloop::metadata::Metadata metadata;
		auto lex = bloop::lexer::CLexer(_code, metadata);
		lex.Parse();
			
		bloop::parser::CLexParser parser(lex, metadata);

		if (const auto code = parser.Parse()) {
			bloop::resolver::Resolve(code.get(), metadata);
			bloop::bytecode::BuildByteCode(code.get(), metadata);
			bloop::vm::VM vm(metadata);

			auto ret = vm.Run();

			std::cout << "\nreturned: " << ret.TypeToString() << " : " << ret.ValueToString() << '\n';
			std::cout << "\n\nfinished!\n";
		}
		
	}
	catch (std::runtime_error& ex) {
		std::cout << ex.what() << '\n';
	}

	return 0;
}
