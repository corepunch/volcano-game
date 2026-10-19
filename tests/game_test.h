#ifndef GAME_TEST_H
#define GAME_TEST_H
#include "game_internal.h"
/* Re-read after every public init/restart: successful loads replace the world. */
#define game (*game_current())
bool game_architecture_selftest(void);
bool test_frame(const char *name);
int test_entity(uint32_t seed);
void test_click_entity(int index);
bool test_choose_building(int lot,int type);
#endif
