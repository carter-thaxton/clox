
TARGET_NAME	:= clox

CC			= gcc
CXX 		= gcc
CFLAGS 		:= -g -O2
CXXFLAGS 	:=

#DBGFLAGS 	:= -g
COBJFLAGS	:= $(CFLAGS) -c

BIN_PATH := bin
OBJ_PATH := obj
SRC_PATH := src
#DBG_PATH := debug

TARGET 		:= $(BIN_PATH)/$(TARGET_NAME)
#TARGET_DEBUG := $(DBG_PATH)/$(TARGET_NAME)

SRC := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.c*)))
OBJ := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))
# OBJ_DEBUG := $(addprefix $(DBG_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))

CLEAN_LIST := $(TARGET) $(OBJ)
# 			  $(TARGET_DEBUG) $(OBJ_DEBUG)

# default rule
default: makedir all

# non-phony targets
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c*
	$(CC) $(COBJFLAGS) -o $@ $<

# $(DBG_PATH)/%.o: $(SRC_PATH)/%.c*
# 	$(CC) $(COBJFLAGS) $(DBGFLAGS) -o $@ $<

# $(TARGET_DEBUG): $(OBJ_DEBUG)
# 	$(CC) $(CFLAGS) $(DBGFLAGS) $(OBJ_DEBUG) -o $@

# phony rules
.PHONY: makedir
makedir:
	@mkdir -p $(BIN_PATH) $(OBJ_PATH)

.PHONY: all
all: $(TARGET)

# .PHONY: debug
# debug: $(TARGET_DEBUG)

.PHONY: clean
clean:
# 	@echo CLEAN $(CLEAN_LIST)
	@rm -f $(CLEAN_LIST)
