all: build run
build:
	ld -r -b binary -o image.o image.png
	gcc image.o chess.c -fPIC -lglut -lGL -lSOIL -o chess
run:
	./chess -s
clean:
	rm image.o
