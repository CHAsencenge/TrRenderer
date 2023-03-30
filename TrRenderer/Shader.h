#pragma once
#include "Model.h"

typedef struct Cubemap
{
	TGAImage* faces[6];
}Cubemap_t;