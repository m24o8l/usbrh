# usage: sudo make
SRC = usbrh_main.c
EXE = usbrh

$(EXE): $(SRC)
	gcc -Wall -O -o $@ $^ -lusb
	chmod u+s $(EXE)

clean: 
	rm -f $(EXE)
