// ============================================================================
//  CorpseLoot  —  RE_Kenshi / KenshiLib 플러그인
//
//  목적
//    시체 처리 용광로가 시체와 함께 소지품까지 없애버리는 것을 막는다.
//    소각 직전에 시체 인벤토리를 운반자에게 옮기고, 자리가 없으면 바닥에 떨군다.
//
//  v22 — cfg 핫리로드. 게임을 끄지 않고 CorpseLoot.cfg 를 고칠 수 있다.
//    유일한 후크(findCorpseDisposal)에서 3초에 한 번 파일 수정 시각을 보고,
//    바뀌었으면 LoadConfig 를 다시 돌린 뒤 반영값을 LC_INIT 로 남긴다.
//    시체 처리 평가가 돌 때만 재확인된다 — lootRange 를 만질 순간과 같은
//    순간이라 실용상 충분하다. 네 항목 전부 런타임 전역이라 전부 재적용된다.
//
//  v21 — 재뒤짐 방지 + 일회 진단 둘.
//    1. 최근에 턴 시체 8구를 기억한다. v20 은 마지막 한 구만 기억해서,
//       운반자가 둘 이상이면 A(시체1)→B(시체2)→A(시체1) 순으로 매번
//       "처음 보는 시체"가 되어 같은 시체를 계속 다시 뒤졌다
//       (호출마다 버퍼 2개 할당 + getEquipped* 2회 낭비 — findCorpseDisposal
//       은 초당 여러 번 불린다. 습격 뒷정리처럼 하역 인원이 여럿인 상황이
//       정상 케이스라 예외가 아니었다).
//    2. lektor 소유권 확인 로그 (첫 회수 때 한 줄, debug=0 에도 찍힘).
//       lektor.h(KEP 실물)로 저자의 전제는 확인했다: 생성자 new T[10],
//       reserve 가 delete[] 후 재할당, 소멸자 무조건 delete[] — 즉 게임이
//       키운 버퍼를 플러그인 힙에서 delete[] 하는 것을 전제로 짜여 있다.
//       "유지" 가 찍히면 v9 방식 무누수 확정, "교체" 면 그 호출만 게임
//       버퍼가 새는 것이다 (착용물 64개 초과 — 사실상 없음).
//    3. 스레드 확인 로그 한 줄. 이름 읽기(CharName)를 debug=0 에서 생략
//       — 점수 함수 경로라 매 호출 문자열 읽기는 낭비였다.
//
//  v20 — 아이템 용광로 자동 투입을 걷어냈다 (v10~v19 에서 만들었던 것).
//    FCS 로 훨씬 깔끔하게 풀렸다. 아이템 용광로를 복제해
//    itemtype limit 을 2(무기) / 3(방어구) 로 지정하면, 게임의 운반 잡이
//    그 건물을 목적지로 인식해 바닥의 장비를 알아서 넣고 활성화까지 한다.
//    바닐라 용광로가 limit=4(제한 없음)라 잡의 대상이 아니었던 것이 원인.
//    → BasusuFurnaceSplit.mod (인게임 확인 완료)
//    이 플러그인은 "시체에서 소지품을 빼내는" 일만 한다.
//
//    걷어내며 확인한 것들은 지식 문서에 남겼다:
//      Building::getInventory() 는 has inventory=0 인 건물에서 널
//      Inventory +0x80 / InventorySection +0xC0 이 소유 객체
//      섹션 이름 in1 / out, 격자 크기는 initialiseNewSection 이 정한다
//      getValueSingle(isPlayer=false) 가 화면의 "가격"
//
//  v9 — 메모리 누수 제거.
//    getEquippedArmour / getEquippedWeapons 에 빈 lektor 를 넘기면
//    게임이 할당하는데, lektor 소멸자가 익스포트에 없어 해제할 수 없었다.
//    이제 버퍼를 미리 잡아 넘기고 우리가 delete[] 한다.
//    (KEP 의 Modified/lektor.h 도 같은 방식이다)
//
//  v8 — debug=0 일 때 회수까지 멎던 문제 수정.
//    진단 전용이던 시절의 "!g_debug 면 즉시 반환"이 남아 있어서,
//    로그를 끄면 기능 자체가 동작하지 않았다.
//
//  v7 — cfg 를 읽는다 (enableLoot / lootRange / debug / logLimit).
//    파일이 없을 때만 만들고, 이후에는 덮어쓰지 않는다.
//    debug 기본값을 끔으로 바꿨다.
//
//  v6 — 등에 멘 가방까지 쓴다.
//    캐릭터 인벤토리와 가방은 별개 Inventory 객체다.
//    Character::hasABackpackOn() 으로 가방을 얻어 그쪽에도 넣어본다.
//    순서: 몸 -> 등짐 -> 바닥.
//
//  v5 — 처리장 근처에 왔을 때 회수한다 (lootRange, 기본 30).
//    v4 는 시체를 드는 즉시 털어서, 가방이 꽉 차면 아이템이 전장에 흩어졌다.
//    이제 처리장 앞에서 털므로 떨어진 것들이 한자리에 모인다.
//    FCS 의 Corpse disposal "use range" 가 25.0 이라 30 이면 넉넉하다.
//
//  v4 — 실제로 아이템을 옮긴다.
//    확인된 것: 착용 방어구·무기는 getAllItems 에 없다. 별도 목록으로 받아야 한다.
//      굶주린 도적 = 느슨한 것 0 / 방어구 4~5 / 무기 1
//      뼈뜰 늑대   = 전부 0 (동물은 착용물 없음)
//    죽은 비아군을 들고 있을 때, 시체 하나당 한 번만 회수한다.
//    운반자 가방에 넣고, 자리가 없으면 바닥에 떨군다.
//
//  v3 — 진단 전용. 착용 장비(방어구·무기)가 getAllItems 에 잡히는지 확인 추가.
//
//  v2 — 진단 전용. lektor 베이스 8바이트를 빠뜨려 아이템 개수가 쓰레기값이었다.
//
//  v1 — 진단 전용. 아무것도 옮기지 않는다.
//    소각 함수 자체는 익스포트에 없다. FurnaceBuilding 은 아이템 용광로(철 재활용)라
//    시체 용광로와 다른 물건이다. 그래서 "소각 순간"을 잡을 수 없고,
//    대신 그 앞 단계인 AI::findCorpseDisposal 을 후킹한다.
//    이 함수가 언제 불리는지, 그때 캐릭터가 시체를 들고 있는지,
//    그 시체가 소지품을 갖고 있는지를 먼저 확인해야 설계가 선다.
//
//  확인하려는 것
//    1. findCorpseDisposal 이 실제로 호출되는가
//    2. 시체를 든 뒤에도 호출되는가 (들기 전에만 불리면 이 지점은 못 쓴다)
//    3. 들고 있는 것에서 Character 와 인벤토리를 꺼낼 수 있는가
//
//  빌드: VS2022 x64 Release, 배포판 KenshiLib.lib 링크
//  설치: 활성 모드 폴더에 DLL + RE_Kenshi.json {"Plugins":["CorpseLoot.dll"]}
// ============================================================================

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// ---------------------------------------------------------------------------
//  게임 클래스 최소 선언
//  KenshiLib 이 익스포트한 것만 쓴다. 정의는 링크 시 채워진다.
// ---------------------------------------------------------------------------
namespace Ogre { class Vector3; }


