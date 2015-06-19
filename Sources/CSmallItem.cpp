#include "NetClientImpl.h"
#include "CSmallItem.h"
#include "MapClass.h"
#include "sync_random.h"
#include "ItemFabric.h"

CMeat::CMeat(size_t id) : SmallItem(id)
{
    SetSprite("icons/meat.png");
    name = "Meat";
};

void CWeed::process()
{
    SmallItem::process();
};

CWeed::CWeed(size_t id) : SmallItem(id)
{
    v_level = 2;
    SetSprite("icons/shards.dmi");

    unsigned int val = get_rand() % 3;
    std::string st;
    if (val == 0)
        st = "large";
    if (val == 1)
        st = "medium";
    if (val == 2)
        st = "small";
    SetState(st);
    name = "Weed";
};

CupItem::CupItem(size_t id) : SmallItem(id)
{
    SetSprite("icons/cup.png");
    name = "Cup";
}
