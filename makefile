CC = g++

all: asembler linker emulator

build:
	mkdir build

parser.tab.cpp parser.tab.hpp : misc/parser.ypp inc/assembler.hpp
	bison -d $< -o build/parser.tab.cpp

lex.yy.cpp: misc/lexer.lpp parser.tab.hpp
	flex -o build/$@ $< 

asembler: build lex.yy.cpp parser.tab.cpp parser.tab.hpp inc/tableStructs.hpp
	$(CC) build/lex.yy.cpp build/parser.tab.cpp src/assembler.cpp src/asmMain.cpp -o asembler

linker: src/linker.cpp inc/tableStructs.hpp
	$(CC) src/linker.cpp inc/tableStructs.hpp -o linker

emulator: src/emulator.cpp inc/emulator.hpp
	$(CC) src/emulator.cpp inc/emulator.hpp -o emulator

clean:
	rm -rf build
	rm asembler
	rm linker
	rm emulator