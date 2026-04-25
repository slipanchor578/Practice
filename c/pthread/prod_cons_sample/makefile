CC = gcc
CFLAGS = -Wall -Wextra -std=c18 -march=native -fdiagnostics-color=always -g

TARGET = myapp
SRCS = main.c consumer.c shared.c
OBJS = $(SRCS:.c=.o)
DEPS = consumer.h shared.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) $(OBJS)

run:
	./$(TARGET)

.PHONY: all clean run