default: dashing

dashing: main.c dashing.c dashing.h
	g++ -W -Wall -O2 -g -std=c++11 $(filter %.c, $^) -o $@ -lboost_random
