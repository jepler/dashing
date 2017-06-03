.PHONY: default
default: dashing

.PHONY: bench%
bench%: dashing%
	perf stat --log-fd=2 ./$< -b -s .1 data/HWOOD6E1.pat data/sf.seg

HEADERS:
dashing: main.o dashing.o
	g++ -W -Wall -O2 -g -std=c++11 $^ -o $@
dashing%: main%.o dashing%.o
	g++ -W -Wall -O2 -g -std=c++11 $^ -o $@

HEADERS := $(wildcard *.hh)

default: gldashing
gldashing: glmain.o dashing.o glsetup.o
	g++ -o $@ $^ -lSDL2 -lGLEW -lGL -lrt
gldashing%: glmain%.o dashing%.o glsetup%.o
	g++ -o $@ $^ -lSDL2 -lGLEW -lGL -lrt

%.o: %.cc $(HEADERS)
	g++ -W -Wall -O2 -g -std=c++11 -c $< -o $@
%-noopt.o: %.cc $(HEADERS)
	g++ -W -Wall -O0 -g -std=c++11 -c $< -o $@
%-clang.o: %.cc $(HEADERS)
	clang++ -W -Wall -O0 -g -std=c++11 -c $< -o $@

.PHONY: clean
clean:
	rm -f dashing dashing-noopt dashing-clang \
		gldashing gldashing-noopt gldashing-clang \
		*.o *.d
