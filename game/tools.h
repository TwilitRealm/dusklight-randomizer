#pragma once

#include "dolphin/types.h"

bool playerIsInRoomStage(s32 room, const char* stage);
void checkTransformFromWolf();
u8 setNextWarashibeItem();
void offWarashibeItem(u8 item);
int initCreatePlayerItem(u32 item, u32 flag, const cXyz* pos, int roomNo, const csXyz* angle, const cXyz* scale);
int getStageID(const char* stage);
bool playerIsOnTitleScreen();
