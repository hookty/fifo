pc: pc.c eventbuf.c eventbuf.h
	gcc -Wall -Wextra -Werror -o pc pc.c eventbuf.c -lpthread