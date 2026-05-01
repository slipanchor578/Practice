CC = gcc
CFLAGS = -Wall -Wextra -std=c18 -march=native -fdiagnostics-color=always -g

SRC_DIR = src
BUILD_DIR = build
# src/*.c を全てSRCS変数に格納する
SRCS = $(wildcard $(SRC_DIR)/*.c)

# 置き換え参照を使っている
# まず対象をSRCS変数内のパスにする
# その中から*.cを対象にする
# マッチしたファイルパスをbuild/*.oのようにする
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TARGET = $(BUILD_DIR)/server
DEPS = $(SRC_DIR)/http.h $(SRC_DIR)/internal.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS)
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -I $(SRC_DIR) -c $< -o $@

check:
	$(CC) $(CFLAGS) -fsyntax-only $(SRCS)

clean:
	rm -rf $(BUILD_DIR)

run:
	./$(TARGET)

.PHONY: all clean run