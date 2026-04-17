#include "tools.h"
#include "stages.h"
#include "d/d_com_inf_game.h"
#include "d/actor/d_a_alink.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "f_op/f_op_actor_mng.h"

bool playerIsInRoomStage(s32 room, const char* stage)
{
    // Only check room if it is valid
    // Room numbers are normally stored as int8_t, so the highest positive value is 127
    if ((room < 0) || (room > 127))
    {
        return false;
    }

    if (room != dStage_roomControl_c::mStayNo)
    {
        return false;
    }

    // Only check stage if it is valid
    if (!stage)
    {
        return false;
    }

    return daAlink_c::checkStageName(stage);
}

void checkTransformFromWolf()
{
    if (dComIfGs_getTransformStatus())
    {
        daAlink_getAlinkActorClass()->procCoMetamorphoseInit();
    }
}

u8 setNextWarashibeItem()
{
    static const u8 questItemsList[] = {
        dItemNo_Randomizer_LETTER_e,
        dItemNo_Randomizer_BILL_e,
        dItemNo_Randomizer_WOOD_STATUE_e,
        dItemNo_Randomizer_IRIAS_PENDANT_e,
        dItemNo_Randomizer_HORSE_FLUTE_e
    };

    u32 listLength = sizeof(questItemsList) / sizeof(questItemsList[0]);

    u8 newItem = 0xFF; // null by default

    for (u32 i = 0; i < listLength; i++)
    {
        const u32 item = questItemsList[i];
        const u8 slotItem = dComIfGs_getItem(21, 0);
        if (item == slotItem)
        {
            newItem = item;
            u32 j = i;
            do
            {
                j = (j + 1) % listLength; // Move to next index, wrapping around if needed.
                if (checkItemGet(questItemsList[j], 1))
                {
                    newItem = questItemsList[j];
                    break;
                }
            } while (j != i);

            // If the item to switch to is the same as the current item and we don't have the item anymore, null the slot
            if ((newItem == item) && !checkItemGet(item, 1))
            {
                newItem = 0xFF;
            }
            dComIfGs_setItem(21, newItem);

            break;
        }
    }
    return newItem;
}

void offWarashibeItem(u8 item)
{
    g_dComIfG_gameInfo.info.getSavedata().getPlayer().getGetItem().offFirstBit(item);
    setNextWarashibeItem();
}

int initCreatePlayerItem(u32 item, u32 flag, const cXyz* pos, int roomNo, const csXyz* angle, const cXyz* scale)
{
    u32 params = 0xFF0000 | ((flag & 0xFF) << 0x8) | (item & 0xFF);
    return fopAcM_create(539, params, pos, roomNo, angle, scale, -1);
}

int getStageID(const char* stage)
{
    int loopCount = sizeof(allStages) / sizeof(allStages[0]);
    for (int i = 0; i < loopCount; i++)
    {
        if (daAlink_c::checkStageName(allStages[i]))
        {
            return i;
        }
    }
    // Didn't find the current stage for some reason
    return -1;
}

bool playerIsOnTitleScreen() {
    // Player is either on title screen movie stage (S_MV000) or on title screen map layer 10
    return strcmp(dComIfGp_getStartStageName(), "S_MV000") == 0 ||
          (strcmp(dComIfGp_getStartStageName(), "F_SP102") == 0 && dComIfG_play_c::getLayerNo(0) == 10);
}