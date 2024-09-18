
TARGET_NAME	:= clox

CC       := g++
CFLAGS   := -g -O2
LFLAGS   := -l readline

BIN_PATH := bin
OBJ_PATH := obj
SRC_PATH := src

TARGET 		:= $(BIN_PATH)/$(TARGET_NAME)

SRC := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.c*)))
OBJ := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))

CLEAN_LIST := $(TARGET) $(OBJ)

# default rule
default: makedir all

# non-phony targets
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $(OBJ)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c*
	$(CC) $(CFLAGS) -c -o $@ $<

# phony rules
.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH) $(OBJ_PATH)

.PHONY: all
all: $(TARGET) test/test.pyc bin/test

.PHONY: clean
clean:
	@rm -f $(CLEAN_LIST)

.PHONY: rebuild r
r: rebuild
rebuild: clean all

# tests
.PHONY: test
test: test/test.pyc
	./run_tests.sh

test/test.pyc: test/test.py
	python3 -m compileall -b test/test.py

# test.cpp
SRC_TEST := test/test.cpp
OBJ_TEST := $(filter-out $(OBJ_PATH)/main.o,$(OBJ))
$(BIN_PATH)/test: $(OBJ_TEST) $(SRC_TEST)
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ -I$(SRC_PATH) $(SRC_TEST) $(OBJ_TEST)