// lektor 레이아웃.
//   Ogre::STLAllocator 베이스가 오프셋 0 에 8바이트를 차지한다 (헤더 주석 기준).
//   그다음 count(4) / maxSize(4) / stuff(8).
// v1 은 베이스를 빠뜨려서 count 를 8바이트 앞에서 읽었고,
// 그래서 아이템 개수가 -1375439000 같은 쓰레기값으로 나왔다.
// 이름이 lektor 여야 getAllItems 의 맹글링(?$lektor@PEAVItem@@)이 맞는다.
template <class T>
class lektor
{
public:
    char         _allocBase[8];   // Ogre::STLAllocator
    unsigned int count;
    unsigned int maxSize;
    T*           stuff;
};

// 가상함수(UEAA/UEBA)는 그대로 선언하면 vtable 호출이 되어 링크·동작이 어긋난다.
// KenshiLib 은 같은 함수의 비가상 직접호출판을 _NV_ 접두어로 함께 내보낸다.
// 그쪽을 쓴다.

class ContainerItem;
class RootObject;

class Item;   // 내용은 들여다보지 않는다. 포인터로만 다룬다.

// 실제 크기는 0x20. 여유를 둬 64바이트로 잡는다.
class hand
{
public:
    hand();                                  // ??0hand@@QEAA@XZ
    RootObject* getRootObject() const;       // ?getRootObject@hand@@QEBAPEAVRootObject@@XZ
    operator bool() const;                   // ??Bhand@@QEBA_NXZ
private:
    char _storage[64];
};

