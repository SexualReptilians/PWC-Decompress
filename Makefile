TARGET = unpack
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

INCLUDE = -Iinclude
LDFLAGS = 
CFLAGS = -MMD -m64 -D_FILE_OFFSET_BITS=64 $(INCLUDE)

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJ) $(DEP) $(TARGET)

-include $(DEP)
