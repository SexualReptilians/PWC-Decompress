TARGET = unpack
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

INCLUDE = -Iinclude
LDFLAGS = 
CFLAGS = -MMD $(INCLUDE)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -f $(OBJ) $(DEP) $(TARGET)

-include $(DEP)
