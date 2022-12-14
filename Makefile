all: build run
build:
	ld -r -b binary -o image.o image.png
	gcc image.o graphics.c network.c game.c chess.c -fPIC -lglut -lGL -lSOIL -o chess
run:
	./chess -s
clean:
	rm image.o
