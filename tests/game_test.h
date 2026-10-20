#ifndef GAME_TEST_H
#define GAME_TEST_H
#include "../src/game/game_internal.h"
/* Re-read after every public init/restart: successful loads replace the world. */
#define game (*game_current())
#define game_fail error_set
#define TEST_PATH 4100
/* Software renderer, content/ and the given asset mounts (lowest first). */
bool test_startup(const char *const *mounts,int count);
void test_shutdown(void);
bool test_frame(const char *name);
int test_entity(uint32_t id);
void test_click_entity(int index);
bool test_choose_building(int lot,int slot);
/* Current sprite name of an entity (static buffer). */
const char *test_sprite(int index);
bool game_selftest(void);
bool game_architecture_selftest(const char *mission);
#endif
