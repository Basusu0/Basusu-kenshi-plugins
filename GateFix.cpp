// ---------------------------------------------------------------------------
//  GateFix  —  RE_Kenshi / KenshiLib 플러그인
//
//  v2-b3 — 오탐·재발 수정판.
//
//  [v2-b2 실전 결함 2건 → 수정]
//    A. 오탐: 게이트 근처를 스치는 내부→내부 이동에도 게이트를 열었다 (경로선분
//       거리만으로는 "통과 예정"과 "근처 지나감"을 못 가른다).
//       → 정지 판정 도입: 주입 전에 250ms 간격 두 표본으로 캐릭터 이동량을 잰다.
//         움직이고 있으면 경로가 있었던 것 — 주입 취소. "막힌 캐릭터는 제자리"
//         (인게임 실측)를 판정 근거로 그대로 쓴다. 벽 방향 기하 추측 불필요.
//    B. 재발 불능: 취소된 대기 항목의 injected=true 가 남아, 같은 캐릭터의 새 클릭이
//       60초 타임아웃까지 침묵했다 (다시 잠그고 클릭 → 정지).
//       → 재클릭 시 대기 수명 전체 리셋 + 게이트 무관 새 이동은 대기 명시 소거.
//
//  v2-b2 — 자동 열기 (지연 주입판).
//
//  [v2-b 실측 → 설계 변경]
//    1. 수동 해제 계측: task=140, dest=NULL, subject=문짝 포인터, f1=0 f2=1.
//       14절의 "subject=게이트 객체" 기록의 실체는 문짝이었다 (문짝 발견 전 기록).
//       → subjectIsDoor 기본 1 로 확정.
//    2. 즉시 주입은 실패: 명령은 수동과 동일하게 나갔으나 캐릭터가 몸만 돌리고 정지.
//       원인 = 클릭 폭풍 — 클릭 1회가 이동명령 수십~수백 회(clear=1)로 들어와서
//       뒤 반복이 주입 명령을 지운다. → 감지 시엔 대기표 등록만, 주입은 틱에서
//       "마지막 감지 후 injectDelay(0.4초) 조용"해진 뒤에 낸다.
//    3. 주입 명령을 140(해제)→72(열기)로: 차단 조건이 닫힘이므로 목표 상태는 열림.
//       "열기 명령 하나가 해제+열기 자동 수행" (14절 인게임 확정) — 잠김/비잠김 공용.
//    4. 사용자 발견: 닫힌 게이트에 막힌 이동은 버려지는 게 아니라 보류되며,
//       문이 열리면 엔진이 스스로 재개한다. 우리 재발행은 이중 안전장치로 유지
//       (주입 72 가 clear=1 이라 보류 이동을 지울 수 있어서 재발행이 필요하다).
//
//  [동작]  닫힌 게이트 너머 맨땅 클릭 →
//    1) 이동명령은 그대로 통과시킨다 (엔진이 버리는 것도 그대로 — 동작 제거 0)
//    2) 경로상 닫힌 게이트를 찾으면 해제 명령(140)을 대신 낸다 → 캐릭터가 걸어가 연다
//    3) update4Frame 틱에서 열림량을 감시, 열리면 기억해둔 목적지로 이동 재발행
//    엔진의 Shift 큐는 웹·인게임 모두 불안정 확인 — 순서 제어는 플러그인이 쥔다.
//    (SleepFix v34 의 "후보 기록 → update4Frame 에서 명령" 2단 구조 재사용)
//
//  [v2-a2 실측 확정]
//    - 문짝(DoorStuff)↔게이트 대응: 문짝 ctor 부모2 == 게이트 건물 포인터
//    - 잠금 판독: 문짝 포인터에 isLocked/getDoorOpenAmount — 인게임 상태와 일치
//    - 차단 조건은 잠김이 아니라 닫힘(열림량 0) — 사용자 인게임 관찰
//    - 경로선분 최소거리 선정: 실클릭 전건 정답 게이트
//
//  [미확정 → cfg 스위치로 런타임 보정 (재빌드 불필요)]
//    - checkPlayerOrderForProblems 반환 의미: checkOK (기본 0 = "거짓이면 문제없음")
//      로그의 체크값이 아군 게이트에서 1로 나오면 checkOK=1 로 뒤집는다
//    - 140 의 subject 가 게이트 건물이냐 문짝이냐: subjectIsDoor (기본 0 = 게이트)
//      14절 실측 기록은 "게이트 객체"지만 문짝 발견 전 기록이라 포인터 정체 미상.
//      이번 판의 addOrder 계측이 수동 해제 1회로 확정해준다
//
//  [안전장치]
//    - enabled=0 이면 주입 전부 끔 (계측은 유지)
//    - 주입 전 checkPlayerOrderForProblems 로 엔진 자신에게 유효성 질의
//      (플레이어 팩션 게터 익스포트 부재 — 소유 판정을 엔진에 위임)
//    - 대기 항목 타임아웃 (기본 60초) + 재발행은 __try 격리
//    - 같은 문짝에 해제 명령은 1회만 (다중 선택 시 중복 주입 방지)
//
//  [v1 실측으로 확정된 것]
//    - 집 문 안쪽 클릭 → 이동명령에 building 전달 → isDoorLocked 1회 → 자동 해제.
//      게이트 너머 클릭(안→밖·밖→안 모두) → building=NULL, root=NULL.
//      맨땅 이동에서 엔진은 게이트를 아예 조회하지 않는다 (verbose=1로 "안 불림" 확정).
//    - scoreUnlockDoorHere · getDestinationGate: 세 케이스 모두 호출 0회.
//      점수 보정·탐색 관찰 가설 전부 기각.
//    - 원거리 집 문(거리 6,477)은 실패가 아니라 교착: AI 가 isDoorOpen=0/isDoorLocked=1
//      을 무한 반복 폴링. 별개 트랙 (게이트 건과 무관).
//    - 클릭 1회가 이동명령 수십~수백 회로 들어온다.
//
//  [v2 방향]  명령 시점 개입: building·root 가 NULL 인 이동명령에서
//    캐릭터→목적지 경로의 아군 잠긴 게이트를 찾아 UNLOCK_DOOR_PLAYER_ORDER(140)
//    선주입 (인자 실측 완료: dest=NULL, subject=게이트, f1=0 f2=1, pos=0).
//    Character→AI 익스포트가 없어 getHomeGate/getDestinationGate 경로는 폐기.
//    게이트 핸들은 GatewayBuilding 생성자 후킹으로 수집한다.
//
//  [이 판(v2-a)이 재는 것 — 주입 전 검증 3가지]
//    ⓐ 생성자 후크가 세이브 로드 시점의 게이트를 실제로 잡는가 (몇 개나)
//    ⓑ DoorStuff·Building 메서드를 GatewayBuilding 포인터에 그대로 불러도 되는가
//       (_NV_isGate 반환이 자기 포인터와 같으면 상속 오프셋 0 확정)
//    ⓒ NULL 이동명령 때 후보 선정(경로선분 거리)이 맞는 게이트를 고르는가
//       + 아군/타군 게이트가 faction 포인터로 구분되는가
//
//  후킹·호출 대상 (전부 익스포트 실물. 추측 없음):
//    ??0GatewayBuilding / ??1GatewayBuilding                 생성·소멸 (등록부)
//    ?isLocked@DoorStuff@@QEBA_NXZ                           잠김 (비가상 const)
//    ?_NV_hasDoorLock@DoorStuff@@QEBA_NXZ                    잠금장치 유무
//    ?_NV_getFaction@DoorStuff@@QEBAPEAVFaction@@XZ          소유 팩션
//    ?_NV_isGate@Building@@QEAAPEAVGatewayBuilding@@XZ       게이트 여부 (동일성 검증용)
//    (v1 의 이동명령·판정 후크는 유지)
//
//  빌드: VS2022 x64 Release, 배포판 KenshiLib.lib 링크 (다른 셋과 동일)
// ---------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>

namespace Ogre
{
    class Vector3
    {
    public:
        float x, y, z;
    };
}

// 캐릭터 위치. SleepFix 실증 심볼 그대로:
//   ?_NV_getPosition@RootObjectBase@@QEAA?AVVector3@Ogre@@XZ
//   (반환 타입 이름을 바꾸면 링크가 깨진다 — 사고 기록의 맹글링 실수 1번)
class RootObjectBase
{
public:
    Ogre::Vector3 _NV_getPosition();
};

class RootObject;
class Faction;
class GatewayBuilding;

// 게이트 여부 판정. 등록된 포인터를 Building* 로 보고 불렀을 때
// 자기 자신이 돌아오면 상속 체인 오프셋 0 확정 (검증 ⓑ).
//   ?_NV_isGate@Building@@QEAAPEAVGatewayBuilding@@XZ
class Building
{
public:
    GatewayBuilding* _NV_isGate();
};

// 게이트의 잠금·소유 조회. GatewayBuilding 이 DoorStuff 를 오프셋 0 으로
// 상속한다는 가정 하의 직접 호출 — 그 가정 자체가 이 판의 검증 대상이라
// 호출은 전부 __try 로 감싼다.
//   ?isLocked@DoorStuff@@QEBA_NXZ            (비가상 — vtable 무관)
//   ?_NV_hasDoorLock@DoorStuff@@QEBA_NXZ
//   ?_NV_getFaction@DoorStuff@@QEBAPEAVFaction@@XZ
enum DoorState { DOORSTATE_CLOSED, DOORSTATE_OPEN, DOORSTATE_OPENING, DOORSTATE_CLOSING };

class DoorStuff
{
public:
    bool           isLocked() const;          // ?isLocked@DoorStuff@@QEBA_NXZ
    bool           _NV_hasDoorLock() const;   // ?_NV_hasDoorLock@DoorStuff@@QEBA_NXZ
    Faction*       _NV_getFaction() const;    // ?_NV_getFaction@DoorStuff@@QEBAPEAVFaction@@XZ
    float          getDoorOpenAmount() const; // ?getDoorOpenAmount@DoorStuff@@QEBAMXZ
    unsigned short getGateCode();             // ?getGateCode@DoorStuff@@QEAAGXZ
    // 문 안팎 지점 — 엔진이 직접 주는 좌표라 벽 방향 추측이 불필요하다.
    //   ?getDoorPosInside@DoorStuff@@QEBAAEBVVector3@Ogre@@XZ
    //   ?getDoorPosOutside@DoorStuff@@QEBAAEBVVector3@Ogre@@XZ
    const Ogre::Vector3& getDoorPosInside() const;
    const Ogre::Vector3& getDoorPosOutside() const;
    // 직접 조작 (익스포트 실물 전부 확인):
    //   ?closeDoor@DoorStuff@@QEAA_NXZ · ?lockDoor@DoorStuff@@QEAAXXZ
    //   ?unlockDoor@DoorStuff@@QEAAXXZ · ?getDoorState@DoorStuff@@QEBA?AW4DoorState@@XZ
    //   ?setDoorState@DoorStuff@@QEAAXW4DoorState@@@Z · ?_forceDoorClosedUT@DoorStuff@@QEAA_NXZ
    bool closeDoor();
    void setDoorState(DoorState what);
    void lockDoor();
    void unlockDoor();
    DoorState getDoorState() const;
    bool _forceDoorClosedUT();
};

class hand
{
public:
    hand();                                  // ??0hand@@QEAA@XZ
    RootObject* getRootObject() const;       // ?getRootObject@hand@@QEBAPEAVRootObject@@XZ
    Building*   getBuilding() const;         // ?getBuilding@hand@@QEBAPEAVBuilding@@XZ
    bool        isNull() const;              // ?isNull@hand@@QEBA_NXZ
private:
    char _storage[64];
};

