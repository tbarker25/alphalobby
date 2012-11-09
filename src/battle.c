#include <malloc.h>
#include <inttypes.h>
#include <unistd.h>

#include <windows.h>

#include "data.h"

#define ALLOC_STEP 10
#define LENGTH(x) (sizeof(x) / sizeof(*x))

static size_t nbBattles;
static Battle **battles;
Battle *gMyBattle;

Battle * FindBattle(uint32_t id)
{
	for (int i=0; i<nbBattles; ++i)
		if (battles[i] && battles[i]->id == id)
			return battles[i];
	return NULL;
}

Battle *NewBattle(void)
{
	int i=0;
	for (; i<nbBattles; ++i) {
		if (battles[i] == NULL)
			break;
	}
	if (i == nbBattles) {
		if (nbBattles % ALLOC_STEP == 0)
			battles = realloc(battles, (nbBattles + ALLOC_STEP) * sizeof(Battle *));
		++nbBattles;
	}
	battles[i] = calloc(1, sizeof(Battle));
	return battles[i];
}

void DelBattle(Battle *b)
{
	free(b);
	b = NULL;
}

