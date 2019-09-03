#user changes
INCLUDES=-I$(HOME)/Programs/uhd/include/
LIBS=-L$(HOME)/Programs/uhd/lib/ -lboost_system

#maybe change
CXX=g++
CFLAGS=-std=c++11
RM=rm -f

#done change
SRCS=src/main.cpp
OBJS=$(subst src/,bin/,$(subst .cpp,.o,$(SRCS)))

bin/tempAtk: $(OBJS)
	$(CXX) $(CFLAGS) -o bin/tempAtk $(OBJS) $(LIBS)

all: bin/tempAtk 

#depend: .depend
#
#.depend: $(SRCS)
#	$(RM) .depend
#	$(CXX) -o -MM $^>>./.depend; $(INCLUDES) $(CFLAGS) $(LIBS)

bin/main.o: src/main.cpp
	$(CXX) $(INCLUDES) -o bin/main.o -c src/main.cpp $(CFLAGS) $(LIBS)


clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) bin/tempAtk
	#$(RM) .depend

run:
	bin/tempAtk
#include .depend
