CC ?= cc
PACKAGES = sdl2
ASSETS ?= reference/rescue-team
CPPFLAGS += $(shell pkg-config --cflags $(PACKAGES)) -Ivendor -Isrc -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -O2 -Wall -Wextra -Wpedantic
LDLIBS += $(shell pkg-config --libs $(PACKAGES)) -lm
GAME_SOURCES = src/game.c src/game_util.c src/game_rules.c src/game_spawn.c src/game_navigation.c src/game_jobs.c src/game_sim.c src/game_render.c src/game_input.c
CORE_SOURCES = src/resources.c src/level.c src/campaign.c src/scene.c $(GAME_SOURCES)
TEST_SOURCES = tests/game_selftest.c tests/game_architecture.c
CORE_HEADERS = src/volcano.h src/game_internal.h
HEADERS = $(CORE_HEADERS) tests/game_test.h
AUDIT_BINARY ?= build/audit_levels
SOURCES = src/main.c $(CORE_SOURCES)
TEST_PACK = build/packs/test
HD_ASSETS = build/hd-assets
CHARACTER_ASSETS = build/kenney-assets

.PHONY: test-retail test-missions
.PHONY: all app run run-emerald-falls run-forest-mayday test test-levels level-example emerald-falls forest-mayday forest-mayday-app analyze-levels sanitize clean
.PHONY: hd-assets hd-levels hd-app run-emerald-falls-hd run-forest-mayday-hd test-hd character-assets
.PHONY: hd-pivot-previews
.PHONY: ancient-echoes ancient-echoes-app test-ancient-echoes
.PHONY: twisted-tracks twisted-tracks-app test-twisted-tracks
.PHONY: run-ancient-echoes run-twisted-tracks library-rescue library-rescue-app run-library-rescue test-library-rescue
all: build/volcano
build/volcano: $(SOURCES) $(CORE_HEADERS) vendor/stb_image.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)
app: build/volcano
	@mkdir -p build/Volcano.app/Contents/MacOS
	cp build/volcano build/Volcano.app/Contents/MacOS/volcano
	cp packaging/Info.plist build/Volcano.app/Contents/Info.plist
	@mkdir -p build/Volcano.app/Contents/Resources
	rsync -a --exclude='.DS_Store' --exclude='Info.plist' "$(ASSETS)/" build/Volcano.app/Contents/Resources/
	@mkdir -p build/Volcano.app/Contents/Resources/texture/particles
	cp assets/particles/log-*.png assets/particles/README.md assets/particles/source/Kenney-license.txt build/Volcano.app/Contents/Resources/texture/particles/
run: all
	./build/volcano
run-emerald-falls: emerald-falls
run-forest-mayday: forest-mayday
run-ancient-echoes: ancient-echoes
run-twisted-tracks: twisted-tracks
run-library-rescue: library-rescue
test: all test-levels test-retail
test-retail: build/test_retail
	SDL_VIDEODRIVER=dummy ./build/test_retail "$(ASSETS)"
