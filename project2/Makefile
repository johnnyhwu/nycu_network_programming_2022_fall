objects = src/neter.o src/shm.o src/shell/cmder.o src/shell/shell.o src/shell/userio.o
targets = np_simple.o np_single_proc.o np_multi_proc.o

np_server: $(objects) $(targets)
	g++ $(objects) $(word 1, $(targets)) -o np_simple
	g++ $(objects) $(word 2, $(targets)) -o np_single_proc
	g++ $(objects) $(word 3, $(targets)) -o np_multi_proc

$(objects) $(targets): %.o: %.cpp
	g++ -c $^ -o $@

clean:
	rm -rf np_simple np_single_proc np_multi_proc
	rm -rf *.o
	rm -rf src/*.o
	rm -rf src/shell/*.o
