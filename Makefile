CCFLAGS=-std=gnu++98 -Wall -Werror -pedantic -ggdb3
ffosh: shell.cpp shell.h 
	g++ $(CCFLAGS) shell.cpp -o ffosh
