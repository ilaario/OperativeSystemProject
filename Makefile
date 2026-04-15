CC = gcc
CFLAGS = -Wvla -Wextra -Werror -D_GNU_SOURCE -std=c99 -I./headers
SRC_DIR = src
BUILD_DIR = build
GFLAGS_DEBUG = $(CFLAGS) -g -O0 -Wall -D DEBUGLOG
DEBUG = gdb
LOGGER_SRC = $(SRC_DIR)/logger.c

all: compile clear

run_tm: compile
	# clear
	$(BUILD_DIR)/director src/config/config_timeout.conf

run_ex: compile
	# clear
	$(BUILD_DIR)/director src/config/config_explode.conf

run_debug: compile_debug
	clear
	$(BUILD_DIR)/director src/config/config_timeout.conf

compile_lib:
	$(CC) -c $(SRC_DIR)/logger.c -o $(SRC_DIR)/logger.o
	rm -rf $(SRC_DIR)/logger.o

compile:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/director $(SRC_DIR)/director.c $(LOGGER_SRC)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/user $(SRC_DIR)/user.c $(LOGGER_SRC)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/operator $(SRC_DIR)/operator.c $(LOGGER_SRC)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/erogatore_ticket $(SRC_DIR)/erogatore_ticket.c $(LOGGER_SRC)

compile_debug:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GFLAGS_DEBUG) -o $(BUILD_DIR)/director $(SRC_DIR)/director.c $(LOGGER_SRC)
	$(CC) $(CFLAGS) $(GFLAGS_DEBUG) -o $(BUILD_DIR)/user $(SRC_DIR)/user.c $(LOGGER_SRC)
	$(CC) $(CFLAGS) $(GFLAGS_DEBUG) -o $(BUILD_DIR)/operator $(SRC_DIR)/operator.c $(LOGGER_SRC)
	$(CC) $(CFLAGS) $(GFLAGS_DEBUG) -o $(BUILD_DIR)/erogatore_ticket $(SRC_DIR)/erogatore_ticket.c $(LOGGER_SRC)

run_valgrind: compile
	valgrind -s --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --log-file=valgrind-out.txt $(BUILD_DIR)/director

clear:
	rm -rf $(BUILD_DIR)