// 명령 주입용. 맹글링 W4TaskType@@ 에 맞추기 위한 전역 enum (값은 캐스팅으로 넣는다).
enum TaskType : int { TT_NONE = 0 };
#define TT_OPEN_DOOR   72    // OPEN_DOOR_PLAYER_ORDER — 열기 하나가 해제+열기를 수행한다 (실측)
#define TT_CLOSE_DOOR  73
#define TT_LOCK_DOOR   77
#define TT_UNLOCK_DOOR 140

// lektor 최소 선언 — CorpseLoot 검증 레이아웃 그대로.
// _allocBase 8바이트를 빠뜨리면 count 가 쓰레기값이 된다 (CorpseLoot v2 사고 기록).
// 이름이 lektor 여야 맹글링(?$lektor@PEAVRootObject@@)이 맞는다.
template <class T>
class lektor
{
public:
    char         _allocBase[8];   // Ogre::STLAllocator
    unsigned int count;
    unsigned int maxSize;
    T*           stuff;
};

// 근처 캐릭터 열거 (v3-a 계측 대상 — bool 인자 의미·반환물 구성 미검증).
//   ?_NV_getCharactersInArea@Faction@@QEAAXAEAV?$lektor@PEAVRootObject@@@@AEBVVector3@Ogre@@M_N@Z
class Faction
{
public:
    void _NV_getCharactersInArea(lektor<RootObject*>& out, const Ogre::Vector3& center,
                                 float radius, bool flag);
};

// 명령 주입·유효성 질의. 둘 다 익스포트 실물:
//   ?addOrder@Character@@QEAAXPEAVBuilding@@W4TaskType@@PEAVRootObject@@_N3AEBVVector3@Ogre@@@Z
//   ?checkPlayerOrderForProblems@Character@@QEAA_NW4TaskType@@PEAVRootObject@@@Z
class CharacterAnimal;

class Character
{
public:
    void addOrder(Building* dest, TaskType t, RootObject* subject,
                  bool f1, bool f2, const Ogre::Vector3& pos);
    bool checkPlayerOrderForProblems(TaskType t, RootObject* subject);
};

namespace KenshiLib
{
    // 다른 셋과 동일 선언. 맹글링이 정확히 맞아야 링크된다.
    enum HookStatus { HOOK_UNKNOWN };
    HookStatus AddHook(void* target, void* hook, void** original);
    __int64    GetRealAddress(void* func);
}

// ---------------------------------------------------------------------------
//  설정 (cfg 는 현재 상태 기록. 값의 출처는 여기다)
// ---------------------------------------------------------------------------
static bool g_debug    = false;   // 상용 기본값. 진단이 필요하면 cfg 로 1
static int  g_logLimit = 400;     // 카테고리별 줄 수 상한
static bool g_verbose  = false;   // 1 이면 판정 함수 호출을 전부 찍는다 (매우 많다)

// v2-b 주입 설정 (전부 핫리로드 — 후크 설치와 무관)
static bool  g_enabled       = true;    // 0 이면 주입 전부 끔 (계측은 유지)
static float g_maxDetour    = 1500.0f; // 이 문을 거치느라 더 걷는 거리가 이보다 크면 무관한 문으로 본다
static float g_pathDist      = 2000.0f; // (느슨한 상한) 경로선분과 게이트의 거리. 분리선이 아니라 안전장치다
static float g_openThreshold = 0.9f;    // 열림량이 이 미만이면 "닫힘". 재발행 문턱이기도 하다 —
                                        // 0.5 는 열리는 중에 재발행이 붙어 개방을 끊었다(문만 열고 정지). 실측 0.9 무결점.
// v3-b 자동 닫기
static bool  g_autoClose     = true;    // 우리가 연 문을 사람이 없어지면 닫는다
static float g_closeRadius   = 110.0f;   // 이 반경에 아군이 없으면 "비었다" (경비 위치는 이 밖에)
                                        // (0 이어도 동물은 제자리 대기하다 문이 열리면 엔진이 자동 재개한다 — 실측)
static float g_closeGrace    = 10.0f;   // 마지막 통과 시작 후 이만큼은 무조건 안 닫는다(초)
static float g_closeDelay    = 3.0f;    // 비어 있는 상태가 이만큼 지속되면 닫는다(초)
static float g_closeGiveUp   = 300.0f;  // 이 시간이 지나도록 못 닫으면 포기(초) — 바닐라처럼 열린 채 둔다
static float g_pendingWait   = 60.0f;   // 대기 항목 타임아웃(초) — 캐릭터가 쓰러지는 등 실패 대비
static float g_injectDelay   = 0.4f;    // 마지막 감지 후 이만큼 조용하면 주입(초) — 클릭 폭풍 회피
static float g_moveEps       = 8.0f;    // 250ms 표본 간 이동이 이보다 크면 "막힌 게 아니다" — 주입 취소
                                        // (3.0 은 무리 이동 충돌 밀림에 오판 — 도착 판정이 생겨 느슨해도 된다)
static float g_arriveEps     = 20.0f;   // 정지 캐릭터가 목적지에서 이 거리 안이면 "도착"으로 보고 주입 취소

// v3-a 계측 (근처 캐릭터 조회 — 닫기 없음, 수치만)
static bool  g_areaFlag      = false;   // getCharactersInArea 의 bool 인자 (의미 미확정 — 로그 대조용)

// ---------------------------------------------------------------------------
//  로그  (CorpseLoot 규약: 카테고리별 상한 + 연속 중복 접기)
// ---------------------------------------------------------------------------
enum { LC_INIT = 0, LC_MOVE, LC_SCORE, LC_GATE, LC_JUDGE, LC_REG, LC_ORDER, LC_ACT, LC_COUNT };
static int  g_written[LC_COUNT] = { 0 };
static int  g_dup[LC_COUNT]     = { 0 };
static char g_last[LC_COUNT][512] = { { 0 } };

static void Log(int cat, const char* fmt, ...)
{
    if (g_logLimit <= 0 || cat < 0 || cat >= LC_COUNT) return;
    if (!g_debug && cat != LC_INIT) return;
    if (g_written[cat] >= g_logLimit)
    {
        // 상한에 닿았다는 사실을 딱 한 번 남긴다 — 안 그러면 "로그가 없다"를
        // "그 일이 일어나지 않았다"로 오독한다 (실제로 두 번 그랬다)
        if (g_written[cat] == g_logLimit)
        {
            ++g_written[cat];
            FILE* fl = NULL;
            if (fopen_s(&fl, "GateFix.log", "a") == 0 && fl)
            {
                fprintf(fl, "        (이 종류는 상한 %d 도달 — 이후 생략. cfg logLimit 을 올리거나 게임을 재시작하라)\n", g_logLimit);
                fclose(fl);
            }
        }
        return;
    }

    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);

    if (strcmp(buf, g_last[cat]) == 0) { ++g_dup[cat]; return; }

    FILE* f = NULL;
    if (fopen_s(&f, "GateFix.log", "a") != 0 || !f) return;
    if (g_dup[cat] > 0)
    {
        fprintf(f, "        (위 줄 %d회 반복)\n", g_dup[cat]);
        g_dup[cat] = 0;
    }
    fprintf(f, "%s\n", buf);
    fclose(f);
    strcpy_s(g_last[cat], sizeof(g_last[cat]), buf);
    ++g_written[cat];
}

