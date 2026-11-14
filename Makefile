# Thin wrappers around the CMake presets. CMake is the real build.
# Choose a configuration with PRESET=debug|release|asan (default: release).
PRESET ?= release
BUILD_DIR := build/$(PRESET)

.DEFAULT_GOAL := build
.PHONY: build test demo ravel clean

$(BUILD_DIR)/CMakeCache.txt:
	cmake --preset $(PRESET)

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build --preset $(PRESET)

test: build
	ctest --preset $(PRESET)

demo: build
	$(BUILD_DIR)/ravel fixtures/session.txt

ravel: build
	cmake -E copy $(BUILD_DIR)/ravel ravel

clean:
	$(RM) -r build ravel ravel-test
