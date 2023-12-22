.PHONY: default
default: dashing

.PHONY: bench
bench: dashing
	perf stat --log-fd=2 ./dashing -b -s .1 data/HWOOD6E1.pat data/sf.seg

dashing: main.cc dashing.cc dashing.hh parse_numbers.hh contours_and_segments.hh
	g++ -W -Wall -O2 -g -std=c++11 $(filter %.cc, $^) -o $@ -lboost_random -flto
dashing-noopt: main.cc dashing.cc dashing.hh parse_numbers.hh contours_and_segments.hh
	g++ -W -Wall -O0 -g -std=c++11 $(filter %.cc, $^) -o $@ -lboost_random
dashing-clang: main.cc dashing.cc dashing.hh parse_numbers.hh
	clang++ -W -Wall -O2 -g -std=c++11 $(filter %.cc, $^) -o $@ -lboost_random -flto
