
SRCS = atomicdata.cpp bitbase.cpp bitboard.cpp book.cpp create_book.cpp \
       debug.cpp endgame.cpp evaluate.cpp nnue.cpp main.cpp main_uci.cpp material.cpp \
       misc.cpp move.cpp movegen.cpp movepick.cpp pawns.cpp pgn.cpp \
       position.cpp search.cpp simple_search.cpp thread.cpp timeman.cpp \
       tt.cpp tuning.cpp types.cpp uci.cpp ucioption.cpp # not sure all needed
HEADERS = atomicdata.h bitboard.h bitcount.h book.h create_book.h debug.h \
          endgame.h evaluate.h nnue.h fics.h history.h lock.h main.h material.h \
          misc.h movegen.h move.h movepick.h pawns.h pgn.h position.h \
          psqtab.h rkiss.h search.h simple_search.h thread.h timeman.h \
          tt.h tuning.h types.h ucioption.h

OBJS = $(SRCS:.cpp=.o)

CXX ?= g++
OPTIONS = -O2 -DNDEBUG -Wall

all: depend atomkraft
%.o : %.cpp Makefile
	$(CXX) $(OPTIONS) -o $@ -c $<
atomkraft: $(HEADERS) $(OBJS) Makefile
	$(CXX) -o atomkraft $(OBJS) -lm -lstdc++ -lpthread
windows: # well, this works for me (direct, no *.o), with 2 align warnings
	$(CXX) -g -O3 \
         -DNDEBUG -DWIN32_BUILD \
         -o ATOMKRAFT.exe *.cpp -static -lstdc++ -lpthread
windows64:
	$(CXX) -g -O3 \
          -DNDEBUG -DWIN64_BUILD \
         -o ATOMKRAFT64.exe *.cpp -static -lstdc++ -lwinpthread
archive:
	tar cf ATOMKRAFT.tar $(SRCS) $(HEADERS) Makefile
depend: $(SRCS) $(HEADERS)
	$(CXX) -MM $(SRCS) > depend
clean:
	rm -f *.o atomkraft depend

-include depend

# note: all the versions (linux, win32, win64) give different node counts :(
