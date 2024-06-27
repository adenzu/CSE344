CC = gcc
CFLAGS = -Wall -Wextra
SRC1 = main.c
SRC2 = executor.c
OBJ1 = $(SRC1:.c=.o)
OBJ2 = $(SRC2:.c=.o)
EXEC1 = main.out
EXEC2 = executor.out
LOG_FILE = StudentGradeManagementSystem.log

all: $(EXEC1) $(EXEC2)

$(EXEC1): $(OBJ1)
	$(CC) $(CFLAGS) -o $@ $^

$(EXEC2): $(OBJ2)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(EXEC1) $(OBJ1) $(EXEC2) $(OBJ2) $(LOG_FILE)
