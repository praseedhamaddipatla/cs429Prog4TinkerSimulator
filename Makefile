build: build.sh
	./build.sh

run: Simulator.c
	./hw4 output1.tko

test: AssemblerTest.c
	./hw4 test.bin

hex: output1.tko
	hexdump -C output1.tko