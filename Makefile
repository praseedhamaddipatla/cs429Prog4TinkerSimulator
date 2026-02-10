build: build.sh
	./build.sh

run: Simulator.c
	./hw4 test/full.tko

test: AssemblerTest.c
	./hw4 test.bin

hex: test/full.tko
	hexdump -C test/full.tko