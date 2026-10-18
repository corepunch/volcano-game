CC ?= cc
PACKAGES = sdl2
CPPFLAGS += $(shell pkg-config --cflags $(PACKAGES)) -Ivendor -Isrc -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS += $(shell pkg-config --libs $(PACKAGES)) -lm
SOURCES = src/main.c src/resources.c src/level.c src/game.c

.PHONY: all app run test sanitize clean
all: build/volcano
build/volcano: $(SOURCES) src/volcano.h vendor/stb_image.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)
app: build/volcano
	@mkdir -p build/Volcano.app/Contents/MacOS
	cp build/volcano build/Volcano.app/Contents/MacOS/volcano
	cp packaging/Info.plist build/Volcano.app/Contents/Info.plist
	@mkdir -p build/Volcano.app/Contents/Resources
	rsync -a --exclude='.DS_Store' --exclude='Info.plist' resources/ build/Volcano.app/Contents/Resources/
run: all
	./build/volcano
test: all
	SDL_VIDEODRIVER=dummy ./build/volcano --software --hidden --self-test
sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test
clean:
	rm -rf build