build/test_retail: tests/retail_main.c $(CORE_SOURCES) $(TEST_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/retail_main.c $(CORE_SOURCES) $(TEST_SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)
$(TEST_PACK): tools/make_test_pack.py
	python3 tools/make_test_pack.py --output $@
	@touch $@
# Retail-free regression suite: placeholder art, the project's own missions.
test-missions: $(TEST_PACK)
	$(MAKE) test-levels ASSETS="$(TEST_PACK)"
build/check_custom_level: tests/check_custom_level.c $(CORE_SOURCES) $(TEST_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/check_custom_level.c $(CORE_SOURCES) $(TEST_SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)
$(AUDIT_BINARY): tools/audit_levels.c $(CORE_SOURCES) $(CORE_HEADERS) vendor/stb_image.h
	@mkdir -p "$(@D)"
	$(CC) $(CPPFLAGS) $(CFLAGS) tools/audit_levels.c $(CORE_SOURCES) -o "$@" $(LDFLAGS) $(LDLIBS)
build/levels/first-rescue.lvl: levels/first-rescue.level.json tools/level_source.py
	python3 tools/level_source.py compile $< $@
build/levels/first-rescue.lvl.rules: build/levels/first-rescue.lvl
	@test -f "$@" || python3 tools/level_source.py compile levels/first-rescue.level.json build/levels/first-rescue.lvl
level-example: all build/levels/first-rescue.lvl.rules
	./build/volcano --assets "$(ASSETS)" --level-file build/levels/first-rescue.lvl
SCENE_IMAGES = levels/emerald-falls.png
build/levels/emerald-falls.lvl: levels/emerald-falls.level.json $(SCENE_IMAGES) tools/level_source.py
	python3 tools/level_source.py compile $< $@
build/levels/emerald-falls.lvl.rules build/levels/emerald-falls.png: build/levels/emerald-falls.lvl
	@test -f "$@" || python3 tools/level_source.py compile levels/emerald-falls.level.json build/levels/emerald-falls.lvl
emerald-falls: all build/levels/emerald-falls.lvl.rules build/levels/emerald-falls.png
	./build/volcano --assets "$(ASSETS)" --level-file build/levels/emerald-falls.lvl
build/levels/forest-mayday.lvl: levels/forest-mayday.level.json levels/forest-mayday.png tools/level_source.py
	python3 tools/level_source.py compile $< $@
build/levels/forest-mayday.lvl.rules build/levels/forest-mayday.png: build/levels/forest-mayday.lvl
	@test -f "$@" || python3 tools/level_source.py compile levels/forest-mayday.level.json build/levels/forest-mayday.lvl
forest-mayday: all build/levels/forest-mayday.lvl.rules build/levels/forest-mayday.png
	./build/volcano --assets "$(ASSETS)" --level-file build/levels/forest-mayday.lvl
forest-mayday-app: all build/levels/forest-mayday.lvl.rules build/levels/forest-mayday.png
	python3 tools/package_custom_level.py "Forest Mayday" build/levels/forest-mayday.lvl "$(ASSETS)"
build/levels/ancient-echoes.lvl: levels/ancient-echoes.level.json levels/ancient-echoes.png tools/level_source.py
	python3 tools/level_source.py compile $< $@
build/levels/ancient-echoes.lvl.rules build/levels/ancient-echoes.png: build/levels/ancient-echoes.lvl
	@test -f "$@" || python3 tools/level_source.py compile levels/ancient-echoes.level.json build/levels/ancient-echoes.lvl
ancient-echoes: all build/levels/ancient-echoes.lvl.rules build/levels/ancient-echoes.png
	./build/volcano --assets "$(ASSETS)" --level-file build/levels/ancient-echoes.lvl
ancient-echoes-app: all build/levels/ancient-echoes.lvl.rules build/levels/ancient-echoes.png
	python3 tools/package_custom_level.py "Ancient Echoes" build/levels/ancient-echoes.lvl "$(ASSETS)"
test-ancient-echoes: build/check_custom_level build/levels/ancient-echoes.lvl.rules build/levels/ancient-echoes.png
	SDL_VIDEODRIVER=dummy ./build/check_custom_level "$(ASSETS)" build/levels/ancient-echoes.lvl --ancient
COASTER_IMAGES = levels/twisted-tracks-interior.png levels/twisted-tracks-debris.png
build/levels/twisted-tracks.lvl: levels/twisted-tracks.level.json $(COASTER_IMAGES) tools/level_source.py
	python3 tools/level_source.py compile $< $@
build/levels/twisted-tracks.lvl.rules build/levels/twisted-tracks-interior.png build/levels/twisted-tracks-debris.png: build/levels/twisted-tracks.lvl
	@test -f "$@" || python3 tools/level_source.py compile levels/twisted-tracks.level.json build/levels/twisted-tracks.lvl
twisted-tracks: all build/levels/twisted-tracks.lvl.rules build/levels/twisted-tracks-interior.png build/levels/twisted-tracks-debris.png
	./build/volcano --assets "$(ASSETS)" --level-file build/levels/twisted-tracks.lvl
twisted-tracks-app: all build/levels/twisted-tracks.lvl.rules build/levels/twisted-tracks-interior.png build/levels/twisted-tracks-debris.png
	python3 tools/package_custom_level.py "Twisted Tracks" build/levels/twisted-tracks.lvl "$(ASSETS)"
build/check_coaster_debris: tests/check_coaster_debris.c $(CORE_SOURCES) $(TEST_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/check_coaster_debris.c $(CORE_SOURCES) $(TEST_SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)
test-twisted-tracks: build/check_custom_level build/check_coaster_debris build/levels/twisted-tracks.lvl.rules build/levels/twisted-tracks-interior.png build/levels/twisted-tracks-debris.png
	SDL_VIDEODRIVER=dummy ./build/check_custom_level "$(ASSETS)" build/levels/twisted-tracks.lvl --coaster
	SDL_VIDEODRIVER=dummy ./build/check_coaster_debris "$(ASSETS)" build/levels/twisted-tracks.lvl
build/levels/library-rescue.lvl: levels/library-rescue.level.json levels/library-rescue.png levels/library-rescue-props.png tools/level_source.py
	python3 tools/level_source.py compile $< $@
build/levels/library-rescue.lvl.rules build/levels/library-rescue.png build/levels/library-rescue-props.png: build/levels/library-rescue.lvl
	@test -f "$@" || python3 tools/level_source.py compile levels/library-rescue.level.json build/levels/library-rescue.lvl
library-rescue: character-assets all build/levels/library-rescue.lvl.rules build/levels/library-rescue.png build/levels/library-rescue-props.png
	./build/volcano --assets "$(CHARACTER_ASSETS)" --level-file build/levels/library-rescue.lvl
library-rescue-app: character-assets all build/levels/library-rescue.lvl.rules build/levels/library-rescue.png build/levels/library-rescue-props.png
	python3 tools/package_custom_level.py "Library Rescue" build/levels/library-rescue.lvl "$(CHARACTER_ASSETS)"
test-library-rescue: character-assets build/check_custom_level build/levels/library-rescue.lvl.rules build/levels/library-rescue.png build/levels/library-rescue-props.png
	SDL_VIDEODRIVER=dummy ./build/check_custom_level "$(CHARACTER_ASSETS)" build/levels/library-rescue.lvl --library-rescue
hd-assets:
	python3 tools/build_hd_assets.py --assets "$(ASSETS)" --output "$(HD_ASSETS)"
	python3 tools/pack_rescue_characters.py --assets "$(HD_ASSETS)" --output "$(HD_ASSETS)"
hd-pivot-previews: hd-assets
	python3 tools/preview_hd_pivots.py --assets "$(HD_ASSETS)"
character-assets:
	python3 tools/pack_rescue_characters.py --assets "$(ASSETS)" --output "$(CHARACTER_ASSETS)"
hd-levels:
	python3 tools/build_hd_levels.py
hd-app: all hd-assets hd-levels
	python3 tools/package_hd_build.py "$(HD_ASSETS)"
run-emerald-falls-hd: all hd-assets hd-levels
	./build/volcano --assets "$(HD_ASSETS)" --level-file build/hd-levels/emerald-falls.lvl
run-forest-mayday-hd: all hd-assets hd-levels
	./build/volcano --assets "$(HD_ASSETS)" --level-file build/hd-levels/forest-mayday.lvl
test-hd: all hd-assets hd-levels build/check_custom_level build/test_retail
	$(MAKE) test-levels ASSETS="$(HD_ASSETS)"
	SDL_VIDEODRIVER=dummy ./build/check_custom_level "$(HD_ASSETS)" build/hd-levels/emerald-falls.lvl --emerald
	SDL_VIDEODRIVER=dummy ./build/check_custom_level "$(HD_ASSETS)" build/hd-levels/forest-mayday.lvl --mayday
	SDL_VIDEODRIVER=dummy ./build/test_retail "$(HD_ASSETS)"
analyze-levels:
	python3 tools/level_source.py analyze "$(ASSETS)/level" build/level-analysis.json
test-levels: all test-ancient-echoes test-twisted-tracks test-library-rescue build/check_custom_level build/levels/first-rescue.lvl.rules build/levels/emerald-falls.lvl.rules build/levels/emerald-falls.png build/levels/forest-mayday.lvl.rules build/levels/forest-mayday.png
	RESCUE_TEAM_ASSETS="$(ASSETS)" python3 -m unittest discover -s tests -p 'test_level_source.py'
	SDL_VIDEODRIVER=dummy ./build/check_custom_level "$(ASSETS)" build/levels/first-rescue.lvl
	SDL_VIDEODRIVER=dummy ./build/check_custom_level "$(ASSETS)" build/levels/emerald-falls.lvl --emerald
	SDL_VIDEODRIVER=dummy ./build/check_custom_level "$(ASSETS)" build/levels/forest-mayday.lvl --mayday
build/test_retail-sanitize: tests/retail_main.c $(CORE_SOURCES) $(TEST_SOURCES) $(HEADERS) vendor/stb_image.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/retail_main.c $(CORE_SOURCES) $(TEST_SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)
build/check_custom_level-sanitize: tests/check_custom_level.c $(CORE_SOURCES) $(TEST_SOURCES) $(HEADERS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/check_custom_level.c $(CORE_SOURCES) $(TEST_SOURCES) -o $@ $(LDFLAGS) $(LDLIBS)
sanitize: build/test_retail-sanitize build/check_custom_level-sanitize build/levels/first-rescue.lvl.rules build/levels/emerald-falls.lvl.rules build/levels/emerald-falls.png build/levels/forest-mayday.lvl.rules build/levels/forest-mayday.png
	SDL_VIDEODRIVER=dummy ./build/test_retail-sanitize "$(ASSETS)"
	SDL_VIDEODRIVER=dummy ./build/check_custom_level-sanitize "$(ASSETS)" build/levels/first-rescue.lvl
	SDL_VIDEODRIVER=dummy ./build/check_custom_level-sanitize "$(ASSETS)" build/levels/emerald-falls.lvl --emerald
	SDL_VIDEODRIVER=dummy ./build/check_custom_level-sanitize "$(ASSETS)" build/levels/forest-mayday.lvl --mayday
clean:
	rm -rf build
