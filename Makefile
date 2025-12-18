# Richard Fernando Heise Ferreira (GRR20191053)

CFLAGS  = -Wall -g -Iinclude
CC = gcc
LDFLAGS = -lm

# Variaveis
SRC_DIR = src
APP_DIR = app
OBJ_DIR = obj

# Fontes
SRC_C = $(wildcard $(SRC_DIR)/*.c)
APP_C = $(wildcard $(APP_DIR)/*.c)

# Objetos
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC_C))
OBJ += $(patsubst $(APP_DIR)/%.c, $(OBJ_DIR)/%.o, $(APP_C))

# Executavel
TARGET = pingpong

#-----------------------------------------------------------------------------#
all : $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(APP_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

#-----------------------------------------------------------------------------#

clean :
	$(RM) -r $(OBJ_DIR)

#-----------------------------------------------------------------------------#

purge: clean
	$(RM) $(TARGET) *.txt
