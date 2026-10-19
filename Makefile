# Volcano build.
#
# Layers: core -> assets -> content -> sim build into libvolcano_core.a WITHOUT
# SDL headers on the include path, so simulation and content code cannot come
# to depend on the renderer. render/ and game/ add SDL; app/ is the executable.
#
#   make                       game executable
#   make test                  retail-free suites (placeholder art)
#   make test-retail           retail suites (needs ASSETS)
#   make run MISSION=library-rescue
#   make run-campaign CAMPAIGN=3

CC ?= cc
AR ?= ar
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -MMD -MP
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Isrc -Ivendor
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)
LDLIBS += -lm

ASSETS ?= reference/rescue-team
MISSION ?= first-rescue
CAMPAIGN ?= 1
OUT ?= build
TEST_PACK := $(OUT)/packs/test
HD_PACK := $(OUT)/packs/hd
MISSION_ASSETS ?= $(TEST_PACK)
RUN := SDL_VIDEODRIVER=dummy

CORE_SOURCES := $(wildcard src/core/*.c src/assets/*.c src/content/*.c src/sim/*.c)
ENGINE_SOURCES := $(wildcard src/render/*.c src/game/*.c)
CORE_OBJECTS := $(CORE_SOURCES:%.c=$(OUT)/obj/%.o)
ENGINE_OBJECTS := $(ENGINE_SOURCES:%.c=$(OUT)/obj/%.o)
CORE_LIB := $(OUT)/libvolcano_core.a
ENGINE_LIB := $(OUT)/libvolcano_engine.a
TEST_SUPPORT := $(OUT)/obj/tests/test_support.o $(OUT)/obj/tests/architecture.o
MISSIONS := $(patsubst missions/%/mission.xml,%,$(wildcard missions/*/mission.xml))

.PHONY: all test test-core test-missions test-retail test-all run run-campaign run-hd hd-pack app sanitize analyze-levels clean
.DELETE_ON_ERROR:

all: $(OUT)/volcano

# SDL-free layers.
$(CORE_OBJECTS) $(OUT)/obj/tests/core_main.o: $(OUT)/obj/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Everything else may use SDL.
$(OUT)/obj/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(SDL_CFLAGS) $(CFLAGS) -c $< -o $@

$(CORE_LIB): $(CORE_OBJECTS)
	$(AR) rcs $@ $^
$(ENGINE_LIB): $(ENGINE_OBJECTS)
	$(AR) rcs $@ $^

$(OUT)/volcano: $(OUT)/obj/src/app/main.o $(ENGINE_LIB) $(CORE_LIB)
	$(CC) $(LDFLAGS) $^ -o $@ $(SDL_LIBS) $(LDLIBS)

# ---- Tests

$(TEST_PACK): tools/make_test_pack.py
	python3 tools/make_test_pack.py --output $@
	@touch $@

$(OUT)/test_core: $(OUT)/obj/tests/core_main.o $(CORE_LIB)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)
$(OUT)/test_missions: $(OUT)/obj/tests/missions_main.o $(TEST_SUPPORT) $(ENGINE_LIB) $(CORE_LIB)
	$(CC) $(LDFLAGS) $^ -o $@ $(SDL_LIBS) $(LDLIBS)
$(OUT)/test_coaster: $(OUT)/obj/tests/coaster_main.o $(TEST_SUPPORT) $(ENGINE_LIB) $(CORE_LIB)
	$(CC) $(LDFLAGS) $^ -o $@ $(SDL_LIBS) $(LDLIBS)
$(OUT)/test_retail: $(OUT)/obj/tests/retail_main.o $(OUT)/obj/tests/game_selftest.o $(TEST_SUPPORT) $(ENGINE_LIB) $(CORE_LIB)
	$(CC) $(LDFLAGS) $^ -o $@ $(SDL_LIBS) $(LDLIBS)

test: test-core test-missions

test-core: $(OUT)/test_core $(TEST_PACK)
	./$(OUT)/test_core $(TEST_PACK)

# The project's own missions, played through the real UI and scheduler.
test-missions: $(OUT)/test_missions $(OUT)/test_coaster $(TEST_PACK)
	$(RUN) ./$(OUT)/test_missions $(MISSION_ASSETS) missions/first-rescue/mission.xml
	$(RUN) ./$(OUT)/test_missions $(MISSION_ASSETS) missions/emerald-falls/mission.xml --emerald
	$(RUN) ./$(OUT)/test_missions $(MISSION_ASSETS) missions/forest-mayday/mission.xml --mayday
	$(RUN) ./$(OUT)/test_missions $(MISSION_ASSETS) missions/ancient-echoes/mission.xml --ancient
	$(RUN) ./$(OUT)/test_missions $(MISSION_ASSETS) missions/twisted-tracks/mission.xml --coaster
	$(RUN) ./$(OUT)/test_missions $(MISSION_ASSETS) missions/library-rescue/mission.xml --library-rescue
	$(RUN) ./$(OUT)/test_coaster $(MISSION_ASSETS) missions/twisted-tracks/mission.xml

# Original campaign, prototype scenes and retail art (not redistributable).
test-retail: $(OUT)/test_retail
	$(RUN) ./$(OUT)/test_retail "$(ASSETS)"
	$(MAKE) test-missions MISSION_ASSETS="$(ASSETS)"
	RESCUE_TEAM_ASSETS="$(ASSETS)" python3 -m unittest discover -s tests -p 'test_level_source.py'

test-all: test test-retail

sanitize:
	$(MAKE) OUT=build/asan CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="-fsanitize=address,undefined" test

# ---- Running

run: $(OUT)/volcano
	./$(OUT)/volcano --assets "$(ASSETS)" --mission missions/$(MISSION)/mission.xml

run-campaign: $(OUT)/volcano
	./$(OUT)/volcano --assets "$(ASSETS)" --campaign $(CAMPAIGN)

# HD statics are an overlay pack built from retail sprites plus authored art.
hd-pack:
	python3 tools/build_hd_assets.py --assets "$(ASSETS)" --output "$(HD_PACK)"

run-hd: $(OUT)/volcano hd-pack
	./$(OUT)/volcano --assets "$(ASSETS)" --pack "$(HD_PACK)" --hd --mission missions/$(MISSION)/mission.xml

# Self-contained macOS bundle: retail assets, content, missions and packs are
# copied in; the launcher only refers to paths inside the bundle.
app: $(OUT)/volcano
	python3 tools/package_app.py --binary $(OUT)/volcano --assets "$(ASSETS)" --mission "$(MISSION)" --output $(OUT)

# ---- Retail analysis

$(OUT)/audit_levels: $(OUT)/obj/tools/audit_levels.o $(ENGINE_LIB) $(CORE_LIB)
	$(CC) $(LDFLAGS) $^ -o $@ $(SDL_LIBS) $(LDLIBS)

analyze-levels:
	python3 tools/level_source.py analyze "$(ASSETS)/level" $(OUT)/level-analysis.json

clean:
	rm -rf build

-include $(shell find $(OUT)/obj -name '*.d' 2>/dev/null)
