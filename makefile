CC = gcc
FLAGS = -lfuse

fs: controller.o fs.o
	$(CC) -o fs controller.o fs.o $(FLAGS)
	
controller.o: controller.c fs.h
	$(CC) -c controller.c

fs.o: fs.c fs.h
	$(CC) -c fs.c


