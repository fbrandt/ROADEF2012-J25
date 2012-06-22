CC      = /usr/bin/g++
CFLAGS  = -std=c++0x -O2 -I../gecode
LDFLAGS = -L../gecode -lgecodekernel -lgecodeint -lgecodeset -lgecodeminimodel -lgecodegist -lgecodesearch -lgecodesupport -lgecodedriver -lpthread

OBJ = BaseSearch.o BestCostBrancher.o  CostPropagator.o Instance.o IterativeSearch.o ProcessFixing.o ProcessNeighborhoodSearch.o ProcessPropagator.o RandomSearch.o ReAssignment.o RescheduleSpace.o SchedulePlotter.o TargetMoveSearch.o UndoMoveSearch.o
BIN = main

main: main.cpp $(OBJ)
	$(CC) $(CFLAGS) main.cpp -o main $(OBJ) $(LDFLAGS)

%.o: %.cpp %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf $(BIN) $(OBJ)
