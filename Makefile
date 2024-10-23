
# Compiler and flags
CC = gcc
CFLAGS = -DNDEBUG=1 -std=gnu2x -fPIC -ffast-math -fopt-info-vec -funroll-loops -ftree-vectorize -mtune=native -O3 -Wall -Wpedantic -Wdouble-promotion -Iinclude
LDFLAGS = -lpthread -lrt -lm -lc

# Directories
PREFIX = /usr/local
INCLUDEDIR = $(PREFIX)/include/rpihub75
LIBDIR = $(PREFIX)/lib
BUILDDIR = build

# Source files
SRC_COMMON = src/util.c src/pixels.c src/rpihub75.c
SRC_GPU = src/gpu.c

# Library output names
LIB_NO_GPU = librpihub75.so
LIB_GPU = librpihub75_gpu.so

# Object files
# OBJ_COMMON = $(SRC_COMMON:%.c=$(BUILDDIR)/%.o)
# OBJ_GPU = $(SRC_GPU:%.c=$(BUILDDIR)/%.o)
OBJ_COMMON = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC_COMMON))
OBJ_GPU = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC_GPU))

# Check for OpenGL ES, GBM, and EGL libraries
GLESV2_FOUND := $(shell pkg-config --exists glesv2 && echo yes || echo no)
GBM_FOUND := $(shell pkg-config --exists gbm && echo yes || echo no)
EGL_FOUND := $(shell pkg-config --exists egl && echo yes || echo no)

# Targets
.PHONY: all clean install check-libs

# Default target to build both libraries
all: check-libs $(LIB_NO_GPU) $(LIB_GPU)

lib: $(LIB_NO_GPU)

libgpu: $(LIB_GPU)

# Create build directory if not exists
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Check for required GPU libraries
check-libs:
ifeq ($(GLESV2_FOUND),no)
    $(error "GLESv2 library not found. Please install it. sudo apt-get install libgles2-mesa-dev")
endif
ifeq ($(GBM_FOUND),no)
    $(error "GBM library not found. Please install it. sudo apt-get install libgbm-dev")
endif
ifeq ($(EGL_FOUND),no)
    $(error "EGL library not found. Please install it. sudo apt-get install libegl1-mesa-dev")
endif

# No-GPU library (without gpu.c, no OpenGL)
$(LIB_NO_GPU): $(OBJ_COMMON) | $(BUILDDIR)
	$(CC) $(CFLAGS) -shared -o $@ $(OBJ_COMMON)  $(LDFLAGS)

# GPU-enabled library (with gpu.c, linked to OpenGL)
$(LIB_GPU): $(OBJ_COMMON) $(OBJ_GPU) | $(BUILDDIR)
	$(CC) $(CFLAGS) -shared -o $@ $(OBJ_COMMON) $(OBJ_GPU) $(LDFLAGS) `pkg-config --libs glesv2 gbm egl`

# Install target
install: all
	# Create directories
	mkdir -p $(INCLUDEDIR)
	mkdir -p $(LIBDIR)
	# Copy header files
	chmod +x *.so
	cp include/rpihub75.h $(INCLUDEDIR)
	cp include/util.h $(INCLUDEDIR)
	cp include/gpu.h $(INCLUDEDIR)
	cp include/pixels.h $(INCLUDEDIR)
	# Copy libraries
	cp $(LIB_NO_GPU) $(LIB_GPU) $(LIBDIR)
	ldconfig

# Clean target
clean:
	rm -rf $(BUILDDIR)
	rm -f $(OBJ_COMMON) $(OBJ_GPU) $(LIB_NO_GPU) $(LIB_GPU)



# Object file compilation rules
$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies (optional)
$(BUILDDIR)/util.o: src/util.c include/util.h
$(BUILDDIR)/pixels.o: src/pixels.c 
$(BUILDDIR)/gpio.o: src/gpio.c include/rpihub75.h
$(BUILDDIR)/gpu.o: src/gpu.c include/rpihub75.h
