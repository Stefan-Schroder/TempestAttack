#user changes
#INCLUDES=-I$(HOME)/Programs/uhd/include/
#LIBS=-L$(HOME)/Programs/uhd/lib/ -lboost_system -luhd
LIBS=-lboost_system -lboost_program_options -luhd

#maybe change
CXX=g++
CFLAGS=-std=c++11
RM=rm -f

#done change
SRCS=src/main.cpp
OBJS=$(subst src/,bin/,$(subst .cpp,.o,$(SRCS)))

tempAtk: $(OBJS)
	$(CXX) $(INCLUDES) -o tempAtk $(OBJS) $(CFLAGS) $(LIBS)

all: bin/tempAtk


bin/main.o: src/main.cpp
	$(CXX) $(INCLUDES) -o bin/main.o -c src/main.cpp $(CFLAGS) $(LIBS)

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) tempAtk

run:
	tempAtk