// 손잡이가 무엇을 가리키는지: 포인터 + vtable.
static void HandInfo(const hand& h, void** obj, void** vt)
{
    *obj = 0; *vt = 0;
    __try
    {
        Building* b = h.getBuilding();
        if (b) { *obj = (void*)b; *vt = *(void**)b; }
        else
        {
            RootObject* r = h.getRootObject();
            if (r) { *obj = (void*)r; *vt = *(void**)r; }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
}

static void PosOf(const Ogre::Vector3* p, float* x, float* y, float* z)
{
    *x = *y = *z = 0.0f;
    if (!p) return;
    __try
    {
        const float* v = (const float*)p;
        *x = v[0]; *y = v[1]; *z = v[2];
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
}

// ---------------------------------------------------------------------------
//  게이트 등록부  (검증 ⓐ)
//  생성자 후크로 채우고 소멸자 후크로 비운다. 지역 스트리밍으로 게이트가
//  로드/언로드를 반복하므로 등록부 유지보수는 debug 값과 무관하게 돈다.
//  게임 스레드에서만 생성·소멸·조회된다는 가정 (셋 다 같은 가정으로 작동 중).
// ---------------------------------------------------------------------------
#define MAX_GATES 256
struct GateEntry { void* ptr; float x, y, z; };
static GateEntry g_gates[MAX_GATES];
static int       g_gateCount = 0;
static int       g_gateTotalSeen = 0;   // 세션 누적 (언로드 포함)

static void GateAdd(void* p, float x, float y, float z)
{
    for (int i = 0; i < g_gateCount; ++i)
        if (g_gates[i].ptr == p) { g_gates[i].x = x; g_gates[i].y = y; g_gates[i].z = z; return; }
    // 세이브 재로드 대비 — 같은 자리에 새 게이트가 생기면 옛 항목은 죽은 포인터다.
    // (소멸자 후크가 세이브 로드 때 안 불린다는 실측. 죽은 문짝의 열림량이 1.00 으로
    //  읽혀 "이미 열림"으로 오판 → 아무도 안 움직이는 증상의 원인이었다)
    for (int i = g_gateCount - 1; i >= 0; --i)
    {
        float dx = g_gates[i].x - x, dz = g_gates[i].z - z;
        if (dx * dx + dz * dz < 25.0f)
        {
            Log(LC_REG, "[등록] 같은 자리의 옛 게이트 %p 제거 (새 %p 로 교체)", g_gates[i].ptr, p);
            g_gates[i] = g_gates[g_gateCount - 1];
            --g_gateCount;
        }
    }
    if (g_gateCount >= MAX_GATES) return;
    g_gates[g_gateCount].ptr = p;
    g_gates[g_gateCount].x = x; g_gates[g_gateCount].y = y; g_gates[g_gateCount].z = z;
    ++g_gateCount;
    ++g_gateTotalSeen;
}

static void GateRemove(void* p)
{
    for (int i = 0; i < g_gateCount; ++i)
        if (g_gates[i].ptr == p)
        {
            g_gates[i] = g_gates[g_gateCount - 1];
            --g_gateCount;
            return;
        }
}

// ---------------------------------------------------------------------------
//  문짝 등록부  (v2-a2)
//  DoorStuff 생성자 인자: (GameData*, 위치, 회전, Faction*, hand, hand,
//                          Layout*, Building* 부모1, Building* 부모2)
//  부모 포인터로 게이트와 대응시킨다. 정착지 건물이 많아 상한을 넉넉히.
// ---------------------------------------------------------------------------
#define MAX_DOORS 1024
struct DoorEntry { void* ptr; void* fac; void* layout; void* b1; void* b2; float x, y, z; };
static DoorEntry g_doors[MAX_DOORS];
static int       g_doorCount = 0;
static int       g_doorTotalSeen = 0;

static void DoorAdd(void* p, void* fac, void* layout, void* b1, void* b2,
                    float x, float y, float z)
{
    for (int i = 0; i < g_doorCount; ++i)
        if (g_doors[i].ptr == p)
        {
            g_doors[i].fac = fac; g_doors[i].layout = layout;
            g_doors[i].b1 = b1; g_doors[i].b2 = b2;
            g_doors[i].x = x; g_doors[i].y = y; g_doors[i].z = z;
            return;
        }
    // 같은 자리의 옛 문짝 제거 (게이트와 같은 이유)
    for (int i = g_doorCount - 1; i >= 0; --i)
    {
        float ddx = g_doors[i].x - x, ddz = g_doors[i].z - z;
        if (ddx * ddx + ddz * ddz < 25.0f)
        {
            Log(LC_REG, "[등록] 같은 자리의 옛 문짝 %p 제거 (새 %p 로 교체)", g_doors[i].ptr, p);
            g_doors[i] = g_doors[g_doorCount - 1];
            --g_doorCount;
        }
    }
    if (g_doorCount >= MAX_DOORS) return;
    g_doors[g_doorCount].ptr = p; g_doors[g_doorCount].fac = fac;
    g_doors[g_doorCount].layout = layout;
    g_doors[g_doorCount].b1 = b1; g_doors[g_doorCount].b2 = b2;
    g_doors[g_doorCount].x = x; g_doors[g_doorCount].y = y; g_doors[g_doorCount].z = z;
    ++g_doorCount;
    ++g_doorTotalSeen;
}

static void DoorRemove(void* p)
{
    for (int i = 0; i < g_doorCount; ++i)
        if (g_doors[i].ptr == p)
        {
            g_doors[i] = g_doors[g_doorCount - 1];
            --g_doorCount;
            return;
        }
}

static DoorEntry* DoorByPtr(void* p)
{
    for (int i = 0; i < g_doorCount; ++i)
        if (g_doors[i].ptr == p) return &g_doors[i];
    return 0;
}

static DoorEntry* DoorOfGate(void* gate)
{
    for (int i = 0; i < g_doorCount; ++i)
        if (g_doors[i].b1 == gate || g_doors[i].b2 == gate)
            return &g_doors[i];
    return 0;
}

// ---------------------------------------------------------------------------
//  대기표  (v2-b — 플러그인이 미니 큐 노릇을 한다)
//  "이 캐릭터는 이 문짝이 열리면 저기로 간다" 하나가 항목 하나.
//  다중 선택으로 여러 캐릭터가 같은 게이트에 걸리면 해제 명령은 첫 캐릭터만 내고
//  나머지는 대기만 한다. 열리는 순간 전원 재발행.
// ---------------------------------------------------------------------------
#define MAX_PENDING 128
struct PendingEntry
{
    void* charPtr;      // 재발행 대상 (스테일 포인터 위험 — 타임아웃과 __try 로 방어)
    void* doorPtr;      // 감시할 문짝
    float dx, dy, dz;   // 기억해둔 원래 목적지
    unsigned __int64 t;        // 등록 틱 (타임아웃용)
    unsigned __int64 lastSeen; // 마지막 감지 틱 — 클릭 폭풍이 끝났는지 판단
    bool  injected;            // 열기 명령을 이미 냈는가
    bool  sampled;             // 정지 판정 1차 표본을 떴는가
    bool  wasLocked;           // 감지 시점 잠김 여부 (v3 상태 복원용 — v3-a 는 기록만)
    float sx, sz;              // 1차 표본 좌표 (250ms 뒤 2차와 비교 — 움직이면 막힌 게 아니다)
    bool  hasLast;             // 수명 판정용 직전 좌표가 있는가
    float lastX, lastZ;        // 수명 판정용 직전 좌표
    unsigned __int64 stillT;   // 마지막으로 움직인 시각 (정지 지속 계산)
    // 주입 후 추적 (v3-b3 진단) — "명령을 못 받았나 / 받고도 실행을 안 했나"를 가른다
    unsigned __int64 injectT;  // 주입 시각
    float injX, injZ;          // 주입 시점 좌표
    float injDoorDist;         // 주입 시점 캐릭터→문 거리
    bool  moveLogged;          // 이동 시작을 이미 찍었는가
    bool  stallLogged;         // 정지 경고를 이미 찍었는가
    // 통과 추적 (v3-b5) — 재발행했다고 지우지 않는다. 실제로 문을 지날 때까지 들고 있어야
    // 자동 닫기가 "걸어오는 사람"을 무시하고 문을 닫아버리는 사고를 막는다.
    bool  reissued;            // 재발행 완료 = 이제 통과를 지켜본다
    int   sideAtDetect;        // 감지 시점 캐릭터가 문의 어느 편이었나 (1 안, 0 밖, -1 모름)
};
static PendingEntry g_pending[MAX_PENDING];
static int g_pendingCount = 0;

static void PendingAdd(void* ch, void* door, float dx, float dy, float dz, bool wasLocked)
{
    unsigned __int64 now = GetTickCount64();
    for (int i = 0; i < g_pendingCount; ++i)
        if (g_pending[i].charPtr == ch)
        {
            // 폭풍 반복 vs 진짜 재클릭 구분 (v3-a2 결함: 반복마다 전체 리셋해서
            // 8인 폭풍에서 일부 캐릭터가 "0.4초 조용"에 영영 못 도달 = 굶음,
            // 주입 후 리셋으로 중복 방지 기억도 소실 = 72 중복 발행).
            float ddx = dx - g_pending[i].dx, ddz = dz - g_pending[i].dz;
            bool sameRequest = (g_pending[i].doorPtr == door)
                            && (ddx * ddx + ddz * ddz < 25.0f);   // 목적지 차이 5 이내
            g_pending[i].lastSeen = now;
            if (sameRequest) return;                              // 상태 보존, 시각만 갱신

            // 새 요청 — 수명 전체 리셋 (v2-b3 의 재클릭 수정은 유지)
            g_pending[i].doorPtr = door;
            g_pending[i].dx = dx; g_pending[i].dy = dy; g_pending[i].dz = dz;
            g_pending[i].t = now;
            g_pending[i].injected = false;
            g_pending[i].sampled  = false;
            g_pending[i].wasLocked = wasLocked;
            g_pending[i].stillT = now;
            g_pending[i].hasLast = false;
            g_pending[i].moveLogged = false;
            g_pending[i].stallLogged = false;
            g_pending[i].reissued = false;
            return;
        }
    if (g_pendingCount >= MAX_PENDING) return;
    g_pending[g_pendingCount].charPtr = ch;
    g_pending[g_pendingCount].doorPtr = door;
    g_pending[g_pendingCount].dx = dx; g_pending[g_pendingCount].dy = dy; g_pending[g_pendingCount].dz = dz;
    g_pending[g_pendingCount].t = now;
    g_pending[g_pendingCount].lastSeen = now;
    g_pending[g_pendingCount].injected = false;
    g_pending[g_pendingCount].sampled  = false;
    g_pending[g_pendingCount].wasLocked = wasLocked;
    g_pending[g_pendingCount].stillT = now;
    g_pending[g_pendingCount].hasLast = false;
    g_pending[g_pendingCount].injectT = 0;
    g_pending[g_pendingCount].moveLogged = false;
    g_pending[g_pendingCount].stallLogged = false;
    g_pending[g_pendingCount].reissued = false;
    g_pending[g_pendingCount].sideAtDetect = -1;
    ++g_pendingCount;
}

// ---------------------------------------------------------------------------
//  감시 목록 (v3-a 계측) — 우리가 연 문. 근처 캐릭터 수·비용만 재고 닫지 않는다.
// ---------------------------------------------------------------------------
#define MAX_WATCH 8
struct WatchEntry
{
    void* doorPtr;
    bool  wasLocked;            // 우리가 열기 전 잠금 상태 — 닫은 뒤 이대로 복원한다
    unsigned __int64 startT;
    unsigned __int64 lastLogT;
    unsigned __int64 emptySince; // 닫기 반경이 빈 시각 (0 = 지금 사람이 있다)
    unsigned __int64 lastPassT;  // 마지막 재발행(통과 시작) 시각 — 유예 계산
    unsigned __int64 closeT;     // 닫기 실행 시각 (0 = 아직). 검증 로그를 2초 뒤에 찍는다
    int   closeStep;             // 시험 중인 닫기 방법 (1~3)
};
static WatchEntry g_watch[MAX_WATCH];
static int g_watchCount = 0;

static void WatchAdd(void* door, bool wasLocked)
{
    unsigned __int64 now = GetTickCount64();
    for (int i = 0; i < g_watchCount; ++i)
        if (g_watch[i].doorPtr == door)
        {
            g_watch[i].startT = now;                  // 추가 통과 — 감시 연장
            g_watch[i].lastPassT = now;
            g_watch[i].emptySince = 0;
            if (wasLocked) g_watch[i].wasLocked = true;
            return;
        }
    if (g_watchCount >= MAX_WATCH) return;
    g_watch[g_watchCount].doorPtr = door;
    g_watch[g_watchCount].wasLocked = wasLocked;
    g_watch[g_watchCount].startT = now;
    g_watch[g_watchCount].lastLogT = 0;
    g_watch[g_watchCount].emptySince = 0;
    g_watch[g_watchCount].lastPassT = now;
    g_watch[g_watchCount].closeT = 0;
    g_watch[g_watchCount].closeStep = 0;
    ++g_watchCount;
}

static void WatchRemoveAt(int i)
{
    g_watch[i] = g_watch[g_watchCount - 1];
    --g_watchCount;
}

static void PendingRemoveChar(void* ch)
{
    for (int i = g_pendingCount - 1; i >= 0; --i)
        if (g_pending[i].charPtr == ch)
        {
            g_pending[i] = g_pending[g_pendingCount - 1];
            --g_pendingCount;
        }
}

static void PendingRemoveAt(int i)
{
    g_pending[i] = g_pending[g_pendingCount - 1];
    --g_pendingCount;
}

// ---------------------------------------------------------------------------
//  계측 후크 — 전부 원본을 그대로 부르고 답도 그대로 돌려준다
// ---------------------------------------------------------------------------

// 게이트 생성자.
//   ??0GatewayBuilding@@QEAA@PEAVGameData@@AEBVVector3@Ogre@@AEBVQuaternion@3@PEAVFaction@@AEBVhand@@4@Z
//   인자: GameData*, 위치, 회전, Faction*, hand, hand — 위치는 생성 인자에서 바로 얻는다
//   (건물 포인터에 getPosition 을 부를 필요가 없어 한 겹 안전).
//   반환은 void* 로 받아 RAX 를 그대로 전달 (MSVC 생성자의 반환 관례 차이를 흡수).
typedef void* (*GateCtorFn)(void*, void*, const Ogre::Vector3&, const void*, void*, const hand&, const hand&);
static GateCtorFn origGateCtor = 0;

static void* hookGateCtor(void* self, void* gd, const Ogre::Vector3& pos,
                          const void* rot, void* faction, const hand& h1, const hand& h2)
{
    if (!origGateCtor) return 0;
    void* r = origGateCtor(self, gd, pos, rot, faction, h1, h2);
    float x, y, z; PosOf(&pos, &x, &y, &z);
    GateAdd(self, x, y, z);
    Log(LC_REG, "[등록] 게이트 %p  pos=(%.0f,%.0f,%.0f)  ctor인자faction=%p  (현재 %d개, 누적 %d)",
        self, x, y, z, faction, g_gateCount, g_gateTotalSeen);
    return r;
}

// 게이트 소멸자.  ??1GatewayBuilding@@UEAA@XZ
typedef void* (*GateDtorFn)(void*);
static GateDtorFn origGateDtor = 0;

static void* hookGateDtor(void* self)
{
    GateRemove(self);
    Log(LC_REG, "[해제] 게이트 %p  (현재 %d개)", self, g_gateCount);
    return origGateDtor ? origGateDtor(self) : 0;
}

// 문짝 생성자.
//   ??0DoorStuff@@QEAA@PEAVGameData@@AEBVVector3@Ogre@@AEBVQuaternion@3@PEAVFaction@@AEBVhand@@4PEAVLayout@@PEAVBuilding@@6@Z
//   게이트에 붙는 문짝의 부모가 게이트 건물 포인터로 오는지가 이 판의 핵심 질문.
typedef void* (*DoorCtorFn)(void*, void*, const Ogre::Vector3&, const void*, void*,
                            const hand&, const hand&, void*, Building*, Building*);
static DoorCtorFn origDoorCtor = 0;

static void* hookDoorCtor(void* self, void* gd, const Ogre::Vector3& pos,
                          const void* rot, void* faction, const hand& h1, const hand& h2,
                          void* layout, Building* b1, Building* b2)
{
    if (!origDoorCtor) return 0;
    void* r = origDoorCtor(self, gd, pos, rot, faction, h1, h2, layout, b1, b2);
    float x, y, z; PosOf(&pos, &x, &y, &z);
    DoorAdd(self, faction, layout, (void*)b1, (void*)b2, x, y, z);
    // 등록부에서 사라진 문짝(세이브 재로드로 교체된 옛 항목)을 가리키는 대기·감시 정리
    for (int k = g_pendingCount - 1; k >= 0; --k)
        if (!DoorByPtr(g_pending[k].doorPtr)) PendingRemoveAt(k);
    for (int k = g_watchCount - 1; k >= 0; --k)
        if (!DoorByPtr(g_watch[k].doorPtr)) WatchRemoveAt(k);
    Log(LC_REG, "[등록] 문짝 %p  pos=(%.0f,%.0f,%.0f)  faction=%p  layout=%p  부모1=%p 부모2=%p  (현재 %d개)",
        self, x, y, z, faction, layout, (void*)b1, (void*)b2, g_doorCount);
    return r;
}

// 문짝 소멸자.  ??1DoorStuff@@UEAA@XZ
typedef void* (*DoorDtorFn)(void*);
static DoorDtorFn origDoorDtor = 0;

static void* hookDoorDtor(void* self)
{
    DoorRemove(self);
    for (int i = g_pendingCount - 1; i >= 0; --i)
        if (g_pending[i].doorPtr == self) PendingRemoveAt(i);
    for (int i = g_watchCount - 1; i >= 0; --i)
        if (g_watch[i].doorPtr == self) WatchRemoveAt(i);
    return origDoorDtor ? origDoorDtor(self) : 0;
}

// 점→선분 수평거리 (x,z 평면. y 는 높이라 뺀다)
static float SegDist2D(float px, float pz, float ax, float az, float bx, float bz)
{
    float dx = bx - ax, dz = bz - az;
    float len2 = dx * dx + dz * dz;
    float t = 0.0f;
    if (len2 > 0.0001f)
    {
        t = ((px - ax) * dx + (pz - az) * dz) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    float cx = ax + t * dx, cz = az + t * dz;
    return (float)sqrt((double)((px - cx) * (px - cx) + (pz - cz) * (pz - cz)));
}

// 문짝 하나의 상태 조회 — v2-a에서 게이트 포인터 직접 캐스팅은 무효로 확정됐다.
// 이번엔 진짜 DoorStuff 포인터에 건다. 그래도 전부 __try, 실패는 -1.
static void ProbeDoor(void* p, int* hasLock, int* locked, void** fac,
                      float* openAmt, int* gateCode)
{
    *hasLock = -1; *locked = -1; *fac = (void*)-1; *openAmt = -1.0f; *gateCode = -1;
    __try { *hasLock = ((DoorStuff*)p)->_NV_hasDoorLock() ? 1 : 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { *locked = ((DoorStuff*)p)->isLocked() ? 1 : 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { *fac = (void*)((DoorStuff*)p)->_NV_getFaction(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { *openAmt = ((DoorStuff*)p)->getDoorOpenAmount(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    __try { *gateCode = (int)((DoorStuff*)p)->getGateCode(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
}

// ---------------------------------------------------------------------------
//  v2-b 감지·주입·재발행
// ---------------------------------------------------------------------------

// __try 격리용 (C++ 언와인딩과 SEH 를 한 함수에 못 섞는다)
static int SafeCheckOrder(void* ch, int task, void* subject)
{
    __try { return ((Character*)ch)->checkPlayerOrderForProblems((TaskType)task, (RootObject*)subject) ? 1 : 0; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
static bool SafeAddDoorOrder(void* ch, int task, void* subject)
{
    Ogre::Vector3 zero; zero.x = 0; zero.y = 0; zero.z = 0;
    __try
    {
        // 바닐라 수동 열기 실측: dest=문짝, subject=문짝, shift=0, clear=0.
        // clear=1 로 내면 명령은 발행되나 캐릭터가 실행하지 않는다 (거리 무관, A/B 확정).
        ((Character*)ch)->addOrder((Building*)subject, (TaskType)task, (RootObject*)subject, false, false, zero);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
static float SafeOpenAmount(void* door)
{
    __try { return ((DoorStuff*)door)->getDoorOpenAmount(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1.0f; }
}
static bool SafeCharPos3(void* ch, float* x, float* y, float* z)
{
    __try
    {
        Ogre::Vector3 p = ((RootObjectBase*)ch)->_NV_getPosition();
        *x = p.x; *y = p.y; *z = p.z;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool SafeCharPos(void* ch, float* x, float* z)
{
    __try
    {
        Ogre::Vector3 p = ((RootObjectBase*)ch)->_NV_getPosition();
        *x = p.x; *z = p.z;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// NULL 이동명령에서 닫힌 게이트를 찾아 대기표에 넣는다. 주입은 여기서 하지 않는다 —
// 클릭 1회가 이동명령 수십~수백 회(clear=1)로 들어와서, 즉시 주입하면 다음 반복이
// 지워버린다 (v2-b 실측: 캐릭터가 몸만 돌리고 정지). 폭풍이 끝난 걸 틱에서 보고 주입.
// 문 안팎 지점 조회 (엔진 제공). 실패 시 false.
static bool SafeDoorSides(void* door, float* ix, float* iz, float* ox, float* oz)
{
    __try
    {
        const Ogre::Vector3& in  = ((DoorStuff*)door)->getDoorPosInside();
        const Ogre::Vector3& out = ((DoorStuff*)door)->getDoorPosOutside();
        *ix = in.x;  *iz = in.z;
        *ox = out.x; *oz = out.z;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static float Dist2D(float ax, float az, float bx, float bz)
{
    float dx = bx - ax, dz = bz - az;
    return (float)sqrt((double)(dx * dx + dz * dz));
}

static void DetectAndQueue(void* ch, float cx, float cy, float cz,
                           float dx, float dy, float dz)
{
    if (!g_enabled) return;

    // ── 게이트 선정 (v3-b4) ─────────────────────────────────────────────
    // 기하로 미리 자르지 않는다. 직선 거리·측면 판정은 실제 경로탐색과 어긋나서
    // 진짜로 막힌 경우까지 탈락시킨 사례가 있었다 (v3-b3 실측: d경로 395 + 측면
    // "같은편"으로 제외 → 감지 0건 → 캐릭터 정지).
    // 대신 "이 문을 거쳐 가면 얼마나 더 걷는가"(추가거리)만 본다.
    // 오탐은 뒤의 정지·도착 판정이 막는다 (여러 판에서 실제로 걸러내는 것 확인).
    float direct = Dist2D(cx, cz, dx, dz);

    int   best = -1;
    float bestToDest = 0.0f, bestDetour = 0.0f, bestSeg = 0.0f;
    bool  bestCross = false;
    float nearestExtra = 1e9f;      // 진단용: 후보가 없을 때 가장 아까웠던 값

    for (int i = 0; i < g_gateCount; ++i)
    {
        DoorEntry* d = DoorOfGate(g_gates[i].ptr);
        if (!d) continue;

        float oa = SafeOpenAmount(d->ptr);
        if (oa < 0.0f) continue;                  // 조회 실패 — 죽은 항목일 수 있다
        if (oa >= g_openThreshold) continue;      // 이미 열린 문은 막는 게 아니다

        float detour = Dist2D(cx, cz, d->x, d->z) + Dist2D(d->x, d->z, dx, dz);
        float extra  = detour - direct;
        if (extra < nearestExtra) nearestExtra = extra;
        if (extra > g_maxDetour) continue;        // 너무 돌아간다 = 이 문과 무관

        float seg = SegDist2D(g_gates[i].x, g_gates[i].z, cx, cz, dx, dz);
        if (seg > g_pathDist) continue;           // 느슨한 안전 상한

        // 측면 판정은 이제 탈락 조건이 아니라 우선순위로만 쓴다
        bool cross = false;
        float ix, iz, ox, oz;
        if (SafeDoorSides(d->ptr, &ix, &iz, &ox, &oz))
        {
            bool charInside = Dist2D(cx, cz, ix, iz) < Dist2D(cx, cz, ox, oz);
            bool destInside = Dist2D(dx, dz, ix, iz) < Dist2D(dx, dz, ox, oz);
            cross = (charInside != destInside);
        }

        // 선정 기준은 "문에서 목적지까지의 거리". 추가거리는 후보를 거르는 상한으로만 쓴다.
        // (v3-c 실측: 캐릭터가 A 문 옆에 서 있으면 A 경유 추가거리가 거의 0 이라 무조건
        //  이겨버려, 목적지가 B 안쪽인데 A 를 열고 멈춰 섰다)
        float toDest = Dist2D(d->x, d->z, dx, dz);
        bool better = (best < 0)
                   || (toDest < bestToDest - 1.0f)
                   || (fabsf(toDest - bestToDest) <= 1.0f && cross && !bestCross)
                   || (fabsf(toDest - bestToDest) <= 1.0f && cross == bestCross && detour < bestDetour);
        if (better) { best = i; bestToDest = toDest; bestDetour = detour; bestSeg = seg; bestCross = cross; }
    }

    if (best < 0)
    {
        PendingRemoveChar(ch);   // 게이트와 무관한 새 이동 = 이전 요청 취소
        static char lastNone[128] = { 0 };
        char key[128];
        sprintf_s(key, sizeof(key), "%.0f,%.0f>%.0f,%.0f", cx, cz, dx, dz);
        if (strcmp(key, lastNone) != 0)
        {
            strcpy_s(lastNone, sizeof(lastNone), key);
            Log(LC_ACT, "[감지] char=%p 후보 없음 — 닫힌 게이트 중 최소 추가거리=%.0f (한도 %.0f)",
                ch, (nearestExtra > 1e8f) ? -1.0f : nearestExtra, g_maxDetour);
        }
        return;
    }

    DoorEntry* door = DoorOfGate(g_gates[best].ptr);
    if (!door) return;
    float openAmt = SafeOpenAmount(door->ptr);

    bool wasLocked = false;
    __try { wasLocked = ((DoorStuff*)door->ptr)->isLocked(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    PendingAdd(ch, door->ptr, dx, dy, dz, wasLocked);
    {   // 감지 시점의 편을 기록해둔다 — 반대편으로 넘어가면 통과 완료다
        float ix2, iz2, ox2, oz2;
        int sideNow = -1;
        if (SafeDoorSides(door->ptr, &ix2, &iz2, &ox2, &oz2))
            sideNow = (Dist2D(cx, cz, ix2, iz2) < Dist2D(cx, cz, ox2, oz2)) ? 1 : 0;
        for (int k = 0; k < g_pendingCount; ++k)
            if (g_pending[k].charPtr == ch) { g_pending[k].sideAtDetect = sideNow; break; }
    }
    Log(LC_ACT, "[감지] char=%p @(%.0f,%.0f) 목적지=(%.0f,%.0f) 문까지=%.0f 문→목적지=%.0f | 문짝=%p 열림량=%.2f 잠김=%d 추가거리=%.0f 측면교차=%d d경로=%.0f — 대기 등록",
        ch, cx, cz, dx, dz, Dist2D(cx, cz, door->x, door->z), bestToDest,
        door->ptr, openAmt, wasLocked ? 1 : 0,
        bestDetour - direct, bestCross ? 1 : 0, bestSeg);
}

// update4Frame 틱의 대기표 처리(ProcessPending)는 origMoveOrder 선언 뒤에 정의한다.

// NULL 이동명령 때 후보 덤프 (검증 ⓒ). 같은 (출발,목적지) 는 한 번만.
static void DumpGateCandidates(float cx, float cy, float cz, float dx, float dy, float dz)
{
    static char lastKey[128] = { 0 };
    char key[128];
    sprintf_s(key, sizeof(key), "%.0f,%.0f>%.0f,%.0f", cx, cz, dx, dz);
    if (strcmp(key, lastKey) == 0) return;
    strcpy_s(lastKey, sizeof(lastKey), key);

    Log(LC_GATE, "[후보] NULL이동 (%.0f,%.0f,%.0f)→(%.0f,%.0f,%.0f)  등록: 게이트 %d개, 문짝 %d개",
        cx, cy, cz, dx, dy, dz, g_gateCount, g_doorCount);
    for (int i = 0; i < g_gateCount; ++i)
    {
        float gx = g_gates[i].x, gy = g_gates[i].y, gz = g_gates[i].z;
        float dChar = (float)sqrt((double)((gx-cx)*(gx-cx) + (gz-cz)*(gz-cz)));
        float dPath = SegDist2D(gx, gz, cx, cz, dx, dz);
        if (dPath > 5000.0f && dChar > 5000.0f) continue;   // 먼 지역 게이트 소음 컷
        Log(LC_GATE, "[후보]   게이트 %p pos=(%.0f,%.0f,%.0f)  d캐릭=%.0f d경로=%.0f",
            g_gates[i].ptr, gx, gy, gz, dChar, dPath);

        // 측면 판정 실측 — 문 안팎 지점이 게이트에서 의미 있는 값인지 여기서 갈린다.
        {
            DoorEntry* sd = DoorOfGate(g_gates[i].ptr);
            float ix, iz, ox, oz;
            if (sd && SafeDoorSides(sd->ptr, &ix, &iz, &ox, &oz))
            {
                float ci = Dist2D(cx, cz, ix, iz), co = Dist2D(cx, cz, ox, oz);
                float di = Dist2D(dx, dz, ix, iz), doo = Dist2D(dx, dz, ox, oz);
                Log(LC_GATE, "[후보]     측면 안=(%.0f,%.0f) 밖=(%.0f,%.0f) 안밖간격=%.0f | 캐릭 안%.0f/밖%.0f=%s  목적 안%.0f/밖%.0f=%s → %s",
                    ix, iz, ox, oz, Dist2D(ix, iz, ox, oz),
                    ci, co, (ci < co) ? "안" : "밖",
                    di, doo, (di < doo) ? "안" : "밖",
                    ((ci < co) != (di < doo)) ? "교차(후보)" : "같은편(제외)");
            }
            else
                Log(LC_GATE, "[후보]     측면 조회 실패 — 이 게이트는 측면 판정 불가");
        }

        // 1) 부모 일치 문짝 — 이게 잡히는지가 이 판의 핵심
        int matched = 0;
        for (int j = 0; j < g_doorCount; ++j)
        {
            if (g_doors[j].b1 != g_gates[i].ptr && g_doors[j].b2 != g_gates[i].ptr)
                continue;
            ++matched;
            int hasLock, locked, gateCode; void* fac; float openAmt;
            ProbeDoor(g_doors[j].ptr, &hasLock, &locked, &fac, &openAmt, &gateCode);
            Log(LC_GATE, "[후보]     문짝(부모일치) %p  잠금장치=%d 잠김=%d faction=%p 열림량=%.2f 코드=%d  (ctor때 faction=%p)",
                g_doors[j].ptr, hasLock, locked, fac, openAmt, gateCode, g_doors[j].fac);
        }
        // 2) 부모 일치가 없으면 최근접 문짝을 진단용으로 — 대응 방법을 다시 찾는 재료
        if (matched == 0)
        {
            int best = -1; float bestD = 1e9f;
            for (int j = 0; j < g_doorCount; ++j)
            {
                float ddx = g_doors[j].x - gx, ddz = g_doors[j].z - gz;
                float d = (float)sqrt((double)(ddx*ddx + ddz*ddz));
                if (d < bestD) { bestD = d; best = j; }
            }
            if (best >= 0)
            {
                int hasLock, locked, gateCode; void* fac; float openAmt;
                ProbeDoor(g_doors[best].ptr, &hasLock, &locked, &fac, &openAmt, &gateCode);
                Log(LC_GATE, "[후보]     부모일치 없음. 최근접 문짝 %p d=%.0f  부모1=%p 부모2=%p  잠금장치=%d 잠김=%d faction=%p 열림량=%.2f 코드=%d",
                    g_doors[best].ptr, bestD, g_doors[best].b1, g_doors[best].b2,
                    hasLock, locked, fac, openAmt, gateCode);
            }
            else
                Log(LC_GATE, "[후보]     부모일치 없음. 등록된 문짝도 없음");
        }
    }
}

// 이동 명령: 로그의 구분선. 이 줄 이후가 한 번의 조작이다.
//   ?_NV_playerMoveOrderDefault@Character@@QEAAXPEAVBuilding@@PEAVRootObject@@AEBVVector3@Ogre@@@Z
typedef void (*MoveOrderFn)(void*, Building*, RootObject*, const Ogre::Vector3&);
static MoveOrderFn origMoveOrder = 0;
static int g_moveNo = 0;

static void hookMoveOrder(void* self, Building* b, RootObject* root,
                          const Ogre::Vector3& pos)
{
    if (!origMoveOrder) return;
    float x, y, z; PosOf(&pos, &x, &y, &z);
    void* vtB = 0; void* vtR = 0;
    __try
    {
        if (b)    vtB = *(void**)b;
        if (root) vtR = *(void**)root;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
    float cx = 0, cy = 0, cz = 0;
    __try
    {
        Ogre::Vector3 cp = ((RootObjectBase*)self)->_NV_getPosition();
        cx = cp.x; cy = cp.y; cz = cp.z;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }

    // v1 결함 수정: 번호가 줄마다 달라 중복 접기가 죽어 있었다.
    // 번호 없는 본문으로 접고, 번호는 카운터로만 유지한다.
    ++g_moveNo;
    Log(LC_MOVE, "───── 이동명령  char=%p @(%.1f,%.1f,%.1f)  building=%p(vt=%p)  root=%p(vt=%p)  목적지=(%.1f,%.1f,%.1f)  직선거리=%.0f",
        self, cx, cy, cz, (void*)b, vtB, (void*)root, vtR, x, y, z,
        (float)sqrt((double)((x-cx)*(x-cx) + (y-cy)*(y-cy) + (z-cz)*(z-cz))));

    if (!b && !root)
    {
        origMoveOrder(self, b, root, pos);         // 엔진 처리 먼저 (관찰대로 버려지는 것 포함)
        // 주입 후에 새 이동명령이 들어오면 우리 열기 명령이 덮일 수 있다 — 그 흔적을 남긴다
        for (int k = 0; k < g_pendingCount; ++k)
            if (g_pending[k].charPtr == self && g_pending[k].injected && g_pending[k].injectT != 0)
            {
                Log(LC_ACT, "[추적] char=%p 주입 %.1f초 뒤 새 이동명령 도착 — 열기 명령이 덮일 수 있다",
                    self, (GetTickCount64() - g_pending[k].injectT) / 1000.0f);
                break;
            }
        DumpGateCandidates(cx, cy, cz, x, y, z);   // 진단 로그
        DetectAndQueue(self, cx, cy, cz, x, y, z); // 그 다음 해제 명령을 얹는다
        return;
    }
    origMoveOrder(self, b, root, pos);
}

// ---------------------------------------------------------------------------
//  update4Frame 틱 — 대기표 감시 (SleepFix 와 같은 후크, 250ms 스로틀)
//    ?_NV_update4Frame@AI@@QEAAXM@Z
// ---------------------------------------------------------------------------
static void CheckConfigReload();   // 정의는 설정 파일 절

static void ProcessPending()
{
    if (g_pendingCount == 0) return;
    unsigned __int64 now = GetTickCount64();
    unsigned __int64 waitMs  = (unsigned __int64)(g_pendingWait * 1000.0f);
    unsigned __int64 delayMs = (unsigned __int64)(g_injectDelay * 1000.0f);

    for (int i = g_pendingCount - 1; i >= 0; --i)
    {
        // 수명 판정: "등록 후 60초"가 아니라 "제자리에 멈춘 채 60초".
        // (원거리 복귀는 게이트까지 걷는 데만 몇 분이 걸려 등록 기준으로는 만료됐다)
        float lx, lz;
        if (SafeCharPos(g_pending[i].charPtr, &lx, &lz))
        {
            float mx = lx - g_pending[i].lastX, mz = lz - g_pending[i].lastZ;
            if (!g_pending[i].hasLast || (mx * mx + mz * mz) > (g_moveEps * g_moveEps))
            {
                g_pending[i].lastX = lx; g_pending[i].lastZ = lz;
                g_pending[i].hasLast = true;
                g_pending[i].stillT = now;      // 움직이는 중 — 시계 리셋
            }
        }
        if (now - g_pending[i].stillT > waitMs)
        {
            Log(LC_ACT, "[재발행] char=%p 정지 %.0f초 — 소거", g_pending[i].charPtr, g_pendingWait);
            PendingRemoveAt(i);
            continue;
        }
        if (now - g_pending[i].t > 600000)      // 절대 상한 10분 (스테일 방지)
        {
            Log(LC_ACT, "[재발행] char=%p 절대 상한 10분 — 소거", g_pending[i].charPtr);
            PendingRemoveAt(i);
            continue;
        }

        float openAmt = SafeOpenAmount(g_pending[i].doorPtr);

        // 1상: 클릭 폭풍이 끝났고, 캐릭터가 실제로 멈춰 있으면 열기 명령 주입
        if (!g_pending[i].injected)
        {
            // 폭풍 종료 대기. 단 등록 후 3초가 지나면 폭풍이 계속돼도 진행한다
            // (굶음 절대 상한 — 8인 테스트에서 반복 지속으로 일부가 영영 대기했다)
            if (now - g_pending[i].lastSeen < delayMs
                && now - g_pending[i].t < 3000) continue;

            // 정지 판정 — "막힌 캐릭터는 제자리"(인게임 실측)를 그대로 쓴다.
            // 250ms 간격 두 표본 사이에 이동이 있으면 경로가 있었던 것 (게이트를
            // 스치기만 하는 내부 이동 오탐 + 사용자 취소를 여기서 걸러낸다).
            float px, pz;
            if (!SafeCharPos(g_pending[i].charPtr, &px, &pz))
            {
                Log(LC_ACT, "[주입] char=%p 위치 조회 실패 — 소거", g_pending[i].charPtr);
                PendingRemoveAt(i);
                continue;
            }
            if (!g_pending[i].sampled)
            {
                g_pending[i].sampled = true;
                g_pending[i].sx = px; g_pending[i].sz = pz;
                continue;   // 다음 틱에 비교
            }
            float mdx = px - g_pending[i].sx, mdz = pz - g_pending[i].sz;
            float moved = (float)sqrt((double)(mdx * mdx + mdz * mdz));
            if (moved > g_moveEps)
            {
                Log(LC_ACT, "[주입] char=%p 이동 중(%.1f>%.1f) — 막힌 게 아니다, 취소",
                    g_pending[i].charPtr, moved, g_moveEps);
                PendingRemoveAt(i);
                continue;
            }

            // 도착 판정 — 정지해 있어도 목적지 곁이면 "다 온 것"이지 막힌 게 아니다.
            // (v3-a 실측: 내부 짧은 이동이 도착 후 정지→오주입. 막힌 캐릭터는
            //  목적지가 게이트 너머라 이 반경에 못 들어온다)
            float adx = px - g_pending[i].dx, adz = pz - g_pending[i].dz;
            float toDest = (float)sqrt((double)(adx * adx + adz * adz));
            if (toDest < g_arriveEps)
            {
                Log(LC_ACT, "[주입] char=%p 목적지 도착(잔여 %.1f<%.1f) — 막힌 게 아니다, 취소",
                    g_pending[i].charPtr, toDest, g_arriveEps);
                PendingRemoveAt(i);
                continue;
            }

            if (openAmt >= g_openThreshold)
            {
                g_pending[i].injected = true;   // 멈춰 있는데 이미 열림 (수동 등) — 재발행 단계로
                continue;
            }
            // 전원 주입 — 바닐라 수동 열기 실측(2026-08-19): 선택된 캐릭터 전원에게
            // 72 가 나간다. "문짝당 1명"은 우리가 만든 제약이었고, 그 탓에 1명이 왕복하는
            // 동안 나머지가 제자리에 서 있어 원거리에서 이동 시간이 두 배가 됐다.
            int chk = SafeCheckOrder(g_pending[i].charPtr, TT_OPEN_DOOR, g_pending[i].doorPtr);
            Log(LC_ACT, "[주입] char=%p 문짝=%p task=%d 열림량=%.2f  체크=%d(기대 %d)",
                g_pending[i].charPtr, g_pending[i].doorPtr, TT_OPEN_DOOR, openAmt, chk, 0);
            if (chk != 0)
            {
                Log(LC_ACT, "[주입] 체크 불일치 — 명령 안 냄 (아군 게이트인데 이 줄이 나오면 cfg 의 checkOK 를 뒤집어라)");
                g_pending[i].injected = true;   // 재시도 안 함. 수동 개방 시 재발행은 살아 있다
                continue;
            }
            bool ok = SafeAddDoorOrder(g_pending[i].charPtr, TT_OPEN_DOOR, g_pending[i].doorPtr);
            // 주입 후 추적 기준점 — 이 캐릭터가 명령을 실제로 실행하는지 본다
            DoorEntry* dj = DoorByPtr(g_pending[i].doorPtr);
            g_pending[i].injectT = now;
            g_pending[i].injX = px; g_pending[i].injZ = pz;
            g_pending[i].injDoorDist = dj ? Dist2D(px, pz, dj->x, dj->z) : -1.0f;
            g_pending[i].moveLogged = false;
            g_pending[i].stallLogged = false;
            Log(LC_ACT, "[주입] 명령(%d) %s  char=%p @(%.0f,%.0f) 문까지=%.0f  subject=%p",
                TT_OPEN_DOOR, ok ? "발행" : "실패(가드)", g_pending[i].charPtr, px, pz,
                g_pending[i].injDoorDist, g_pending[i].doorPtr);
            g_pending[i].injected = true;
            continue;
        }

        // 3상: 재발행까지 끝난 뒤 — 실제로 문을 지날 때까지 남아 자동 닫기를 막는다
        if (g_pending[i].reissued)
        {
            float px3, pz3;
            if (!SafeCharPos(g_pending[i].charPtr, &px3, &pz3)) { PendingRemoveAt(i); continue; }

            // 목적지 도착 = 통과 완료
            float adx = px3 - g_pending[i].dx, adz = pz3 - g_pending[i].dz;
            if ((float)sqrt((double)(adx * adx + adz * adz)) < g_arriveEps)
            {
                Log(LC_ACT, "[통과] char=%p 목적지 도착 — 대기 해제 (남은 %d)",
                    g_pending[i].charPtr, g_pendingCount - 1);
                PendingRemoveAt(i);
                continue;
            }
            // 문의 반대편으로 넘어갔으면 통과 완료
            if (g_pending[i].sideAtDetect >= 0)
            {
                float ix3, iz3, ox3, oz3;
                if (SafeDoorSides(g_pending[i].doorPtr, &ix3, &iz3, &ox3, &oz3))
                {
                    int sideNow = (Dist2D(px3, pz3, ix3, iz3) < Dist2D(px3, pz3, ox3, oz3)) ? 1 : 0;
                    if (sideNow != g_pending[i].sideAtDetect)
                    {
                        Log(LC_ACT, "[통과] char=%p 문 반대편으로 넘어감 — 대기 해제 (남은 %d)",
                            g_pending[i].charPtr, g_pendingCount - 1);
                        PendingRemoveAt(i);
                        continue;
                    }
                }
            }
            continue;   // 아직 지나는 중 — 이 항목이 남아 있는 동안 문은 안 닫힌다
        }

        // 2상: 주입 후 실행 추적 + 열림 감시 → 재발행
        if (g_pending[i].injectT != 0)
        {
            float px2, pz2;
            if (SafeCharPos(g_pending[i].charPtr, &px2, &pz2))
            {
                float mdx = px2 - g_pending[i].injX, mdz = pz2 - g_pending[i].injZ;
                float moved = (float)sqrt((double)(mdx * mdx + mdz * mdz));
                if (!g_pending[i].moveLogged && moved > g_moveEps)
                {
                    DoorEntry* dj = DoorByPtr(g_pending[i].doorPtr);
                    Log(LC_ACT, "[추적] char=%p 주입 %.1f초 뒤 이동 시작 (%.0f 이동, 문까지 %.0f→%.0f)",
                        g_pending[i].charPtr, (now - g_pending[i].injectT) / 1000.0f, moved,
                        g_pending[i].injDoorDist, dj ? Dist2D(px2, pz2, dj->x, dj->z) : -1.0f);
                    g_pending[i].moveLogged = true;
                }
                else if (!g_pending[i].moveLogged && !g_pending[i].stallLogged
                         && now - g_pending[i].injectT > 5000)
                {
                    Log(LC_ACT, "[추적] char=%p 주입 5초간 제자리 — 명령 미실행 의심 (문까지 %.0f, 열림량 %.2f)",
                        g_pending[i].charPtr, g_pending[i].injDoorDist, openAmt);
                    g_pending[i].stallLogged = true;
                }
            }
        }

        if (openAmt < g_openThreshold) continue;   // 아직 닫힘 (조회 실패 -1 도 대기 유지)

        Ogre::Vector3 dst;
        dst.x = g_pending[i].dx; dst.y = g_pending[i].dy; dst.z = g_pending[i].dz;
        bool ok = false;
        __try { if (origMoveOrder) { origMoveOrder(g_pending[i].charPtr, 0, 0, dst); ok = true; } }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
        Log(LC_ACT, "[재발행] char=%p → (%.0f,%.0f,%.0f)  열림량=%.2f  %s",
            g_pending[i].charPtr, dst.x, dst.y, dst.z, openAmt, ok ? "성공" : "실패(가드)");
        WatchAdd(g_pending[i].doorPtr, g_pending[i].wasLocked);   // 연 문 감시 시작
        // 여기서 지우지 않는다. 통과할 때까지 남아 자동 닫기를 막는다 (3상으로).
        g_pending[i].reissued = true;
    }
}

typedef void (*Update4Fn)(void*, float);
static Update4Fn origUpdate4 = 0;

// ---------------------------------------------------------------------------
//  v3-a 감시 처리 — 1초마다 근처 캐릭터 열거, QPC 로 비용 실측. 닫지 않는다.
//  lektor 는 CorpseLoot 방식: 버퍼를 우리가 미리 잡아 넘긴다 (빈 lektor 를 넘기면
//  게임이 할당하고 소멸자 익스포트가 없어 누수).
// ---------------------------------------------------------------------------
static int SafeGetArea(Faction* fac, lektor<RootObject*>* lek, const Ogre::Vector3* center, float radius)
{
    __try
    {
        fac->_NV_getCharactersInArea(*lek, *center, radius, g_areaFlag);
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// v3-b 문 직접 조작. 명령이 아니라 문짝에 바로 건다 — 닫아줄 사람이 필요 없다.
// "잠금 명령이 닫기+잠금을 자동 수행"(인게임 확정)이라 lockDoor 한 번이면 닫힘+잠김.
// 원래 안 잠긴 문이었다면 unlockDoor 로 되돌린다 (해제는 열지 않는다 — 실측).
// 닫기 시도. v3-b5 실측: lockDoor() 만으로는 안 닫힌다 (2초 뒤 열림량 1.00).
// 잠금은 "닫힌 뒤에 잠근다"는 뜻이고 닫는 동작은 closeDoor() 가 맡는 것으로 보인다.
// closeDoor 의 bool 반환과 문 상태를 함께 찍어 어디서 막히는지 남긴다.
// 닫기 시도 — 어느 함수가 실제로 닫는지는 미검증이라 한 단계씩 시험하고 로그로 가른다.
//   1단계 closeDoor()          2단계 setDoorState(CLOSING)     3단계 _forceDoorClosedUT()
// 셋 다 익스포트·헤더 실물 확인. 이름에서 동작을 짐작하지 않는다 — 2초 뒤 열림량이 판정한다.
// (v3-b5 실측: lockDoor() 단독은 닫지 못한다. 2초 뒤 열림량 1.00)
static int SafeTryClose(void* door, int step, int* rc, int* stateAfter)
{
    *rc = -1; *stateAfter = -1;
    __try
    {
        if      (step == 1) *rc = ((DoorStuff*)door)->closeDoor() ? 1 : 0;
        else if (step == 2) { ((DoorStuff*)door)->setDoorState(DOORSTATE_CLOSING); *rc = 2; }
        else                *rc = ((DoorStuff*)door)->_forceDoorClosedUT() ? 1 : 0;
        *stateAfter = (int)((DoorStuff*)door)->getDoorState();
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

// 잠금 상태 복원은 닫힌 것이 확인된 뒤에 따로 건다
static int SafeRestoreLock(void* door, bool wasLocked)
{
    __try
    {
        if (wasLocked) ((DoorStuff*)door)->lockDoor();
        else           ((DoorStuff*)door)->unlockDoor();
        return 1;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static int SafeDoorState(void* door)
{
    __try { return (int)((DoorStuff*)door)->getDoorState(); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
}

static const char* StateName(int st)
{
    switch (st) { case 0: return "닫힘"; case 1: return "열림";
                  case 2: return "열리는중"; case 3: return "닫히는중"; default: return "?"; }
}

// 이 문짝을 기다리는 대기자가 있는가 (통과 예정자가 남아 있으면 닫지 않는다)
static bool PendingWaitsForDoor(void* door)
{
    for (int i = 0; i < g_pendingCount; ++i)
        if (g_pending[i].doorPtr == door) return true;
    return false;
}

static void ProcessWatch()
{
    if (g_watchCount == 0) return;
    unsigned __int64 now = GetTickCount64();

    static RootObject* s_buf[256];
    static LARGE_INTEGER s_freq = { 0 };
    if (s_freq.QuadPart == 0) QueryPerformanceFrequency(&s_freq);

    for (int i = g_watchCount - 1; i >= 0; --i)
    {
        DoorEntry* d = DoorByPtr(g_watch[i].doorPtr);
        if (!d) { WatchRemoveAt(i); continue; }

        float openAmt = SafeOpenAmount(d->ptr);

        // 닫기를 실행한 뒤 2초 지나서 결과를 찍는다 — 호출 직후엔 애니메이션 전이라
        // 항상 1.00 이 나와 판독이 불가능했다 (v3-b 오독의 원인).
        if (g_watch[i].closeT != 0)
        {
            if (now - g_watch[i].closeT < 2000) continue;
            bool closed = (openAmt >= 0.0f && openAmt < g_openThreshold);
            Log(LC_ACT, "[닫기] %d단계 결과: 문짝=%p 열림량=%.2f 상태=%s → %s",
                g_watch[i].closeStep, d->ptr, openAmt, StateName(SafeDoorState(d->ptr)),
                closed ? "닫혔다" : "안 닫혔다");
            if (closed)
            {
                int rl = SafeRestoreLock(d->ptr, g_watch[i].wasLocked);
                Log(LC_ACT, "[닫기] 잠금 복원 %s (원래잠김=%d) — 완료",
                    rl ? "실행" : "실패(가드)", g_watch[i].wasLocked ? 1 : 0);
                WatchRemoveAt(i);
                continue;
            }
            if (g_watch[i].closeStep >= 3)
            {
                Log(LC_ACT, "[닫기] 문짝=%p 세 방법 모두 실패 — 이 문은 포기한다", d->ptr);
                WatchRemoveAt(i);
                continue;
            }
            g_watch[i].closeT = 0;      // 다음 단계로
            g_watch[i].emptySince = now - (unsigned __int64)(g_closeDelay * 1000.0f);
            continue;
        }

        // 이미 닫혀 있으면 (사용자가 수동으로 닫았거나) 감시 끝
        if (openAmt >= 0.0f && openAmt < g_openThreshold)
        {
            Log(LC_ACT, "[닫기] 문짝=%p 이미 닫힘 — 감시 종료", d->ptr);
            WatchRemoveAt(i);
            continue;
        }

        // 마지막 통과 시작 후 유예 — 문을 향해 걸어오는 사람이 반경 밖이어도 기다린다
        if (now - g_watch[i].lastPassT < (unsigned __int64)(g_closeGrace * 1000.0f))
        {
            g_watch[i].emptySince = 0;
            continue;
        }

        if (now - g_watch[i].startT > (unsigned __int64)(g_closeGiveUp * 1000.0f))
        {
            Log(LC_ACT, "[닫기] 문짝=%p %.0f초 동안 못 닫음 — 포기 (바닐라처럼 열린 채 둔다)",
                d->ptr, g_closeGiveUp);
            WatchRemoveAt(i);
            continue;
        }

        if (now - g_watch[i].lastLogT < 1000) continue;   // 1초 간격 판정
        g_watch[i].lastLogT = now;

        // 아직 이 문을 기다리는 통과 예정자가 있으면 닫지 않는다
        if (PendingWaitsForDoor(g_watch[i].doorPtr))
        {
            g_watch[i].emptySince = 0;
            continue;
        }

        Faction* fac = 0;
        __try { fac = ((DoorStuff*)d->ptr)->_NV_getFaction(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
        if (!fac) { Log(LC_ACT, "[닫기] 문짝=%p 팩션 조회 실패 — 보류", d->ptr); continue; }

        lektor<RootObject*> lek;
        memset(&lek, 0, sizeof(lek));
        lek.stuff = s_buf; lek.maxSize = 256; lek.count = 0;
        Ogre::Vector3 center; center.x = d->x; center.y = d->y; center.z = d->z;

        LARGE_INTEGER t0, t1;
        QueryPerformanceCounter(&t0);
        int callOk = SafeGetArea(fac, &lek, &center, g_closeRadius);
        QueryPerformanceCounter(&t1);
        double usec = (double)(t1.QuadPart - t0.QuadPart) * 1000000.0 / (double)s_freq.QuadPart;

        if (!callOk)
        {
            Log(LC_ACT, "[닫기] 문짝=%p 근처 조회 크래시(가드) — 감시 중단", d->ptr);
            WatchRemoveAt(i);
            continue;
        }
        if (lek.stuff != s_buf)
        {
            Log(LC_ACT, "[닫기] 문짝=%p 경고: lektor 재할당(count=%u) — 이번 판정 건너뜀", d->ptr, lek.count);
            continue;
        }

        unsigned int n = (lek.count <= 256u) ? lek.count : 0;
        if (n > 0)
        {
            if (g_watch[i].emptySince != 0)
                Log(LC_ACT, "[닫기] 문짝=%p 근처 %u명 — 대기 리셋 (소요=%.0fus)", d->ptr, n, usec);
            g_watch[i].emptySince = 0;
            continue;
        }

        if (g_watch[i].emptySince == 0)
        {
            g_watch[i].emptySince = now;
            Log(LC_ACT, "[닫기] 문짝=%p 반경 %.0f 비었다 — %.0f초 뒤 닫는다 (소요=%.0fus)",
                d->ptr, g_closeRadius, g_closeDelay, usec);
            continue;
        }
        if (now - g_watch[i].emptySince < (unsigned __int64)(g_closeDelay * 1000.0f)) continue;

        if (!g_autoClose)
        {
            Log(LC_ACT, "[닫기] 문짝=%p 조건 충족이나 autoClose=0 — 닫지 않음", d->ptr);
            WatchRemoveAt(i);
            continue;
        }

        int step = g_watch[i].closeStep + 1;
        const char* stepName = (step == 1) ? "closeDoor()"
                             : (step == 2) ? "setDoorState(CLOSING)" : "_forceDoorClosedUT()";
        int rc = -1, stAfter = -1;
        int ok = SafeTryClose(d->ptr, step, &rc, &stAfter);
        Log(LC_ACT, "[닫기] %d단계 %s 문짝=%p %s  반환=%d 직후상태=%s (2초 뒤 결과 확인)",
            step, stepName, d->ptr, ok ? "실행" : "실패(가드)", rc, StateName(stAfter));
        g_watch[i].closeStep = step;
        g_watch[i].closeT = now;
    }
}

static void hookUpdate4(void* self, float t)
{
    if (origUpdate4) origUpdate4(self, t);
    static unsigned __int64 lastTick = 0;
    unsigned __int64 now = GetTickCount64();
    if (now - lastTick < 250) return;
    lastTick = now;
    CheckConfigReload();
    ProcessPending();
    ProcessWatch();
}

// ---------------------------------------------------------------------------
//  addOrder 계측 — 수동 해제 1회로 140 의 subject 정체(게이트/문짝)가 확정된다.
//  우리가 주입한 140 도 여길 지나며 찍힌다 (자기 검증).
//  전 명령을 다 찍으면 홍수라 문 관련(29·72·73·77·140) + 등록부 일치만 찍는다.
// ---------------------------------------------------------------------------
static const char* TagPtr(void* p)
{
    if (!p) return "NULL";
    for (int i = 0; i < g_gateCount; ++i) if (g_gates[i].ptr == p) return "게이트!";
    for (int i = 0; i < g_doorCount; ++i) if (g_doors[i].ptr == p) return "문짝!";
    return "?";
}

typedef void (*AddOrderFn)(void*, Building*, TaskType, RootObject*, bool, bool, const Ogre::Vector3&);
static AddOrderFn origAddOrder = 0;

static void hookAddOrder(void* self, Building* dest, TaskType task, RootObject* subject,
                         bool f1, bool f2, const Ogre::Vector3& pos)
{
    if (!origAddOrder) return;
    int t = (int)task;
    bool doorTask = (t == 29 || t == 72 || t == 73 || t == 77 || t == 140);
    bool regHit = false;
    if (!doorTask)
    {
        void* d = (void*)dest; void* s = (void*)subject;
        for (int i = 0; i < g_gateCount && !regHit; ++i)
            if (g_gates[i].ptr == d || g_gates[i].ptr == s) regHit = true;
        for (int i = 0; i < g_doorCount && !regHit; ++i)
            if (g_doors[i].ptr == d || g_doors[i].ptr == s) regHit = true;
    }
    if (doorTask || regHit)
    {
        float x, y, z; PosOf(&pos, &x, &y, &z);
        Log(LC_ORDER, "[명령] char=%p task=%d  dest=%p(%s)  subject=%p(%s)  f1=%d f2=%d  pos=(%.0f,%.0f,%.0f)",
            self, t, (void*)dest, TagPtr((void*)dest), (void*)subject, TagPtr((void*)subject),
            f1 ? 1 : 0, f2 ? 1 : 0, x, y, z);
    }
    origAddOrder(self, dest, task, subject, f1, f2, pos);
}

// 문 열기 후보 점수 (v1 실측: 세 케이스 모두 호출 0회. 관찰용으로 유지)
typedef float (*ScoreFn)(void*, const hand&, const Ogre::Vector3&);
static ScoreFn origScoreUnlock = 0;

static float hookScoreUnlock(void* self, const hand& h, const Ogre::Vector3& pos)
{
    if (!origScoreUnlock) return 0.0f;
    float r = origScoreUnlock(self, h, pos);
    void* obj; void* vt; HandInfo(h, &obj, &vt);
    float x, y, z; PosOf(&pos, &x, &y, &z);
    Log(LC_SCORE, "[점수] scoreUnlockDoorHere  ai=%p  대상=%p(vt=%p)  점수=%.2f  pos=(%.1f,%.1f,%.1f)",
        self, obj, vt, r, x, y, z);
    return r;
}

// 목적지 게이트 탐색 (v1 실측: 호출 0회. 관찰용으로 유지)
typedef float (*DestGateFn)(void*, const hand&, hand&, bool);
static DestGateFn origDestGate = 0;

static float hookDestGate(void* self, const hand& subject, hand& out, bool justAsking)
{
    if (!origDestGate) return 0.0f;
    float r = origDestGate(self, subject, out, justAsking);
    void* so; void* sv; HandInfo(subject, &so, &sv);
    void* oo; void* ov; HandInfo(out, &oo, &ov);
    Log(LC_GATE, "[게이트] getDestinationGate  ai=%p  subject=%p(vt=%p) -> out=%p(vt=%p)  점수=%.2f  물어만봄=%d",
        self, so, sv, oo, ov, r, justAsking ? 1 : 0);
    return r;
}

// 잠금·안팎·열림 판정. verbose 일 때만 전부 찍는다 (호출이 매우 잦다).
typedef bool (*JudgeFn)(void*, const hand&, const Ogre::Vector3&);
static JudgeFn origIsLocked = 0, origInside = 0, origOutside = 0, origIsOpen = 0, origIntruder = 0;

static void CheckConfigReload();   // 정의는 설정 파일 절

static bool judgeCommon(const char* name, JudgeFn orig, void* self,
                        const hand& h, const Ogre::Vector3& pos)
{
    if (!orig) return false;
    bool r = orig(self, h, pos);
    // 이 플러그인엔 프레임 훅이 없다. 판정 함수가 자주 불리므로 여기에 얹는다
    // (자체 스로틀 3초라 매 호출 비용은 u64 비교 하나).
    CheckConfigReload();
    if (g_verbose || r)
    {
        void* obj; void* vt; HandInfo(h, &obj, &vt);
        Log(LC_JUDGE, "[판정] %-24s ai=%p  대상=%p(vt=%p)  답=%d",
            name, self, obj, vt, r ? 1 : 0);
    }
    return r;
}
static bool hookIsLocked (void* s, const hand& h, const Ogre::Vector3& p) { return judgeCommon("isDoorLocked",            origIsLocked, s, h, p); }
static bool hookInside   (void* s, const hand& h, const Ogre::Vector3& p) { return judgeCommon("isDoorLockedAndMeInside", origInside,   s, h, p); }
static bool hookOutside  (void* s, const hand& h, const Ogre::Vector3& p) { return judgeCommon("isDoorLockedAndMeOutside",origOutside,  s, h, p); }
static bool hookIsOpen   (void* s, const hand& h, const Ogre::Vector3& p) { return judgeCommon("isDoorOpen",              origIsOpen,   s, h, p); }
static bool hookIntruder (void* s, const hand& h, const Ogre::Vector3& p) { return judgeCommon("intruderIsOutsideGates",  origIntruder, s, h, p); }

// ---------------------------------------------------------------------------
//  설정 파일
// ---------------------------------------------------------------------------
static void LoadConfig()
{
    FILE* f = NULL;
    if (fopen_s(&f, "GateFix.cfg", "r") != 0 || !f) return;
    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        char* p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == 0) continue;
        char key[64] = { 0 }; float val = 0.0f;
        if (sscanf_s(p, "%63[^=]=%f", key, (unsigned)sizeof(key), &val) != 2) continue;
        size_t n = strlen(key);
        while (n > 0 && (key[n-1] == ' ' || key[n-1] == '\t')) key[--n] = 0;
        if      (strcmp(key, "debug")    == 0) g_debug    = (val != 0.0f);
        else if (strcmp(key, "logLimit") == 0) g_logLimit = (int)val;
        else if (strcmp(key, "verbose")  == 0) g_verbose  = (val != 0.0f);
        else if (strcmp(key, "enabled")       == 0) g_enabled       = (val != 0.0f);
        else if (strcmp(key, "pathDist")      == 0) g_pathDist      = val;
        else if (strcmp(key, "maxDetour")     == 0) g_maxDetour     = val;
        else if (strcmp(key, "openThreshold") == 0) g_openThreshold = val;
        else if (strcmp(key, "pendingWait")   == 0) g_pendingWait   = val;
        else if (strcmp(key, "injectDelay")   == 0) g_injectDelay   = val;
        else if (strcmp(key, "moveEps")       == 0) g_moveEps       = val;
        else if (strcmp(key, "autoClose")     == 0) g_autoClose     = (val != 0.0f);
        else if (strcmp(key, "closeRadius")   == 0) g_closeRadius   = val;
        else if (strcmp(key, "closeDelay")    == 0) g_closeDelay    = val;
        else if (strcmp(key, "closeGrace")    == 0) g_closeGrace    = val;
        else if (strcmp(key, "closeGiveUp")   == 0) g_closeGiveUp   = val;
        else if (strcmp(key, "arriveEps")     == 0) g_arriveEps     = val;
        else if (strcmp(key, "areaFlag")      == 0) g_areaFlag      = (val != 0.0f);
    }
    fclose(f);
}

static void WriteConfigSnapshot()
{
    FILE* f = NULL;
    if (fopen_s(&f, "GateFix.cfg", "r") == 0 && f) { fclose(f); return; }
    if (fopen_s(&f, "GateFix.cfg", "w") != 0 || !f) return;
    fprintf(f, "# GateFix 설정 - 저장하면 몇 초 안에 자동 반영된다 (게임 켠 채로).\n\n");
    fprintf(f, "enabled=%d      # 자동 열기 전체 스위치. 0 이면 계측만 남는다.\n", g_enabled ? 1 : 0);
    fprintf(f, "pathDist=%.0f   # (폴백) 측면 판정이 못 고를 때만 쓰는 선분 거리 상한.\n", g_pathDist);
    fprintf(f, "maxDetour=%.0f    # 이 문을 거치느라 더 걷는 거리 한도. 이보다 크면 무관한 문으로 본다.\n", g_maxDetour);
    fprintf(f, "autoClose=%d    # 우리가 연 문을 사람이 없어지면 자동으로 닫고 원래 잠금 상태로 되돌린다.\n", g_autoClose ? 1 : 0);
    fprintf(f, "closeRadius=%.0f    # 이 반경에 아군이 없으면 비었다고 본다. 경비는 이 밖에 세워라.\n", g_closeRadius);
    fprintf(f, "closeDelay=%.0f    # 비어 있는 상태가 이만큼 지속되면 닫는다(초).\n", g_closeDelay);
    fprintf(f, "closeGrace=%.0f    # 마지막 통과 시작 후 이만큼은 무조건 안 닫는다(초).\n", g_closeGrace);
    fprintf(f, "closeGiveUp=%.0f    # 이 시간까지 못 닫으면 포기하고 열린 채 둔다(초).\n", g_closeGiveUp);
    fprintf(f, "openThreshold=%.2f  # 이 미만이면 닫힘. 재발행 문턱이기도 하다 (0.5 는 개방을 끊는다 — 실측).\n", g_openThreshold);
    fprintf(f, "pendingWait=%.0f    # 제자리에 멈춘 채 이만큼 지나면 대기 소거(초). 걷는 중엔 리셋.\n", g_pendingWait);
    fprintf(f, "injectDelay=%.1f    # 클릭 폭풍이 끝난 뒤 이만큼 지나서 주입(초).\n", g_injectDelay);
    fprintf(f, "moveEps=%.1f    # 250ms 표본 간 이동이 이보다 크면 막힌 게 아니라고 보고 취소.\n", g_moveEps);
    fprintf(f, "arriveEps=%.1f    # 정지 캐릭터가 목적지에서 이 안이면 도착으로 보고 취소.\n", g_arriveEps);
    fprintf(f, "areaFlag=%d    # getCharactersInArea bool 인자 (의미 미확정 — 0/1 대조용).\n", g_areaFlag ? 1 : 0);
    fprintf(f, "\n");
    fprintf(f, "debug=%d        # 계측 로그. 측정이 끝나면 0.\n", g_debug ? 1 : 0);
    fprintf(f, "verbose=%d      # 1 이면 잠금·열림 판정 호출을 전부 찍는다.\n", g_verbose ? 1 : 0);
    fprintf(f, "                # 0 이면 '참'인 답만 찍는다 (로그가 훨씬 짧다).\n");
    fprintf(f, "logLimit=%d    # 카테고리별 줄 수 상한\n", g_logLimit);
    fclose(f);
}

// v23 계열과 동일한 핫리로드 (게임 스레드에서만 부른다)
static void CheckConfigReload()
{
    static unsigned __int64 lastCheck = 0, lastMtime = 0;
    unsigned __int64 now = GetTickCount64();
    if (now - lastCheck < 3000) return;
    lastCheck = now;

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA("GateFix.cfg", GetFileExInfoStandard, &fad)) return;
    ULARGE_INTEGER u;
    u.LowPart  = fad.ftLastWriteTime.dwLowDateTime;
    u.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    if (u.QuadPart == 0) return;
    if (lastMtime == 0) { lastMtime = u.QuadPart; return; }
    if (u.QuadPart == lastMtime) return;
    lastMtime = u.QuadPart;

    LoadConfig();
    if (g_logLimit < 0) g_logLimit = 400;
    // 전문 출력 — 시작 로그와 같은 형식. 어느 값이 실제로 반영됐는지 눈으로 확인한다.
    Log(LC_INIT, "[cfg 재적용] debug=%d verbose=%d logLimit=%d | enabled=%d maxDetour=%.0f pathDist=%.0f openThreshold=%.2f autoClose=%d closeRadius=%.0f closeDelay=%.0f closeGrace=%.0f pendingWait=%.0f injectDelay=%.1f moveEps=%.1f arriveEps=%.1f",
        g_debug ? 1 : 0, g_verbose ? 1 : 0, g_logLimit,
        g_enabled ? 1 : 0, g_maxDetour, g_pathDist, g_openThreshold,
        g_autoClose ? 1 : 0, g_closeRadius, g_closeDelay, g_closeGrace, g_pendingWait,
        g_injectDelay, g_moveEps, g_arriveEps);
}

// ---------------------------------------------------------------------------
//  설치
// ---------------------------------------------------------------------------
static void Install(const char* label, const char* mangled, void* hook, void** orig)
{
    HMODULE lib = GetModuleHandleA("KenshiLib.dll");
    void* stub = lib ? (void*)GetProcAddress(lib, mangled) : 0;
    if (!stub) { Log(LC_INIT, "  %-24s 익스포트를 찾지 못했다 — 이 후크는 없다", label); return; }
    void* target = (void*)KenshiLib::GetRealAddress(stub);
    KenshiLib::HookStatus st = KenshiLib::AddHook(target, hook, orig);
    Log(LC_INIT, "  %-24s addr=%p  status=%d", label, target, (int)st);
}

//  진입점  (RE_Kenshi 가 "?startPlugin@@YAXXZ" 로 찾는다 — extern "C" 금지)
__declspec(dllexport) void startPlugin();

void startPlugin()
{
    LoadConfig();
    if (g_logLimit < 0) g_logLimit = 400;
    WriteConfigSnapshot();

    Log(LC_INIT, "GateFix 시작 (v3 — 게이트 자동 개방·자동 닫기 (상용))");
    Log(LC_INIT, "  debug=%d verbose=%d logLimit=%d | enabled=%d maxDetour=%.0f pathDist=%.0f(상한) openThreshold=%.2f autoClose=%d closeRadius=%.0f closeDelay=%.0f closeGrace=%.0f pendingWait=%.0f injectDelay=%.1f moveEps=%.1f arriveEps=%.1f",
        g_debug ? 1 : 0, g_verbose ? 1 : 0, g_logLimit,
        g_enabled ? 1 : 0, g_maxDetour, g_pathDist, g_openThreshold,
        g_autoClose ? 1 : 0, g_closeRadius, g_closeDelay, g_closeGrace, g_pendingWait,
        g_injectDelay, g_moveEps, g_arriveEps);

    Install("게이트생성",
            "??0GatewayBuilding@@QEAA@PEAVGameData@@AEBVVector3@Ogre@@AEBVQuaternion@3@PEAVFaction@@AEBVhand@@4@Z",
            (void*)&hookGateCtor, (void**)&origGateCtor);
    Install("게이트소멸",
            "??1GatewayBuilding@@UEAA@XZ",
            (void*)&hookGateDtor, (void**)&origGateDtor);
    Install("문짝생성",
            "??0DoorStuff@@QEAA@PEAVGameData@@AEBVVector3@Ogre@@AEBVQuaternion@3@PEAVFaction@@AEBVhand@@4PEAVLayout@@PEAVBuilding@@6@Z",
            (void*)&hookDoorCtor, (void**)&origDoorCtor);
    Install("문짝소멸",
            "??1DoorStuff@@UEAA@XZ",
            (void*)&hookDoorDtor, (void**)&origDoorDtor);
    Install("명령계측",
            "?addOrder@Character@@QEAAXPEAVBuilding@@W4TaskType@@PEAVRootObject@@_N3AEBVVector3@Ogre@@@Z",
            (void*)&hookAddOrder, (void**)&origAddOrder);
    Install("프레임틱",
            "?_NV_update4Frame@AI@@QEAAXM@Z",
            (void*)&hookUpdate4, (void**)&origUpdate4);
    Install("이동명령",
            "?_NV_playerMoveOrderDefault@Character@@QEAAXPEAVBuilding@@PEAVRootObject@@AEBVVector3@Ogre@@@Z",
            (void*)&hookMoveOrder, (void**)&origMoveOrder);
    Install("문열기점수",
            "?_NV_scoreUnlockDoorHere@AI@@QEAAMAEBVhand@@AEBVVector3@Ogre@@@Z",
            (void*)&hookScoreUnlock, (void**)&origScoreUnlock);
    Install("목적지게이트",
            "?getDestinationGate@AI@@QEAAMAEBVhand@@AEAV2@_N@Z",
            (void*)&hookDestGate, (void**)&origDestGate);
    Install("잠김판정",
            "?isDoorLocked@AI@@QEAA_NAEBVhand@@AEBVVector3@Ogre@@@Z",
            (void*)&hookIsLocked, (void**)&origIsLocked);
    Install("잠김+내가안",
            "?isDoorLockedAndMeInside@AI@@QEAA_NAEBVhand@@AEBVVector3@Ogre@@@Z",
            (void*)&hookInside, (void**)&origInside);
    Install("잠김+내가밖",
            "?isDoorLockedAndMeOutside@AI@@QEAA_NAEBVhand@@AEBVVector3@Ogre@@@Z",
            (void*)&hookOutside, (void**)&origOutside);
    Install("열림판정",
            "?isDoorOpen@AI@@QEAA_NAEBVhand@@AEBVVector3@Ogre@@@Z",
            (void*)&hookIsOpen, (void**)&origIsOpen);
    Install("침입자밖",
            "?intruderIsOutsideGates@AI@@QEAA_NAEBVhand@@AEBVVector3@Ogre@@@Z",
            (void*)&hookIntruder, (void**)&origIntruder);

    Log(LC_INIT, "설치 완료. status 가 0 이 아니거나 '찾지 못했다' 가 있으면 그 후크는 죽은 것이다.");
    Log(LC_INIT, "테스트: ①닫힌 게이트 수동 우클릭 해제 1회([명령] 줄로 subject 확정) ②닫힌 게이트 너머 클릭([주입]→[재발행] 확인).");
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { /* startPlugin 에서 처리 */ }
    return TRUE;
}
