CC ?= cc
PACKAGES = sdl2
ASSETS ?= reference/rescue-team
CPPFLAGS += $(shell pkg-config --cflags $(PACKAGES)) -Ivendor -Isrc -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS += $(shell pkg-config --libs $(PACKAGES)) -lm
SOURCES = src/main.c src/resources.c src/level.c src/campaign.c src/game.c

.PHONY: all app run test test-levels level-example emerald-falls analyze-levels sanitize clean
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
test: all test-levels
	SDL_VIDEODRIVER=dummy ./build/volcano --software --hidden --self-test
build/check_custom_level: tests/check_custom_level.c src/game.c src/resources.c src/level.c src/campaign.c src/volcano.h src/campaign_checks.h src/missions_4_6_checks.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/check_custom_level.c src/resources.c src/level.c src/campaign.c -o $@ $(LDFLAGS) $(LDLIBS)
build/levels/first-rescue.lvl: levels/first-rescue.level.json tools/level_source.py
	python3 tools/level_source.py compile $< $@
build/levels/first-rescue.lvl.rules: build/levels/first-rescue.lvl
	@test -f "$@" || python3 tools/level_source.py compile levels/first-rescue.level.json build/levels/first-rescue.lvl
level-example: all build/levels/first-rescue.lvl.rules
	./build/volcano --assets "$(ASSETS)" --level-file build/levels/first-rescue.lvl
build/levels/emerald-falls.lvl: levels/emerald-falls.level.json levels/emerald-falls.png tools/level_source.py
	python3 tools/level_source.py compile $< $@
build/levels/emerald-falls.lvl.rules build/levels/emerald-falls.png: build/levels/emerald-falls.lvl
	@test -f "$@" || python3 tools/level_source.py compile levels/emerald-falls.level.json build/levels/emerald-falls.lvl
emerald-falls: all build/levels/emerald-falls.lvl.rules build/levels/emerald-falls.png
	./build/volcano --assets "$(ASSETS)" --level-file build/levels/emerald-falls.lvl
analyze-levels:
	python3 tools/level_source.py analyze "$(ASSETS)/level" build/level-analysis.json
test-levels: all build/check_custom_level build/levels/first-rescue.lvl.rules build/levels/emerald-falls.lvl.rules build/levels/emerald-falls.png
	RESCUE_TEAM_ASSETS="$(ASSETS)" python3 -m unittest discover -s tests -p 'test_level_source.py'
	SDL_VIDEODRIVER=dummy ./build/check_custom_level "$(ASSETS)" build/levels/first-rescue.lvl
	SDL_VIDEODRIVER=dummy ./build/check_custom_level "$(ASSETS)" build/levels/emerald-falls.lvl --emerald
build/volcano-sanitize: $(SOURCES) src/volcano.h src/campaign_checks.h src/missions_4_6_checks.h vendor/stb_image.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)
build/check_custom_level-sanitize: tests/check_custom_level.c src/game.c src/resources.c src/level.c src/campaign.c src/volcano.h src/campaign_checks.h src/missions_4_6_checks.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/check_custom_level.c src/resources.c src/level.c src/campaign.c -o $@ $(LDFLAGS) $(LDLIBS)
sanitize: build/volcano-sanitize build/check_custom_level-sanitize build/levels/first-rescue.lvl.rules build/levels/emerald-falls.lvl.rules build/levels/emerald-falls.png
	SDL_VIDEODRIVER=dummy ./build/volcano-sanitize --software --hidden --self-test
	SDL_VIDEODRIVER=dummy ./build/check_custom_level-sanitize "$(ASSETS)" build/levels/first-rescue.lvl
	SDL_VIDEODRIVER=dummy ./build/check_custom_level-sanitize "$(ASSETS)" build/levels/emerald-falls.lvl --emerald
clean:
	rm -rf build
