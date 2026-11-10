BUILD_DIR ?= build
PREFIX    ?= $(shell brew --prefix 2>/dev/null || echo /usr/local)

.PHONY: all serve run clean

all: $(BUILD_DIR)/Makefile
	cmake --build $(BUILD_DIR)

$(BUILD_DIR)/Makefile: CMakeLists.txt
	cmake -S . -B $(BUILD_DIR) -DCMAKE_PREFIX_PATH=$(PREFIX) -DCMAKE_BUILD_TYPE=Release

serve: all
	$(BUILD_DIR)/server

run: all
	$(BUILD_DIR)/server & server=$$!; \
	trap "kill $$server" EXIT; \
	cargo run --release --manifest-path editor/Cargo.toml

clean:
	rm -rf $(BUILD_DIR)
