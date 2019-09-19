#user changes
#INCLUDES=-I$(HOME)/Programs/uhd/include/
#LIBS=-L$(HOME)/Programs/uhd/lib/ -lboost_system -luhd
LIBS=-lboost_system -lboost_program_options -luhd -lopencv_core -lopencv_imgproc -lopencv_videoio -lopencv_highgui -lopencv_imgcodecs

#maybe change
CXX=g++
CFLAGS=-std=c++11
RM=rm -f

#done change
SRCS=src/interface.cpp src/tempest.cpp src/frameStream.cpp src/extraMath.cpp
OBJS=$(subst src/,bin/,$(subst .cpp,.o,$(SRCS)))

tempAtk: $(OBJS)
	$(CXX) -o tempAtk $(OBJS) $(CFLAGS) $(LIBS)

all: bin/tempAtk

bin/interface.o: src/interface.cpp src/resconvert.h
	$(CXX) -o bin/interface.o -c src/interface.cpp $(CFLAGS) $(LIBS)

bin/tempest.o: src/tempest.cpp src/tempest.h
	$(CXX) -o bin/tempest.o -c src/tempest.cpp $(CFLAGS) $(LIBS)

bin/frameStream.o: src/frameStream.cpp src/frameStream.h
	$(CXX) -o bin/frameStream.o -c src/frameStream.cpp $(CFLAGS) $(LIBS)

bin/extraMath.o: src/extraMath.cpp src/extraMath.h
	$(CXX) -o bin/extraMath.o -c src/extraMath.cpp $(CFLAGS) $(LIBS)

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) tempAtk

run:
	tempAtk
