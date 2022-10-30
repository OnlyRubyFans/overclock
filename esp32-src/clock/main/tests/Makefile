all: test_fti run_tests

fti: ../fti.c
	cc -c ../fti.c

test_fti: test_fti.cc fti
	c++ -std=c++11 test_fti.cc -o test_fti fti.o

run_tests:
	./test_fti