class Inventory
{
public:
    // ?getAllItems@Inventory@@QEBAAEBV?$lektor@PEAVItem@@@@XZ  (비가상, 참조 반환이라 안전)
    const lektor<Item*>& getAllItems() const;
    // 착용 중인 방어구·무기는 별도 목록으로 받는다.
    //   ?getEquippedArmour@Inventory@@QEAAXAEAV?$lektor@PEAVItem@@@@@Z
    //   ?getEquippedWeapons@Inventory@@QEAAXAEAV?$lektor@PEAVItem@@@@@Z
    // 출력 인자를 게임이 채운다. 버퍼를 미리 잡아 넘기면 우리가 해제할 수 있다
    // (LootCorpse 참조). 그래도 호출 자체가 싸지 않으니 시체당 한 번만 부른다.
    void getEquippedArmour(lektor<Item*>& out);
    void getEquippedWeapons(lektor<Item*>& out);
    bool  _NV_addItem(Item* item, int a, bool b, bool c);   // ?_NV_addItem@Inventory@@QEAA_NPEAVItem@@H_N1@Z
    void  _NV_dropItem(Item* item);                         // ?_NV_dropItem@Inventory@@QEAAXPEAVItem@@@Z
    // ?_NV_removeItemDontDestroy_returnsItem@Inventory@@QEAAPEAVItem@@PEAV2@H_N@Z
    Item* _NV_removeItemDontDestroy_returnsItem(Item* item, int a, bool b);
};

// 등에 멘 가방. 캐릭터 인벤토리와 별개의 Inventory 를 따로 들고 있다.
// 그래서 캐릭터 쪽에 addItem 해도 가방에는 들어가지 않는다.
class ContainerItem
{
public:
    Inventory* _NV_getInventory() const;  // ?_NV_getInventory@ContainerItem@@QEBAPEAVInventory@@XZ
};

class Character
{
public:
    // ?hasABackpackOn@Character@@QEBAPEAVContainerItem@@XZ  없으면 널
    ContainerItem* hasABackpackOn() const;
    bool isPlayerCharacter() const;   // ?isPlayerCharacter@Character@@QEBA_NXZ
    bool isDead() const;              // ?isDead@Character@@QEBA_NXZ
    hand getCarryingObject() const;   // ?getCarryingObject@Character@@QEBA?AVhand@@XZ
    Inventory* _NV_getInventory() const;  // ?_NV_getInventory@Character@@QEBAPEAVInventory@@XZ
};

class AI
{
public:
    Character* getCharacter();        // ?getCharacter@AI@@QEAAPEAVCharacter@@XZ (const 아님)
    // ?findCorpseDisposal@AI@@QEAAMAEBVhand@@AEAV2@_N@Z
    float findCorpseDisposal(const hand& subject, hand& out, bool justAsking);
    // ?isCarryingSomething@AI@@QEAA_NAEBVhand@@AEBVVector3@Ogre@@@Z
    bool isCarryingSomething(const hand& subject, const Ogre::Vector3& pos);
};

namespace KenshiLib
{
    // SleepFix 와 동일한 선언. 맹글링이 정확히 맞아야 링크된다.
    //   ?AddHook@KenshiLib@@YA?AW4HookStatus@1@PEAX0PEAPEAX@Z   (셋째 인자가 void**)
    //   ?GetRealAddress@KenshiLib@@YA_JPEAX@Z                    (__int64 반환)
    enum HookStatus { HOOK_UNKNOWN };
    HookStatus AddHook(void* target, void* hook, void** original);
    __int64    GetRealAddress(void* func);
}

// 멤버 함수 포인터를 void* 로 (SleepFix 에서 쓰던 것과 동일)
template <class T>
static void* PMF(T pmf)
{
    union { T in; void* out; } u;
    u.in = pmf;
    return u.out;
}

// ---------------------------------------------------------------------------
//  설정
//  값은 여기에만 둔다. cfg 는 현재 상태를 보여주는 기록일 뿐이다.
//  (SleepFix 에서 cfg 미반영으로 헛돈 판이 네 번 있었다)
// ---------------------------------------------------------------------------
static bool  g_enableLoot = true;   // 실제로 아이템을 옮긴다. 끄면 진단만 한다
static float g_lootRange  = 30.0f;  // 시체 처리장에서 이 거리 안일 때만 회수한다.
                                    // FCS 의 Corpse disposal "use range" 가 25.0 이다.
                                    // 0 이면 거리를 따지지 않고 드는 즉시 회수한다.
static bool g_debug    = false;   // 동작 기록. 필요할 때 cfg 에서 1 로 켠다
static int  g_logLimit = 200;     // 카테고리별 줄 수 상한

