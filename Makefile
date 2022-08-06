CFLAGS= -O3 -W -Wformat=2 -g -Wall -Wextra -Werror -pedantic
LDFLAGS= 
NAME=main
OUTFILE=gol
CC=gcc

all: ${NAME}

${NAME}: ${NAME}.c
	${CC} ${NAME}.c -o ${OUTFILE} ${CFLAGS} ${LDFLAGS}

run: ${OUTFILE}
	./${OUTFILE}
