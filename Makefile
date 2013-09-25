# Copyright (c) 2013 Kaname Kizu
# Relased under the MIT license. See LICENSE for more detail.

PROJECT_NAME = viewgrafy

CROSS_PREFIX ?= i686-pc-mingw32-
CXX = $(CROSS_PREFIX)g++
RC = $(CROSS_PREFIX)windres
LD = $(CROSS_PREFIX)g++

SOURCES = viewgrafy.cpp
RESOUCES = resource.rc

BUILD_DIR = obj
APP_DIR = $(PROJECT_NAME)

OBJ_FILES = $(SOURCES:.cpp=.o)
OBJS = $(patsubst %,$(BUILD_DIR)/%,$(OBJ_FILES))

RESOBJ_FILES = $(RESOUCES:.rc=.o)
RESOBJS = $(patsubst %,$(BUILD_DIR)/%,$(RESOBJ_FILES))

APP = $(APP_DIR)/$(PROJECT_NAME).exe

VPATH=source

CXXFLAGS = -Wall -O2
LDFLAGS = -static-libgcc -mwindows -lgdi32 -lgdiplus -lComCtl32 -static

.PHONY: all clean
.SUFFIXES: .o .cpp .rc

all: $(APP)

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(APP_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

$(BUILD_DIR)/%.o: %.rc
	$(RC) $< -o $@

$(APP): $(BUILD_DIR) $(OBJS) $(RESOBJS)
	mkdir -p $(APP_DIR)
	$(LD) -o $(APP) $(OBJS) $(RESOBJS) $(LDFLAGS) 

