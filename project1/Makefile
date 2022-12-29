objects = main.o src/userio.o src/cmder.o

npshell: $(objects)
	g++ $(objects) -o npshell
	mv npshell demo/

$(objects): %.o: %.cpp
	g++ -c $^ -o $@

clean:
	rm demo/npshell
	rm *.o
	rm src/*.o
