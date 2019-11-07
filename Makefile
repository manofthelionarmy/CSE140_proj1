sim : computer.o sim.o
	gcc -g -fno-stack-protector -Wall -o sim sim.o computer.o

sim.o : computer.h sim.c
	gcc -g -fno-stack-protector -c -Wall sim.c

computer.o : computer.c computer.h
	gcc -g -fno-stack-protector -c -Wall computer.c

clean:
	\rm -rf *.o sim
