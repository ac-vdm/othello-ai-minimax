#COMPILER ?= mpicc
COMPILER ?= mpicc

CFLAGS ?= -O2 -g -Wall -Wno-variadic-macros -pedantic -DDEBUG $(GCC_SUPPFLAGS)
LDFLAGS ?= -g 
LDLIBS =

MYPLAYER = random 
EXECUTABLE = obj/${MYPLAYER}

SRCS=$(wildcard src/*.c)
OBJS=$(SRCS:src/%.c=obj/%.o)

all: release move

release: $(OBJS)
	$(COMPILER) $(LDFLAGS) -o $(EXECUTABLE) $(OBJS) $(LDLIBS) 

obj/%.o: src/%.c | obj 
	$(COMPILER) $(CFLAGS) -o $@ -c $<

obj:
	mkdir -p $@

move: obj
	mv $(EXECUTABLE) ../players/$(MYPLAYER)
	rm -f obj/*.o

clean:
	rm -f obj/*.o
	rm ${EXECUTABLE} 
	rmdir obj

cleandata:
	rm -r Logs/*
	rm black*.txt
	rm white*.txt