// ---------------------------------------------------------------------------
//  로그 — 직전과 같은 줄은 접는다. 점수 함수는 초당 여러 번 불린다.
// ---------------------------------------------------------------------------
enum LogCat { LC_INIT = 0, LC_FIND, LC_CARRY, LC_ITEM, LC_COUNT };

static int  g_written[LC_COUNT] = { 0 };
static char g_last[LC_COUNT][512] = { { 0 } };
static int  g_dup[LC_COUNT] = { 0 };

static void Log(int cat, const char* fmt, ...)
{
    if (g_logLimit <= 0 || cat < 0 || cat >= LC_COUNT) return;
    // debug=0 이면 시작 정보만 남긴다. 회수 기록도 멈춘다.
    if (!g_debug && cat != LC_INIT) return;
    if (g_written[cat] >= g_logLimit) return;

    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);

    if (strcmp(buf, g_last[cat]) == 0) { ++g_dup[cat]; return; }

    FILE* f = NULL;
    if (fopen_s(&f, "CorpseLoot.log", "a") != 0 || !f) return;
    if (g_dup[cat] > 0) { fprintf(f, "        (위 줄 %d회 반복)\n", g_dup[cat]); g_dup[cat] = 0; }
    for (int i = 0; i < LC_COUNT; ++i)
    {
        if (i == cat || g_dup[i] <= 0) continue;
        fprintf(f, "        (앞선 다른 줄 %d회 반복)\n", g_dup[i]);
        g_dup[i] = 0;
    }
    fprintf(f, "%s\n", buf);
    fclose(f);

    strcpy_s(g_last[cat], sizeof(g_last[cat]), buf);
    ++g_written[cat];
}

