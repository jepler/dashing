default: dashing

dashing: main.cc dashing.cc dashing.h
	g++ -W -Wall -O2 -g -std=c++11 $(filter %.cc, $^) -o $@ -lboost_random
dashing-noopt: main.cc dashing.cc dashing.h
	g++ -W -Wall -O0 -g -std=c++11 $(filter %.cc, $^) -o $@ -lboost_random
