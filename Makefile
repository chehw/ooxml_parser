BIN_DIR=bin
LIB_DIR=lib

DEBUG ?=1

TARGET=$(BIN_DIR)/ooxml_parser

CC=gcc -std=gnu99
LINKER=$(CC)

CFLAGS = -Iinclude -Isrc -Wall
LIBS = -lm -lpthread -lzip -ljson-c
OPTIMIZE = -O2

ifeq ($(DEBUG),1)
CFLAGS += -g -D_DEBUG
OPTIMIZE = -O0
endif

CFLAGS += $(shell pkg-config --cflags libxml-2.0)
LIBS += $(shell pkg-config --libs libxml-2.0)

CFLAGS += $(shell pkg-config --cflags gtk+-3.0)
LIBS += $(shell pkg-config --libs gtk+-3.0)

ifeq ($(USE_POPPLER_PDF),1)
POPPLER_GLIB_PKG := $(shell pkg-config --list-all | grep poppler-glib)
CFLAGS += $(shell pkg-config --cflags poppler-glib)
LIBS += $(shell pkg-config --libs poppler-glib)
endif

LDFLAGS = $(CFLAGS) $(OPTIMIZE)

INC_DEPS := $(wildcard include/*.h)
INC_DEPS += $(wildcard src/*.h)
C_IMPL_DEPS := $(wildcard $(SRC_DIR)/ui/*.c)
DEPS := $(INC_DEPS) $(C_IMPL_DEPS)

SRC_DIR = src
OBJ_DIR = obj

SOURCES := $(wildcard $(SRC_DIR)/*.c)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

all: do_init $(TARGET)

$(BIN_DIR)/ooxml_parser: $(OBJECTS)
	$(LINKER) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJECTS): $(OBJ_DIR)/%.o : $(SRC_DIR)/%.c $(DEPS)
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: do_init clean
do_init:
	@[ -d bin ] || mkdir bin
	@[ -d obj ] || mkdir obj
	@[ -d tmp ] || mkdir tmp 
	
clean:
	rm -f $(OBJECTS) $(TARGET)
	