// ---------------------------------------------------------------------------
//  캐릭터 이름
//  Character + 0x18 부터 std::string (32바이트, 앞에 프록시 없음)
//    +0x00 SSO 버퍼 16 / 용량>=16 이면 힙 포인터,  +0x10 길이,  +0x18 용량
//  SleepFix 에서 실측으로 확정한 배치다. 틀릴 때를 대비해 SEH 로 감싼다.
// ---------------------------------------------------------------------------
static const char* CharName(Character* c, char* out, int outLen)
{
    if (!c) { strcpy_s(out, outLen, "널"); return out; }
    __try
    {
        const unsigned char* s = (const unsigned char*)c + 0x18;
        size_t len = *(const size_t*)(s + 0x10);
        size_t cap = *(const size_t*)(s + 0x18);
        const char* p = (cap < 16) ? (const char*)s : *(const char* const*)s;
        if (p && len > 0 && len <= 48)
        {
            int n = (int)len;
            if (n > outLen - 1) n = outLen - 1;
            memcpy(out, p, n);
            out[n] = 0;
            return out;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    sprintf_s(out, outLen, "chr@%p", (void*)c);
    return out;
}

static bool IsPlayerChar(Character* c)
{
    if (!c) return false;
    bool r = false;
    __try { r = c->isPlayerCharacter(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { r = false; }
    return r;
}

// ---------------------------------------------------------------------------
//  후크
// ---------------------------------------------------------------------------
typedef float (*FindCorpseFn)(AI*, const hand*, hand*, bool);
static FindCorpseFn origFindCorpse = 0;

// RootObjectBase::pos (+0x48). Character 도 Building 도 RootObjectBase 를
// 오프셋 0 에 두므로 같은 자리에서 읽을 수 있다.
static bool ReadPos(const void* rootObj, float out[3])
{
    if (!rootObj) return false;
    __try
    {
        const float* p = (const float*)((const unsigned char*)rootObj + 0x48);
        out[0] = p[0]; out[1] = p[1]; out[2] = p[2];
        // 켄시 좌표는 수만 단위까지 간다. 말이 안 되는 값이면 실패로 본다.
        for (int i = 0; i < 3; ++i)
            if (!(out[i] > -1.0e7f && out[i] < 1.0e7f)) return false;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}

static float FlatDistance(const float a[3], const float b[3])
{
    // 높이는 빼고 평면 거리로 본다. 층이 다른 경우를 감안한 것.
    float dx = a[0] - b[0];
    float dz = a[2] - b[2];
    float d2 = dx * dx + dz * dz;
    if (d2 <= 0.0f) return 0.0f;
    // sqrt 대신 근사 없이 그냥 계산 (자주 불리지 않는다)
    float r = d2, prev = 0.0f;
    for (int i = 0; i < 20 && r != prev; ++i) { prev = r; r = 0.5f * (r + d2 / r); }
    return r;
}

// ---------------------------------------------------------------------------
//  시체 소지품 회수
//
//  실측으로 확인된 것:
//    - 착용 방어구·무기는 getAllItems 에 안 들어간다. 별도 목록으로 받아야 한다.
//      (굶주린 도적: 느슨한 것 0개, 방어구 4~5개, 무기 1개)
//    - 동물은 착용물이 없다 (뼈뜰 늑대: 전부 0개)
//
//  [안전] 죽은 비아군만 대상으로 한다.
//    기절한 아군을 침대로 옮기는 중에 장비를 벗겨버리면 안 된다.
//
//  인자 의미는 KenshiLib 헤더에 이름이 남아 있어 확정했다:
//    addItem(item, quantity, dropOnFail, destroyOnFail)
//    removeItemDontDestroy_returnsItem(it, howmany, returnCopyIfSomeLeft)
//  v4 초안은 여기에 0 을 넣었는데, howmany=0 은 "0개를 꺼낸다"는 뜻이라
//  아무것도 안 옮겨지거나 이상하게 동작했을 것이다.
//  이제 Item::quantity(+0x12C)를 읽어 그 수량만큼 옮긴다.
//  dropOnFail=true 로 두면 자리가 없을 때 게임이 알아서 바닥에 떨군다.
// ---------------------------------------------------------------------------
static void LootCorpse(Character* carrier, Character* corpse,
                       const char* who, const char* cn, const void* disposalObj)
{
    Inventory* src = corpse->_NV_getInventory();
    Inventory* dst = carrier->_NV_getInventory();
    if (!src || !dst) return;

    // 옮길 목록을 먼저 복사해 둔다.
    // 옮기는 도중에 원본 목록이 바뀌므로 순회하면서 꺼내면 안 된다.
    Item* buf[256];
    int   n = 0;

    const lektor<Item*>& loose = src->getAllItems();
    if (loose.count <= 256u && loose.stuff)
        for (unsigned int i = 0; i < loose.count && n < 256; ++i)
            buf[n++] = loose.stuff[i];

    // 목록 버퍼를 우리가 미리 잡아서 넘긴다.
    // 빈 lektor 를 넘기면 게임이 할당하는데, lektor 소멸자가 익스포트에 없어
    // 우리가 해제할 방법이 없다 (누수). 미리 넉넉히 잡아 주면 게임이
    // 그 안에 채우기만 하므로 우리가 delete[] 로 정리할 수 있다.
    // KEP 도 같은 방식을 쓴다 (Modified/lektor.h 의 생성자가 new T[10] 을 잡는다).
    lektor<Item*> armour, weapons;
    memset(&armour, 0, sizeof(armour));
    memset(&weapons, 0, sizeof(weapons));
    Item** armourBuf  = new Item*[64];
    Item** weaponsBuf = new Item*[64];
    armour.maxSize  = 64; armour.stuff  = armourBuf;  armour.count  = 0;
    weapons.maxSize = 64; weapons.stuff = weaponsBuf; weapons.count = 0;

    src->getEquippedArmour(armour);
    src->getEquippedWeapons(weapons);

    // [일회 진단] 게임이 우리 버퍼를 그대로 쓰는지 확인한다 (debug=0 에도 찍힘).
    // "유지" = v9 방식 무누수 확정. "교체" = 그 호출은 게임 버퍼가 새는 것.
    static bool ownLogged = false;
    if (!ownLogged)
    {
        ownLogged = true;
        Log(LC_INIT, "lektor 소유권: 방어구 stuff=%s max=%u 개수=%u / 무기 stuff=%s max=%u 개수=%u",
            armour.stuff  == armourBuf  ? "유지" : "교체", armour.maxSize,  armour.count,
            weapons.stuff == weaponsBuf ? "유지" : "교체", weapons.maxSize, weapons.count);
    }

    if (armour.count <= 256u && armour.stuff)
        for (unsigned int i = 0; i < armour.count && n < 256; ++i)
            buf[n++] = armour.stuff[i];
    if (weapons.count <= 256u && weapons.stuff)
        for (unsigned int i = 0; i < weapons.count && n < 256; ++i)
            buf[n++] = weapons.stuff[i];

    if (n == 0)
    {
        if (armour.stuff  == armourBuf)  delete[] armourBuf;
        if (weapons.stuff == weaponsBuf) delete[] weaponsBuf;
        return;
    }

    // 가방은 캐릭터 인벤토리와 별개 객체다. 따로 얻어와야 한다.
    Inventory* bag = 0;
    __try
    {
        ContainerItem* bp = carrier->hasABackpackOn();
        if (bp) bag = bp->_NV_getInventory();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { bag = 0; }

    int moved = 0, inBag = 0, dropped = 0, failed = 0;
    for (int i = 0; i < n; ++i)
    {
        Item* it = buf[i];
        if (!it) continue;

        // 스택 수량. 값이 이상하면 1 로 본다.
        int q = 1;
        __try
        {
            int raw = *(const int*)((const unsigned char*)it + 0x12C);   // Item::quantity
            if (raw >= 1 && raw <= 100000) q = raw;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { q = 1; }

        Item* taken = src->_NV_removeItemDontDestroy_returnsItem(it, q, false);
        if (!taken) { ++failed; continue; }

        // 몸 인벤토리 -> 등짐 -> 바닥 순으로 시도한다.
        // 첫 시도에 dropOnFail=true 를 쓰면 가방을 써보기도 전에 떨어뜨린다.
        if (dst->_NV_addItem(taken, q, false, false))
            ++moved;
        else if (bag && bag->_NV_addItem(taken, q, false, false))
            ++inBag;
        else
        {
            dst->_NV_dropItem(taken);
            ++dropped;
        }
    }

    // 게임이 버퍼를 키웠다면 stuff 가 우리 것이 아니다.
    // 그 경우에만 해제를 포기한다 (착용물이 64개를 넘는 일은 사실상 없다).
    if (armour.stuff  == armourBuf)  delete[] armourBuf;
    if (weapons.stuff == weaponsBuf) delete[] weaponsBuf;

    Log(LC_ITEM, "[%s] %s 회수: 몸 %d개  등짐 %d개  바닥 %d개  실패 %d개 (대상 %d개)%s",
        who, cn, moved, inBag, dropped, failed, n, bag ? "" : "  [가방 없음]");
}

// 들고 있는 것을 들여다본다. 아직 아무것도 바꾸지 않는다.
static void InspectCarried(Character* carrier, const char* who,
                           const void* disposalObj)
{
    hand held = carrier->getCarryingObject();
    if (!held)
    {
        Log(LC_CARRY, "[%s] 아무것도 안 들고 있음", who);
        return;
    }

    RootObject* ro = held.getRootObject();
    if (!ro)
    {
        Log(LC_CARRY, "[%s] 들고 있는데 RootObject 가 널", who);
        return;
    }

    // 들고 있는 것이 캐릭터(시체)인지 확인한다.
    // RootObject -> Character 는 정식 경로가 없으므로, 우선 그대로 캐스팅해
    // isDead / 인벤토리 접근이 성립하는지 본다. 실패하면 SEH 로 잡힌다.
    Character* corpse = (Character*)ro;
    __try
    {
        char cn[64]; cn[0] = 0;
        if (g_debug) CharName(corpse, cn, sizeof(cn));   // 이름은 로그에만 쓴다
        bool dead = corpse->isDead();

        Inventory* inv = corpse->_NV_getInventory();
        int itemCount = -1;
        if (inv)
        {
            const lektor<Item*>& items = inv->getAllItems();
            unsigned int n = items.count;
            // 값이 터무니없으면 오프셋이 또 틀린 것이다. 쓰레기값을 그대로 믿지 않는다.
            itemCount = (n <= 512u) ? (int)n : -2;
        }

        if (g_debug)
        Log(LC_CARRY, "[%s] 운반 중: %s  사망=%d  인벤=%s  느슨한아이템=%d개%s",
            who, cn, dead ? 1 : 0, inv ? "있음" : "널", itemCount,
            itemCount == -2 ? " (개수가 이상함 — 오프셋 재확인 필요)" : "");

        // 죽은 비아군을 들고 있을 때만, 시체 하나당 한 번만 회수한다.
        // 같은 시체를 계속 훑으면 lektor 할당이 쌓이고 게임도 느려진다.
        //
        // 최근 8구를 기억한다. v20 은 마지막 한 구만 기억해서(lastLooted),
        // 운반자가 둘 이상이면 A(시체1)→B(시체2)→A(시체1) 순으로 매번
        // "처음 보는 시체"가 되어 같은 시체를 계속 다시 뒤졌다.
        static Character* recent[8] = { 0 };
        static int        recentIdx = 0;
        bool alreadyLooted = false;
        for (int ri = 0; ri < 8; ++ri)
            if (recent[ri] == corpse) { alreadyLooted = true; break; }

        if (g_enableLoot && inv && dead && !alreadyLooted
            && !IsPlayerChar(corpse))
        {
            // 처리장 근처에 왔을 때 회수한다.
            // 드는 즉시 털면 바닥에 떨어진 것들이 전장에 흩어진다.
            // 처리장 앞에서 털면 한자리에 모이고, 주워담기도 쉽다.
            bool inRange = true;   // near / far 는 MSVC 예약어라 쓸 수 없다
            float dist = -1.0f;
            if (g_lootRange > 0.0f)
            {
                float pc[3], pd[3];
                if (ReadPos(carrier, pc) && ReadPos(disposalObj, pd))
                {
                    dist = FlatDistance(pc, pd);
                    inRange = (dist <= g_lootRange);
                }
                else
                {
                    // 위치를 못 읽으면 거리 조건을 포기하고 그냥 회수한다.
                    // 아이템이 사라지는 것보다는 흩어지는 편이 낫다.
                    Log(LC_ITEM, "[%s] 위치를 못 읽어 거리 판정 생략", who);
                }
            }

            if (inRange)
            {
                recent[recentIdx] = corpse;
                recentIdx = (recentIdx + 1) & 7;
                Log(LC_ITEM, "[%s] %s 회수 시작 (처리장까지 %.1f)", who, cn, dist);
                LootCorpse(carrier, corpse, who, cn, disposalObj);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Log(LC_CARRY, "[%s] 운반물 해석 실패 (Character 가 아닐 수 있음) root=%p",
            who, (void*)ro);
    }
}

static void CheckConfigReload();   // v22. 정의는 LoadConfig 아래

static float hookFindCorpse(AI* self, const hand* subject, hand* out, bool justAsking)
{
    if (!origFindCorpse) return 0.0f;

    CheckConfigReload();   // v22. 자체 스로틀(3초) — 매 호출 비용은 u64 비교 하나.

    // [일회 진단] 어느 스레드에서 불리는지 (debug=0 에도 찍힘).
    // 한 줄이면 단일 스레드 = 락 없는 전역(recent, g_last 등)이 안전하다.
    static DWORD tid0 = 0;
    static bool  tidWarned = false;
    DWORD tid = GetCurrentThreadId();
    if (!tid0) { tid0 = tid; Log(LC_INIT, "스레드 확인: findCorpseDisposal = %lu", (unsigned long)tid); }
    else if (tid != tid0 && !tidWarned)
    {
        tidWarned = true;
        Log(LC_INIT, "[주의] 다른 스레드에서도 불림: %lu — 락 필요", (unsigned long)tid);
    }

    float r = origFindCorpse(self, subject, out, justAsking);

    // 로그가 꺼져 있어도 회수는 해야 한다.
    // v1 은 진단 전용이라 여기서 !g_debug 로 빠져나갔는데,
    // 나중에 회수 기능을 그 아래에 붙이면서 debug=0 이면 기능까지 멎었다.
    // 아무 일도 안 할 때만 빠져나간다.
    if (!g_enableLoot && !g_debug) return r;

    Character* c = 0;
    __try { c = self->getCharacter(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return r; }
    if (!IsPlayerChar(c)) return r;

    char who[64]; who[0] = 0;
    if (g_debug)
    {
        CharName(c, who, sizeof(who));   // 이름은 로그에만 쓴다. v20 은 매번 읽었다
        Log(LC_FIND, "[%s] findCorpseDisposal 결과=%.3f  justAsking=%d",
            who, r, justAsking ? 1 : 0);
    }

    // out 에 찾은 처리장이 담긴다. 거리 판정에 쓴다.
    const void* disposal = 0;
    __try
    {
        if (out && *out) disposal = (const void*)out->getRootObject();
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { disposal = 0; }

    InspectCarried(c, who, disposal);
    return r;
}

// ---------------------------------------------------------------------------
//  설정 기록 (읽지 않는다. 현재 값을 보여줄 뿐)
// ---------------------------------------------------------------------------
static void WriteConfigSnapshot();

// cfg 를 읽는다. 파일에 없는 항목은 코드 기본값을 쓴다.
// 파일이 없으면 기본값으로 하나 만들어 둔다.
// 한 번 만들어진 뒤에는 덮어쓰지 않으므로, 주석을 달거나 줄 순서를 바꿔도
// 그대로 유지된다.
static void LoadConfig()
{
    FILE* f = NULL;
    if (fopen_s(&f, "CorpseLoot.cfg", "r") != 0 || !f)
    {
        WriteConfigSnapshot();
        return;
    }

    char line[256] = { 0 };
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        char key[64] = { 0 }; float val = 0.0f;
        if (sscanf_s(line, "%63[^=]=%f", key, (unsigned)sizeof(key), &val) != 2) continue;

        if      (strcmp(key, "enableLoot") == 0) g_enableLoot = (val != 0.0f);
        else if (strcmp(key, "lootRange")  == 0) g_lootRange  = val;
        else if (strcmp(key, "debug")      == 0) g_debug      = (val != 0.0f);
        else if (strcmp(key, "logLimit")   == 0) g_logLimit   = (int)val;
    }
    fclose(f);
}

// --- v22 cfg 핫리로드. hookFindCorpse (게임 스레드) 에서만 부른다.
//     이 플러그인의 유일한 후크가 시체 처리 평가라서, 재적용 확인도
//     시체 처리 잡이 돌 때만 일어난다 — lootRange 조정이 필요한 순간과
//     정확히 같은 순간이므로 실용상 충분하다. ---
static void CheckConfigReload()
{
    static unsigned __int64 lastCheck = 0;
    static unsigned __int64 lastMtime = 0;

    unsigned __int64 now = GetTickCount64();
    if (now - lastCheck < 3000) return;
    lastCheck = now;

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA("CorpseLoot.cfg", GetFileExInfoStandard, &fad)) return;
    ULARGE_INTEGER u;
    u.LowPart  = fad.ftLastWriteTime.dwLowDateTime;
    u.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    if (u.QuadPart == 0) return;
    if (lastMtime == 0) { lastMtime = u.QuadPart; return; }   // 첫 확인은 기준점만
    if (u.QuadPart == lastMtime) return;
    lastMtime = u.QuadPart;

    LoadConfig();
    Log(LC_INIT, "[cfg 재적용] enableLoot=%d lootRange=%.1f debug=%d logLimit=%d",
        g_enableLoot ? 1 : 0, g_lootRange, g_debug ? 1 : 0, g_logLimit);
}

static void WriteConfigSnapshot()
{
    FILE* f = NULL;
    if (fopen_s(&f, "CorpseLoot.cfg", "w") != 0 || !f) return;
    fprintf(f, "# CorpseLoot 설정 - 저장하면 몇 초 안에 자동 반영된다 (게임 켠 채로,\n");
    fprintf(f, "# 단 시체 처리 잡이 돌고 있을 때만 재확인한다).\n\n");
    fprintf(f, "enableLoot=%d    # 0 이면 아무것도 옮기지 않고 기록만 한다\n", g_enableLoot ? 1 : 0);
    fprintf(f, "lootRange=%.1f  # 시체 처리장에서 이 거리 안에 들어오면 회수한다.\n", g_lootRange);
    fprintf(f, "                 # 아이템은 항상 운반자 발밑에 떨어지므로,\n");
    fprintf(f, "                 # 값이 작을수록 처리장 가까이에 모인다 (10 이면 바로 앞).\n");
    fprintf(f, "                 # 0 이면 거리를 안 따지고 시체를 드는 즉시 회수한다.\n");
    fprintf(f, "                 # FCS 의 Corpse disposal use range 는 25.0 이다.\n\n");
    fprintf(f, "debug=%d         # 1 이면 동작을 기록한다. 평소에는 0\n", g_debug ? 1 : 0);
    fprintf(f, "logLimit=%d     # 카테고리별 줄 수 상한\n", g_logLimit);
    fclose(f);
}

static void Install(const char* name, void* target, void* hook, void** orig)
{
    if (!target)
    {
        Log(LC_INIT, "  %-18s 주소 해석 실패", name);
        return;
    }
    KenshiLib::HookStatus st = KenshiLib::AddHook(target, hook, orig);
    Log(LC_INIT, "  %-18s addr=%p  status=%d  orig=%p", name, target, (int)st, *orig);
}

// ---------------------------------------------------------------------------
//  진입점  (RE_Kenshi 가 "?startPlugin@@YAXXZ" 로 찾는다 — extern "C" 금지)
//  선언에 dllexport 를 붙여야 심볼이 내보내진다. 없으면 RE_Kenshi 가
//  DLL 은 읽지만 진입점을 못 찾아 "Could not initialize plugin" 이 뜬다.
// ---------------------------------------------------------------------------
__declspec(dllexport) void startPlugin();

void startPlugin()
{
    LoadConfig();

    Log(LC_INIT, "CorpseLoot 시작");
    Log(LC_INIT, "  enableLoot=%d  lootRange=%.1f  debug=%d  logLimit=%d",
        g_enableLoot ? 1 : 0, g_lootRange, g_debug ? 1 : 0, g_logLimit);

    Install("시체처리장찾기",
            (void*)KenshiLib::GetRealAddress(PMF(&AI::findCorpseDisposal)),
            (void*)&hookFindCorpse, (void**)&origFindCorpse);

    Log(LC_INIT, "설치 완료. status 가 0 이 아니면 후킹 실패다.");
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    return TRUE;
}
