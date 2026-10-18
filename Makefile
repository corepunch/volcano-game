CC ?= cc
PACKAGES = sdl2
ASSETS ?= reference/rescue-team
CPPFLAGS += $(shell pkg-config --cflags $(PACKAGES)) -Ivendor -Isrc -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS += $(shell pkg-config --libs $(PACKAGES)) -lm
SOURCES = src/main.c src/resources.c src/level.c src/campaign.c src/game.c

.PHONY: all app run test sanitize clean
all: build/volcano
build/volcano: $(SOURCES) src/volcano.h src/campaign_checks.h src/missions_4_6_checks.h vendor/stb_image.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)
app: build/volcano
	@mkdir -p build/Volcano.app/Contents/MacOS
	cp build/volcano build/Volcano.app/Contents/MacOS/volcano
	cp packaging/Info.plist build/Volcano.app/Contents/Info.plist
	@mkdir -p build/Volcano.app/Contents/Resources
	rsync -a --exclude='.DS_Store' --exclude='Info.plist' "$(ASSETS)/" build/Volcano.app/Contents/Resources/
run: all
	./build/volcano
test: all
	SDL_VIDEODRIVER=dummy ./build/volcano --software --hidden --self-test
build/volcano-sanitize: $(SOURCES) src/volcano.h src/campaign_checks.h src/missions_4_6_checks.h vendor/stb_image.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)
sanitize: build/volcano-sanitize
	SDL_VIDEODRIVER=dummy ./build/volcano-sanitize --software --hidden --self-test
clean:
	rm -rf build
