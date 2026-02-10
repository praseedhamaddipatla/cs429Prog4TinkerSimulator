build: build.sh
	./build.sh

run: Simulator.c
	./hw4 output.tko

test: AssemblerTest.c
	gcc -Wall -Wextra -std=c11 Simulator.c SimulatorTest.c -lm -o hw4