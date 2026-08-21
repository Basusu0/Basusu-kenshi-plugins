// ============================================================================
//  SleepFix  —  RE_Kenshi / KenshiLib 플러그인
//
//  켄시 AI 는 행동마다 점수를 매겨 가장 높은 것을 고른다.
//  "침대로 가기"(_NV_scoreGoToBed) 점수가 작업류에 밀려서
//  부상자가 일하러 가는 것이 이 문제의 정체다.
//
//  이 플러그인은 그 점수 함수 두 개를 후킹해 배율만 곱한다.
//    _NV_scoreGoToBed                     부상 시 침대로
//    _NV_scoreGetOutOfBedOnceFullyHealed  다 나으면 일어나기
//
//  배율은 SleepFix.cfg 로 조정한다(재컴파일 불필요).
//  진단 로그는 SleepFix.log 에 남는다.
//
// ----------------------------------------------------------------------------
//  v38 변경점 — 코드 기본값을 "완성 상태"로 뒤집는다. cfg 가 없어도 본 기능이
//    켜진 채 뜬다: autoBedOrder 기본 1 (이전 기본 0 — cfg 를 지우면 자동 취침이
//    조용히 죽는 함정이었다), probeOrder 기본 0 (v33 진단은 필요할 때만).
//  v37 변경점 — cfg 핫리로드. 게임을 끄지 않고 SleepFix.cfg 를 고칠 수 있다.
//    hookUpdate4 에서 3초에 한 번 파일 수정 시각을 보고, 바뀌었으면
//    LoadConfig 를 다시 돌린 뒤 반영값을 전문 로그(LC_INIT)로 남긴다.
//    모든 항목이 런타임 전역이라 전부 재적용된다 (훅 설치 항목 없음).
//    게임 스레드에서만 돌므로 락 불필요. 재적용 시 스냅샷은 다시 쓰지 않는다
//    (쓰면 mtime 이 또 바뀌어 자기 자신을 트리거한다).
//  v36 변경점 — 로그에 슬롯 번호(#N)를 붙인다. 동명이인 때문이다.
//    Basusu 지적: 먼지 도적·먼지 두목·Not 2B 는 이름이 같은 캐릭터가 여럿이다
//    (포로 등). 그런데 v34 로그는 이름만 찍어서 구분이 안 됐다.
//    나는 그 로그를 "한 명이 침대를 갈아탄다"고 읽고 v35 를 만들었는데,
//    실은 **동명이인 여러 명이 각자 한 번씩** 받은 것일 수 있다.
//    그러면 재발행 자체가 없었고 고칠 것도 없다.
//    슬롯 번호는 캐릭터 포인터 하나당 하나라 이것으로 갈린다:
//      #7 이 반복되면 진짜 재발행, #7 #12 #19 로 흩어지면 각자 한 번씩이라 정상.
//    v35 의 in-bed 가드는 어느 쪽이든 해가 없으므로 그대로 둔다.
// ----------------------------------------------------------------------------
//  v35 변경점 — 자동 명령의 재발행을 막는다.
//    v34 인게임 결과: **2안 성공.** 상시 잡 보유자가 자동으로 침대로 갔다.
//      [자동] 바로 뒤에 [명령] 이 따라왔고, 값이 사람이 우클릭한 것과
//      한 글자도 다르지 않았다. 게임 스레드 발행도 문제없었다.
//    남은 것 하나: 같은 사람이 침대를 갈아타며 명령을 여러 번 받았다.
//      원인으로 보이는 것 — 침대에 도착하면 명령이 완료되어 사라지고
//      (hasPlayerOrder(258)=false), 아직 다쳤으니 발행 조건이 다시 성립한다.
//      그러면 자던 사람을 일으켜 다른 빈 침대로 보내게 된다.
//      → 침대에 누워 있으면 내지 않는다 (hookGetUp 의 isInBed 를 슬롯에 캐싱).
//    [주의] v34 로그에는 시각이 없어 "도배인지 정상 간격인지" 가릴 수 없었다.
//      나는 도배라고 단정했는데 근거가 없었다. 그래서 이번 판은
//      직전 발행으로부터 몇 초가 지났는지, 이번 부상 중 몇 번째인지를 함께 남긴다.
//      그 숫자를 보고 나서 더 조일지 정한다.
//    쿨다운 기본값 5초 -> 15초 (5배속이면 실시간 5초는 게임 안에서 짧다).
//    [오판 정정] 여러 명이 같은 침대를 후보로 잡는 것은 결함이 아니다.
//      침대가 인원보다 적으면 당연하고, 실제로는 빈 침대로 잘 분산된다
//      (인게임 확인). 로그의 중복 subject 만 보고 결함이라 했던 것을 취소한다.
// ----------------------------------------------------------------------------
//  v34 변경점 — 2안(직접 명령) 실장. 기본은 꺼져 있다.
//    v33 측정으로 우클릭 명령의 전 인자를 실측했고,
//    WASD Direct Control 1.3.1 소스로 인자 이름과 전례를 확인했다.
//    이제 추측으로 채우는 인자가 하나도 없다.
//
//    addOrder(Character* thisptr, Building* dest, TaskType t,
//             RootObject* subject, bool shift, bool clear,
//             const Ogre::Vector3& location)      <- WASD 소스의 이름
//      dest     = 침대가 놓인 건물  <- Building::furnitureParentBuilding()
//      t        = 258 (USE_BED_ORDER)              <- v33 실측
//      subject  = 침대 개체         <- findOwnedFreeBed out -> hand::getBuilding()
//      shift    = false (대기열에 쌓지 않음)        <- v33 실측 f1=0
//      clear    = true  (기존 명령을 지움)          <- v33 실측 f2=1
//      location = 침대 좌표         <- RootObjectBase::_NV_getPosition()
//    (v33 로그: b!=root 였고 root 의 vtable 이 [침대] 줄과 일치했다.
//     즉 dest 와 subject 는 서로 다른 객체다. 헷갈리지 말 것.)
//
//    스레드: 우클릭은 UI 스레드(48748), 우리는 게임 스레드(11376)에서 낸다.
//      WASD 가 게임 스레드에서 playerMoveOrderDefault 를 열 군데 가까이
//      호출하고 배포판이 정상 동작한다 — 게임 스레드 발행의 전례가 있다.
//      (그래도 미검증 조합이므로 기본 꺼짐으로 둔다.)
//
//    [함정] WASD 주석이 경고한 것: 명령을 매 프레임 다시 내면 경로탐색이
//      계속 초기화돼 캐릭터가 아예 출발하지 못한다("never-starts stutter").
//      그래서 발행 조건을 좁게 잡았다:
//        - 이미 USE_BED_ORDER 가 걸려 있으면 안 낸다 (hasPlayerOrder)
//        - 같은 캐릭터에게 bedOrderCooldownMs(기본 5초) 안에 다시 안 낸다
//        - 한 프레임에 한 명만
//        - 선택 중이거나 전투 중이면 건너뛴다
//      기존 clearBedOrder 가 회복 후 명령을 풀어주므로 복귀는 이미 된다.
//
//    설정: autoBedOrder=0 (기본 꺼짐), bedOrderCooldownMs=5000
//    발행 직전 인자 전부를 로그로 남긴다 — v33 의 우클릭 줄과 눈으로 대조할 것.
// ----------------------------------------------------------------------------
//  v33 변경점 — 2안(직접 명령) 준비. 재기만 한다. 동작 변경 없음.
//    미해결 "상시 잡 보유자 자동 취침"의 남은 길은 2안뿐이다:
//    점수로 잡을 이기려는 시도는 다섯 번 다 실패했고(2절 재시도 금지),
//    수동 우클릭은 항상 작동한다(OBEDIENCE 가 잡보다 위). 그러니
//    플러그인이 그 우클릭을 대신 내려주면 된다.
//    다만 아직 **해결책이 아니라 미착수 가설**이다. 모르는 것이 셋:
//      1. addOrder 인자를 어떻게 채워야 우클릭과 같아지는가
//         (Building*, TaskType, RootObject*, bool 둘, Vector3)
//      2. 플러그인이 낸 명령이 사람이 클릭한 것과 같은 취급을 받는가
//      3. findOwnedFreeBed 의 out(hand)에서 침대 건물을 꺼낼 수 있는가
//    사고 기록에 "함수 인자 의미를 추측하지 말 것 (howmany=0 오동작)"이 있고,
//    직전 용광로 건에서 추측으로 여덟 판을 날렸다. 그러니 먼저 잰다.
//    이번 판이 하는 일:
//      - hookAddOrder 가 **전 인자**를 찍는다. v32 는 root 와 pos 를
//        빠뜨렸는데 정확히 그 둘이 미지수다. b/root 의 vtable 과
//        b==root 여부까지 남겨 정체를 가른다.
//      - hookFindBed 가 out(hand)에서 hand::getBuilding() 으로 침대를
//        꺼내 찍는다 (익스포트 확인). 우클릭 때 넘어간 building 과
//        같은 포인터면 3번이 풀리고 2안의 재료가 갖춰진다.
//      - 위 둘은 probeOrder=1 이면 debug=0 에서도 찍힌다. 상한 60줄.
//    사용법: 다친 캐릭터를 침대에 **우클릭**한 뒤 로그를 본다.
// ----------------------------------------------------------------------------
//  v32 변경점
//    1. 슬롯 테이블 64 -> 128 + 오래된 슬롯 재사용.
//       스쿼드가 76인이라 64칸으로는 12명 이상이 영구 미등록이었고
//       (clearBedOrder·부상 추적 불능), 게임 재시작 없이 세이브를 불러오면
//       테이블이 옛 주소로 꽉 차서 그 뒤로는 전원이 미등록이 됐다.
//       이제 가장 오래 안 보인 슬롯을 비우고 재사용한다 (플래그도 초기화).
//    2. 디버그에만 쓰는 계산을 debug=0 에서 하지 않는다.
//       isFullyRested / getOverallHealthRating / CharName 이 로그에만
//       쓰이는데 매 평가마다 돌았다. 점수 후크는 초당 수백 번 불린다.
//       슬롯 등록은 hookUpdate4 의 CharName 이 이미 맡고 있어 영향 없다.
//    3. 스레드 확인 (LC_INIT, debug=0 에도 찍힘). 후크가 전부 같은
//       스레드면 한 줄만 찍힌다 — 락 없는 전역들이 안전하다는 증거.
//       "[주의] 스레드가 하나가 아니다" 가 뜨면 그때 락을 고민한다.
//    4. GetUpHealed 배율 로그 조건이 !g_debug 로 뒤집혀 있었다 (죽은 코드).
//
//  v31 변경점
//    1. debug=0 이면 시작 정보 외에는 아무것도 기록하지 않는다.
//       g_debug 를 개별 호출부에만 걸어둬서, 꺼도 "침대점수 하한",
//       "아직 회복 중" 같은 동작 기록이 계속 쌓였다. Log 진입부에서 막는다.
//
//  v30 변경점 (마무리)
//    1. cfg 읽기를 되살렸다. 다만 조정할 만한 항목만 읽는다.
//       실패로 확인된 넷(pauseJobs / skipJobs / forceGoal / skipSelected)은
//       cfg 에 뭐라고 적혀 있든 무시하고 꺼진 채로 고정한다.
//       실수로 켜면 기능이 되레 망가지기 때문이다.
//    2. debug / dumpParts 기본값을 끔으로. 진단이 끝났다.
//       다시 봐야 할 일이 생기면 cfg 에서 1 로 켜면 된다.
//
//  v29 변경점
//    1. 회복 판정을 derivedFleshHealthPercent(+0x60) 로 바꿨다.
//       이 값이 게임 화면에 뜨는 퍼센트 그 자체다. 실측 대조로 확정:
//         화면 머리 9 / 가슴 93 / 복부 58 / 오른다리 92
//         로그 P0.09 / P0.93 / P0.59 / P0.92          -- 전부 일치
//       그동안 쓰던 flesh/maxHealth 는 무관한 값이었다.
//       화면이 9%%일 때 79/100 이었고, 도로롱은 217/200 처럼 최대치를 넘겼다.
//       그래서 머리가 51%%인 캐릭터가 완전 회복으로 판정돼 침대에서 나갔다.
//    2. 부위 인덱스 순서도 확정됐다 (화면 순서와 같다):
//       0=머리 1=가슴 2=복부 3=왼팔 4=오른팔 5=왼다리 6=오른다리
//
//  v28 변경점
//    1. 부위마다 자기 정체를 함께 찍는다.
//         +0x08 whatAmI (TORSO=0 LEG=1 ARM=2 HEAD=3)
//         +0x20 side    (없음=0 좌=1 우=2 양쪽=3)
//       FCS 의 LOCATIONAL_DAMAGE "body part type" 과 값 체계가 같다.
//       실측 모순: 우리는 0번(오른다리) 79, 2번(왼다리) 93 이 손상됐다고 읽는데
//       게임 화면은 머리 52, 복부 87 이라고 한다. 서로 다른 부위를 보고 있다.
//       인덱스 추측 대신 부위가 스스로 밝히는 이름표로 대조한다.
//    2. 판정 로직은 이번에도 건드리지 않는다. 진단만 추가.
//
//  v27 변경점
//    1. 부위 덤프에 필드 네 개를 함께 찍는다.
//       F=flesh(+0x40)  W=wearDamage(+0x50)  M=_maxHealth(+0x54)
//       P=derivedFleshHealthPercent(+0x60)
//       실측 모순: 나가는 순간 F 는 전 부위 만땅인데 게임 화면은
//       머리 52 / 복부 87 이었다. 즉 화면이 보여주는 값은 flesh 가 아니다.
//       어느 필드가 화면과 일치하는지 확인한 뒤 그 값으로 판정을 바꾼다.
//       판정 로직 자체는 이번 판에서 건드리지 않는다.
//
//  v26 변경점
//    1. 부위 상태를 세 시점에 남긴다: 최초 / 명령해제시점 / 기상허용시점.
//       v25 의 최초 1회 덤프로는 "나갈 때 정말 만땅이었나"를 알 수 없었다.
//       실측 모순: 스노우 화이트가 최초 덤프에서 최저 0.7896(부상) 이었는데
//       잠시 뒤 medRested=1 로 통과했다. 그 사이 무엇이 바뀌었는지
//       나가는 순간의 값을 봐야 가려진다.
//
//  v25 변경점
//    1. 부위 덤프를 프레임 갱신 후크로 옮겼다.
//       GoToBed 후크에 두면 선택 중이거나 수동 명령이 걸린 캐릭터는
//       그 후크가 아예 안 불려서 덤프가 영원히 안 나온다.
//       실측: 스노우 화이트만 덤프가 없었고, 정작 그 캐릭터가 문제였다.
//    2. 덤프 줄에 최저 비율 / 읽기 성공 여부 / 최종 판정을 함께 찍는다.
//       머리 51%%인데 회복완료로 판정된 이유를 이 한 줄로 가른다.
//
//  v24 변경점
//    1. clearBedOrder 가 사용자의 새 명령을 즉시 취소하던 문제 수정.
//       회복 여부만 보고 판단해서, 이미 나은 캐릭터를 눕히면 그 자리에서
//       명령을 지워버렸다 (로그에 눕히기/해제가 여섯 번 반복).
//       이제 "다친 상태에서 걸린 취침 명령"만 표시해 두고 그것만 해제한다.
//    2. 로그의 "읽기실패" 표시는 거짓이었다. 인자 평가 순서 때문에
//       플래그가 함수 호출 전에 읽혔다. 값을 먼저 계산해서 넘긴다.
//       부위별 읽기 자체는 정상 작동 중이었다.
//    3. 부위 목록을 캐릭터당 한 번 통째로 덤프한다 (dumpParts).
//       머리 50 / 다른 부위 80 인데도 완전 회복으로 판정된 사례가 있어,
//       부위를 도는 방식이 실제로 무엇을 보고 있는지 확인해야 한다.
//
//  v23 변경점
//    1. 회복 판정을 부위별 현재/최대 비율로 바꿨다.
//       getOverallHealthRating() 은 자기 최대치 대비가 아니었다 — 실측:
//       부위 최대 75 인 삑은 전 부위 만땅인데 평점 0.74 에 머물러
//       영원히 회복 판정을 못 받았고, 최대 100 인 스노우 화이트만 일어났다.
//       이제 getPartCount / getPart / maxHealth 로 부위마다 비율을 재고
//       그중 최저값을 기준과 비교한다. 최대치가 낮은 캐릭터도 공평해진다.
//    2. healThreshold 를 다시 1.00 으로. 이제 "자기 최대치의 100%%" 라는 뜻이라
//       도달 가능하다.
//
//  v22 변경점
//    1. healThreshold 기본값을 1.00 -> 0.99 로 내렸다.
//       실측: getOverallHealthRating() 이 1.00 에 도달하지 않는다 (최대 0.99x).
//       기준을 1.00 으로 두면 아무도 완전 회복 판정을 못 받아
//       침대에서 영원히 안 나온다. v21 에서 실제로 그렇게 됐다.
//    2. 체력 평점을 소수점 넷째 자리까지 찍는다. 실제 상한을 보고
//       기준을 더 조일 수 있게 하려는 것.
//
//  v21 변경점
//    1. cfg 를 더 이상 읽지 않는다. 값은 이 파일 위쪽 전역 변수가 전부다.
//       예전에는 cfg 를 읽었는데, 파일에 새 항목이 없거나 옛 값이 남아서
//       "코드는 고쳤는데 동작은 그대로"인 사고가 네 번 났다
//       (skipJobs / forceGoal / goalPriority 미적용, getUpMult 되돌아감).
//       SleepFix.cfg 는 이제 현재 값을 보여주는 기록일 뿐이며 매 실행 덮어쓴다.
//    2. 기본값을 실전 설정으로 정리했다. 실패로 확인된 항목은 한 덩어리로 묶어
//       주석에 이유를 붙였다.
//
//  v20 변경점
//    1. 완전 회복 판정을 체력 평점 직접 비교로 바꿨다.
//       MedicalSystem::isFullyRested() 도 100% 기준이 아니었다 — 실측으로
//       최대 100 인 부위가 91 에서, 최대 75 인 부위가 69 에서 참이 됐다.
//       둘 다 약 91%. AI 판정과 문턱만 다를 뿐 같은 방식이었다.
//       이제 getOverallHealthRating() >= healThreshold 로 판정한다.
//       기본 1.00 = 전 부위 만땅. 이 값이 침대로 보내는 기준과
//       침대에서 내보내는 기준 양쪽에 함께 쓰인다.
//
//  v19 변경점
//    1. clearBedOrder — 100%% 회복 시 수동 취침 명령을 풀어준다.
//       실측: USE_BED_ORDER 이후 그 캐릭터의 GoToBed / GetUpHealed 로그가
//       통째로 사라졌다. 명령은 TP_OBEDIENCE 라 목표 평가 자체가 멈춘다.
//       그래서 다 나아도 안 일어나고 일도 재개하지 않았다.
//       명령을 풀면 평가가 재개되고 잡이 최고점을 받아 하던 일로 돌아간다.
//       침대 명령이 실제로 있을 때만 건드린다.
//
//  v18 변경점
//    1. stayInBed — 100%% 회복 전에는 침대에서 나오지 않는다.
//       실측: medRested=0 인데 기상 점수 orig=2.000 이 붙었다.
//       scoreGetOutOfBedOnceFullyHealed 는 이름과 달리 완전 회복 전에도
//       값을 낸다 (AI 기준 8~90%% 면 "다 쉬었다"로 본다).
//       침대로 보내는 쪽만 의료 기준으로 바꾸고 나오는 쪽을 안 바꾼 탓이었다.
//       비상 기상은 별도 태스크(TaskType 208)이고 바닐라에서 이미 URGENT 라
//       습격이 오면 정상적으로 일어난다.
//    2. skipJobs 는 실패로 확정. 켜면 침대 평가까지 멈춘다. 기본 0 유지.
//
//  v17 변경점
//    1. skipJobs — 부상 중에는 잡 후보 생성 자체를 건너뛴다.
//       실측: 침대 10점 @ NON_URGENT vs 잡 13점 @ URGENT. 점수로도 버킷으로도
//       못 이기고, 우리가 올린 점수는 목표로 가지도 않는다.
//       그래서 이기려 하지 않고 경쟁자를 안 만든다.
//       잡 목록은 손대지 않는다 (addPermajob 이 없어 제거는 되돌릴 수 없다).
//       매 프레임 후보 생성만 건너뛰므로 세이브에 남는 변화가 없다.
//    2. AITaskSytem -> 슬롯 대응표를 둔다. 잡 선택 후크는 AITaskSytem 만 받는데
//       거기서 Character 로 가는 익스포트가 없어, AI 후크에서 본 값을 기록해 쓴다.
//
//  v16 변경점
//    1. 주입 우선순위를 설정으로 뺐다 (goalPriority, 기본 3=URGENT).
//       v15 실측: URGENT 로 꽂으면 잡을 이기지만 게임이 곧바로 되찾아가서
//       초당 여러 번 재주입되는 왕복이 생겼다. 4(OBEDIENCE) 로 올리면
//       플레이어 명령과 같은 층이라 게임이 덜 뒤집을 가능성이 있다.
//    2. 주입 로그를 10 회마다 한 줄로 묶고 누적 횟수를 남긴다.
//       v15 는 주입 줄만으로 로그가 다 찼다.
//
//  v15 변경점
//    1. 주입 후크의 선택 검사를 skipSelected 에 연동 (기본 꺼짐).
//       v14 는 설정과 무관하게 무조건 건너뛰었고, 선택한 채로 저장된
//       캐릭터는 불러온 뒤에도 계속 선택 상태라 영구히 제외됐다.
//    2. 재주입 조건을 "현재 목표가 침대가 아닐 때"로 바꿨다.
//       이미 침대면 손대지 않는다(제자리걸음 방지),
//       잡이 뺏어가면 최소 간격만 지키고 되찾는다.
//    3. 건너뛴 사유는 캐릭터당 한 번만 기록. v14 는 이 줄이 로그를 다 먹었다.
//
//  v14 변경점
//    1. 시작 줄에 설정을 전부 찍는다. v13 까지는 네 개만 찍어서
//       forceGoal 이 켜졌는지 로그로 확인할 수 없었다.
//    2. 목표 주입을 건너뛸 때 그 이유를 남긴다.
//       forceGoal=100 인데 주입 로그가 0 줄인 상황을 가르기 위한 것.
//
//  v13 변경점
//    1. 목표 표시가 부실했다. getCurrentGoal().getTaskData() 가 대부분 널이라
//       "목표=(없음)(-1) 점수=13.00" 처럼 점수는 있는데 이름이 안 나왔다.
//       TaskMatch 가 Tasker 로 만들어진 경우 TaskData 가 비는 것으로 보인다.
//       그래서 sameAs(autoSleepTask) 로 "지금 목표가 자동취침인가"를 직접 묻는다.
//       => 침대목표=1 이면 침대가 이긴 것이다. 이름이 안 떠도 판정된다.
//    2. 선택/해제 로그를 부상자에 대해서만 남긴다.
//       v12 로그는 이 줄이 100 줄 넘게 예산을 먹었다.
//    3. "침대 강제 중단" 문구 수정. skipSelected=0 이면 중단하지 않는데
//       문구가 v10 시절 그대로였다.
//
//  v12 변경점
//    1. FCS 병행 전제. "Go home go to bed" AI Task 의 classification 을
//       TP_NON_URGENT -> TP_URGENT 로 올리는 .mod 패치와 함께 쓴다.
//       버킷을 올려야 잡과 같은 층에서 붙을 수 있고,
//       그 층에서 이기려면 점수도 잡(12~14) 위여야 한다. 둘 다 필요하다.
//    2. forceBed 를 "하한값"으로 바꿨다.
//       기존: 점수가 0 일 때만 대체 -> 0.1 인 캐릭터는 bedMult 에만 의존
//       변경: 배율 적용 후에도 forceBed 미만이면 forceBed 로 끌어올린다.
//       손잡이 하나로 0 인 애도 0.1 인 애도 같이 처리된다.
//    3. 선택 예외 제거 (skipSelected, 기본 끔).
//       선택 중에도 회복하러 가길 원하므로 v10 의 예외는 방향이 반대였다.
//       추적 자체는 남겨 로그로만 쓴다.
//    4. pauseJobs 는 남아 있지만 켜지 말 것. 잡을 끄면 침대 경로가 끊긴다.
//
//  v11 변경점
//    1. pauseJobs 를 기본 끔으로 돌린다.
//       실측 제보: 작업(잡) 스위치가 꺼져 있으면 회복하러 아예 안 간다.
//       즉 "침대로 가기"도 잡 시스템(doJobsEnabled)을 타고 나온다.
//       잡을 끄는 것은 침대 가는 길 자체를 끊는 짓이었다.
//    2. 대신 forceGoal — 목표를 직접 꽂는다.
//       잡은 URGENT(3) 버킷, 침대는 NON_URGENT(2) 버킷이라 점수로는 못 이긴다.
//       AITaskSytem 이 들고 있는 autoSleepTask 를
//       setCurrentGoal(task, 점수, TP_URGENT) 로 직접 현재 목표에 넣는다.
//       점수 경쟁을 우회하는 방식이다.
//    3. 주입은 점수 함수 안이 아니라 AI::update4Frame 후크에서 한다.
//       목표 선택이 진행 중인 함수 안에서 목표를 갈아끼우면 위험하다.
//       기본값 0(꺼짐). cfg 에서 켠다.
//
//  v10 변경점
//    1. 선택 상태를 드디어 읽는다. isThePlayer() 가 아니라
//       Character::_NV_select / _NV_unselect 를 후킹해서 직접 추적한다.
//       (둘 다 익스포트 확인. 가상함수지만 본체를 후킹하므로 vtable 경로도 잡힌다.)
//    2. 선택 중인 캐릭터는 "사용자가 조종 중"으로 본다.
//         - 내가 꺼둔 잡을 되돌린다 (일 시킬 수 있게)
//         - 침대 강제(forceBed / 잡 정지)를 하지 않는다
//       선택을 풀면 원래 동작으로 돌아가 다시 침대로 간다.
//       엔진이 선택 중엔 자동 행동을 보류하는 것과도 방향이 같다.
//
//  v9 변경점
//    1. 잡 일시정지를 "지속 감시"로 바꿨다.
//       v8 은 한 번 끄면 g_jobsPaused 플래그 때문에 다시 끄지 않았다.
//       그래서 부상 중에 잡이 다시 켜지면(수동 토글 등) 그대로 잡이
//       이겨버렸다 — v8 로그 실측: 일시정지 후 로그 없이 잡=1 로 복귀,
//       이후 OPERATE_MACHINERY(URGENT) 가 계속 승리.
//       이제 부상(의료 기준) 동안에는 잡이 켜져 있는 것이 보일 때마다
//       다시 끈다. 회복되면 원래대로 켠다.
//       => 부상 중 잡 버튼을 손으로 켜도 다음 판정 때 다시 꺼진다.
//          그게 싫으면 pauseJobs=0 으로 기능째 끄면 된다.
//
//  v8 변경점
//    1. pauseJobs — 부상자(의료 기준)의 잡을 일시정지한다.
//       실측 결과, 잡(JOB_MEDIC 14, LOOT_ANIMALS_JOB 13, OPERATE_MACHINERY 13)은
//       URGENT(3) 버킷에서 뽑히고 침대 목표는 NON_URGENT(2) 버킷이다.
//       버킷이 점수보다 우선이라 forceBed 를 500 으로 올려도 잡을 못 이긴다.
//       그래서 점수 싸움을 하지 않고, 다친 동안 잡 스위치(doJobsEnabled)를
//       꺼 버린다. 잡 버킷이 비면 침대 목표가 그대로 이긴다.
//       다 나으면(의료 기준 100%) 스위치를 되돌린다 = 하던 일 복귀.
//       내가 끈 것만 되돌린다. 원래 꺼져 있던 캐릭터는 건드리지 않는다.
//    2. 선택됨= 표기 제거. isThePlayer() 는 "선택된 캐릭터"가 아니라
//       플레이어 팩션 여부였다 (전원 1 로 찍힘). 선택 상태는 이 경로로
//       감지할 수 없다. 선택 중 침대로 안 가는 것은 엔진 동작으로 받아들인다.
//
//  v7 변경점
//    1. 부상 판정을 MedicalSystem::isFullyRested() 로 바꾼다.
//       AI::isFullyRested 는 사지가 8~90% 만 되어도 "쉴 만큼 쉬었다"로 본다.
//       그래서 수동으로 빼내면 다시 안 들어갔다.
//       의료 쪽 판정은 전부 100% 여야 참이므로 "조금이라도 다치면"에 맞는다.
//    2. 진단 확장: 부상자 한 줄에 아래를 전부 찍는다.
//         medRested  의료 판정 (100% 회복인가)
//         health     전체 체력 평점
//         aiRested   AI 판정 (기존 것)
//         theePlayer 지금 선택된 캐릭터인가   <- 선택하면 안 가는 현상 확인용
//         orders     플레이어 명령이 남아 있는가
//         jobs       잡 활성 / 상시잡 개수     <- 연구하러 가는 현상 확인용
//
//  v6 변경점
//    1. 이름 복원. HEX 덤프로 확정한 실제 배치는 아래와 같다.
//         Character + 0x18   displayName 시작
//           +0x00  SSO 버퍼 16바이트 (또는 용량>=16 이면 힙 포인터)
//           +0x10  길이
//           +0x18  용량
//       v4 가 틀렸던 이유: 앞에 프록시 포인터가 있다고 봤는데 없었다. 8바이트씩 밀렸다.
//    2. HEX 덤프는 dumpHex=1 일 때만 (이제 필요 없다).
//    3. GetUpHealed 줄에 rested 를 같이 찍는다.
//       다 낫기 전에 일어나기 점수가 붙는지 확인하려는 것.
//
//  v5 변경점
//    1. [실험] forceBed — 침대 점수가 0 일 때, 침대가 실제로 찾히면
//       0 대신 이 값을 돌려준다. 0 이 "해당 없음"인지 그냥 "낮음"인지 가른다.
//       기본값 0(꺼짐). cfg 에서 켠다.
//    2. 캐릭터 이름을 C1, C2 … 로 바꿨다. v4 의 displayName 직접 읽기는
//       "[? 존]" 처럼 앞부분이 깨졌다 — 오프셋이 틀렸다.
//       대신 Character 앞 0x60 바이트를 캐릭터당 한 번만 덤프한다(HEX 줄).
//       그걸 보고 문자열 위치를 확정한 뒤 다음 판에 이름을 되살린다.
//    3. 로그 잡음 제거: 멀쩡한 캐릭터의 GoToBed, 침대 밖 GetUpHealed 는
//       기록하지 않는다. v4 는 이 두 줄이 상한을 다 먹었다.
//
//  v4 변경점 (진단 전용 — 동작 변경 없음)
//    1. 로그 줄마다 캐릭터 이름을 붙인다. v3 는 누구 줄인지 구분이 안 됐다.
//    2. 중복 접기의 마지막 묶음이 버려지던 문제 수정
//       (다른 카테고리에 줄이 써질 때 밀린 반복 횟수를 전부 흘린다).
//    3. 핵심 진단: 부상자인데 침대점수가 0 인 순간,
//       findOwnedFreeBed(justAsking=true) 를 직접 불러서
//       "쓸 수 있는 침대를 찾을 수 있는 상태인지"를 따로 확인한다.
//         결과>0  침대는 찾히는데 점수가 0  -> 부상 판정 쪽이 막고 있다
//         결과<=0 침대를 못 찾는다          -> 소유·배정 쪽이 원인이다
//
//  v3 변경점
//    1. 현재 목표 덤프 추가: 다쳤는데 침대로 안 가는 순간의
//       "지금 무슨 목표를 잡고 있는지 / 그 점수 / 그 우선순위 버킷"을 남긴다.
//       배율(1안)이 통할 수 있는 상황인지 아닌지가 이 한 줄로 갈린다.
//    2. TaskType 291개 이름표 내장. 로그에 숫자 대신 USE_BED(98) 로 찍힌다.
//    3. KenshiLib.def 대신 배포된 KenshiLib.lib 를 링크하면 된다
//       (임포트 라이브러리 확인함 — 심볼 9824개). def 는 이제 필요 없다.
//
//  v2 변경점 (v1 대비 — 기능은 그대로, 안정성과 로그 품질만 손봄)
//    1. 로그를 카테고리별로 따로 세고, 직전과 같은 줄은 접는다.
//       v1 은 상한 200 줄을 NPC 호출과 같은 줄 반복으로 채워버려서
//       정작 아군 부상자 줄이 안 남을 수 있었다.
//    2. 점수 후크는 아군이 아니면 즉시 원본을 돌려준다 (기록도 안 함).
//    3. 모든 후크에 원본 포인터 널 가드. 후킹 6개 중 하나라도 실패하면
//       v1 은 그 함수가 처음 불릴 때 게임이 죽는다.
//    4. 주소 해석 실패(0) 시 AddHook 을 호출하지 않는다.
//    5. h / v 포인터 널 가드 (isFullyRested, isInBed 역참조).
// ----------------------------------------------------------------------------
//
//  [빌드] Visual Studio 2022 / x64 / Release.
//         DLL 경계를 넘는 타입이 포인터·bool·float·enum 뿐이라
//         런타임 버전이 달라도 ABI 문제가 없다.
//         단, std::string 을 반환하는 KenshiLib API
//         (hand::toString, BinaryVersion::GetVersion 등)는 쓰지 말 것.
//
//  [주의] 진입점에 extern "C" 를 붙이면 안 된다.
//         RE_Kenshi 는 "?startPlugin@@YAXXZ" 로 찾는다.
//
//  [주의] KenshiLib 공개 헤더에는 아래 함수들이 선언돼 있지 않다.
//         그래서 맹글링 이름이 일치하도록 클래스를 직접 선언한다.
//         클래스 이름 / class·struct / const 여부 / 인자 타입이
//         하나라도 다르면 링크가 깨진다. 수정하지 말 것.
// ============================================================================

// VS2022 는 SDL 검사가 기본 활성이라 fopen/sscanf 의 C4996 을 에러로 올린다.
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


// ---------------------------------------------------------------------------
//  게임 클래스 선언 (맹글링 일치용 — 멤버 레이아웃은 쓰지 않는다)
// ---------------------------------------------------------------------------
// Ogre::Vector3 은 전방 선언만으로는 부족하다.
// 값을 반환하는 함수는 **반환 타입까지 맹글링에 들어가므로**, 다른 이름으로
// 흉내내면 링크가 깨진다. v34 첫 빌드가 그랬다:
//   우리가 만든 것 : ?_NV_getPosition@RootObjectBase@@QEAA?AUVec3Raw@@XZ
//   실제 심볼      : ?_NV_getPosition@RootObjectBase@@QEAA?AVVector3@Ogre@@XZ
//   (U=구조체 / V=클래스 인 것까지 다르다)
// 그래서 이름·종류를 그대로 맞춘 최소 정의를 둔다. float 3개 레이아웃은 확정이고,
// 12바이트라 x64 에서는 어차피 숨은 포인터로 반환된다 — 실제 Ogre 와 같은 방식이다.
namespace Ogre
{
    class Vector3
    {
    public:
        float x, y, z;
    };
}

// 전방 선언은 hand 보다 **위**에 있어야 한다.
// v33 에서 hand::getBuilding() 을 추가하면서 Building 이 아래에 있어
// "';'이(가) '*' 앞에 없습니다 / getBuilding 은 hand 의 멤버가 아닙니다" 로 무너졌다.
class Building;
class RootObject;
class MedicalSystem;      // 본체 선언은 아래에 있다. Character 가 포인터로 먼저 쓴다.
enum TaskType { TASKTYPE_UNKNOWN };   // 맹글링용. 실제 값은 TASK_NAMES 표 참조
// 실제 크기는 0x20 (vtable 포인터 + type/container/containerSerial/index/serial).
// 여유를 둬서 64바이트로 잡는다. 게임 생성자가 앞 0x20 만 채워도 무해하다.
class hand
{
public:
    hand();                 // ??0hand@@QEAA@XZ
    // ?getBuilding@hand@@QEBAPEAVBuilding@@XZ  (익스포트 확인)
    // findOwnedFreeBed 의 out 에서 침대 건물을 꺼내는 데 쓴다.
    Building* getBuilding() const;
private:
    char _storage[64];
};

class Character
{
public:
    bool isPlayerCharacter() const;   // ?isPlayerCharacter@Character@@QEBA_NXZ
    bool isBeingCarried() const;      // ?isBeingCarried@Character@@QEBA_NXZ
    bool isDead() const;              // ?isDead@Character@@QEBA_NXZ
    bool isInCombatMode(bool, bool) const;
    void addOrder(Building*, TaskType, RootObject*, bool, bool, const Ogre::Vector3&);
    void addJob(TaskType, RootObject*, bool, bool, const Ogre::Vector3&);
    void reThinkCurrentAIAction();
    // 플레이어가 우클릭할 때 게임이 부르는 함수. TaskType 이 없다 —
    // 건물만 넘기면 게임이 알아서 그 건물에 맞는 행동을 고른다.
    void _NV_playerMoveOrderDefault(Building*, RootObject*, const Ogre::Vector3&);
    MedicalSystem* getMedical();      // ?getMedical@Character@@QEAAPEAVMedicalSystem@@XZ
    void _NV_select();                // ?_NV_select@Character@@QEAAXXZ
    void _NV_unselect();              // ?_NV_unselect@Character@@QEAAXXZ
};

// v34: 침대(가구)에서 소속 건물과 좌표를 얻는다.
// addOrder 의 dest 와 location 에 넣을 값이다. 둘 다 익스포트 확인.
class Building
{
public:
    // ?furnitureParentBuilding@Building@@QEBAPEAV1@XZ
    // 가구(침대)가 속한 건물. v33 로그의 dest 가 이것으로 보인다.
    Building* furnitureParentBuilding() const;
    // ?isFurniture@Building@@QEBA_NXZ
    bool isFurniture() const;
};

// ?_NV_getPosition@RootObjectBase@@QEAA?AVVector3@Ogre@@XZ
// 반환 타입이 맹글링에 들어가므로 Ogre::Vector3 그대로여야 한다.
class RootObjectBase
{
public:
    Ogre::Vector3 _NV_getPosition();
};

class Tasker;
class TaskData;
class AITaskSytem;

class TaskMatch
{
public:
    // ?getTaskData@TaskMatch@@QEBAPEBVTaskData@@XZ
    const TaskData* getTaskData() const;
    // ?key@TaskMatch@@QEBA?AW4TaskType@@XZ   — enum 반환이라 ABI 안전
    TaskType key() const;
    // ?sameAs@TaskMatch@@QEBA_NPEBVTasker@@@Z
    bool sameAs(const Tasker* t) const;
};

class MedicalSystem
{
public:
    // 부위 하나. flesh(현재 체력)는 익스포트가 없어 오프셋으로 읽는다.
    class HealthPartStatus
    {
    public:
        float maxHealth() const;   // ?maxHealth@HealthPartStatus@MedicalSystem@@QEBAMXZ
    };

    bool  isFullyRested() const;          // ?isFullyRested@MedicalSystem@@QEBA_NXZ
    float getOverallHealthRating() const; // ?getOverallHealthRating@MedicalSystem@@QEBAMXZ
    int   getPartCount() const;           // ?getPartCount@MedicalSystem@@QEBAHXZ
    // ?getPart@MedicalSystem@@QEAAPEAVHealthPartStatus@1@_K@Z
    HealthPartStatus* getPart(unsigned __int64 index);
};

class OrdersReceiver
{
public:
    // ?getCurrentGoal@OrdersReceiver@@QEBAAEBVTaskMatch@@XZ
    const TaskMatch& getCurrentGoal() const;
    bool hasPlayerOrders() const;         // ?hasPlayerOrders@OrdersReceiver@@QEBA_NXZ
    // ?hasPlayerOrder@OrdersReceiver@@QEBA_NW4TaskType@@@Z  특정 명령이 있는지
    bool hasPlayerOrder(TaskType t) const;
    void clearOrders();                   // ?clearOrders@OrdersReceiver@@QEAAXXZ
    bool hasOrders() const;               // ?hasOrders@OrdersReceiver@@QEBA_NXZ
    bool isJobsEnabled() const;           // ?isJobsEnabled@OrdersReceiver@@QEBA_NXZ
    int  getPermajobCount() const;        // ?getPermajobCount@OrdersReceiver@@QEBAHXZ
};

// 실제 레이아웃도 OrdersReceiver 가 오프셋 0 이라 this 보정이 없다.
enum taskPriority { TP_JUST_ACTION, TP_FLUFF, TP_NON_URGENT, TP_URGENT, TP_OBEDIENCE };

class AITaskSytem : public OrdersReceiver
{
public:
    bool isThePlayer();               // ?isThePlayer@AITaskSytem@@QEAA_NXZ
    void _NV_setNeedGOAP();           // ?_NV_setNeedGOAP@AITaskSytem@@QEAAXXZ  목표 재평가 요청
    // ?_NV_setCurrentGoal@AITaskSytem@@QEAAXPEAVTasker@@MW4taskPriority@@@Z
    void _NV_setCurrentGoal(Tasker* t, float score, taskPriority pri);
};

class AI
{
public:
    Character* getCharacter();        // ?getCharacter@AI@@QEAAPEAVCharacter@@XZ
    AITaskSytem* getTaskSystem() const;  // ?getTaskSystem@AI@@QEBAPEAVAITaskSytem@@XZ
    void _NV_update4Frame(float t);      // ?_NV_update4Frame@AI@@QEAAXM@Z
    float _NV_scoreGoToBed(const hand&, const Ogre::Vector3&);
    float _NV_scoreGetOutOfBedOnceFullyHealed(const hand&, const Ogre::Vector3&);
    bool  isFullyRested(const hand&, const Ogre::Vector3&);  // 다 나았는가
    bool  isInBed(const hand&, const Ogre::Vector3&);        // 지금 침대에 있는가
    float findOwnedFreeBed(const hand&, hand&, bool);        // 쓸 수 있는 침대 탐색
};

namespace KenshiLib
{
    enum HookStatus { HOOK_UNKNOWN };
    HookStatus AddHook(void* target, void* hook, void** original);
    __int64    GetRealAddress(void* func);
}

// 멤버함수 포인터 -> void*  (KenshiLib 이 쓰는 union 방식과 동일)
template<typename T>
static void* PMF(T f)
{
    union { T src; void* dst; } u;
    u.src = f;
    return u.dst;
}

// ---------------------------------------------------------------------------
//  설정
// ---------------------------------------------------------------------------
static float g_bedMult   = 15.0f;  // 침대로 가기 점수 배율
static float g_getUpMult = 1.00f;  // 기상 점수 배율. 1 이 맞다 — 올리면 조기 기상
static bool  g_skipCombat = true;  // 전투 중에는 보정하지 않는다
static int   g_logLimit  = 200;    // 카테고리별 로그 줄 수 상한 (0 = 로그 끔)
static bool  g_debug     = false;  // 진단 로그. 필요할 때 cfg 에서 1 로 켠다
static float g_forceBed  = 30.0f;  // 침대 점수 하한. 0 점 캐릭터를 후보로 올린다
static bool  g_dumpHex   = false;  // 캐릭터 메모리 덤프 (이름 오프셋 조사용)
static bool  g_probeOrder = false;  // v33: 우클릭 명령의 전 인자와 침대 손잡이를
                                   // 찍는다. debug=0 에도 찍힌다. 상한 60줄.
static int   g_probeLines = 0;
static bool  g_autoBedOrder = true;  // v34: 다친 아군에게 침대 명령을 대신 낸다.
                                      // 상시 잡 보유자도 눕는다. v38 부터 기본 켬.
static int   g_bedOrderCooldownMs = 15000;  // 같은 사람에게 다시 낼 때까지 최소 간격.
                                      // 짧으면 경로탐색이 계속 초기화돼 출발을 못 한다
                                      // (WASD 소스가 경고한 never-starts stutter).
static bool  g_dumpParts = false;  // 부위 목록 덤프. 필요할 때 cfg 에서 켠다
static bool  g_stayInBed = true;   // 완전 회복 전에는 침대에서 안 나온다
static bool  g_clearBedOrder = true;  // 회복되면 수동 취침 명령을 풀어준다
static float g_healThreshold = 1.00f; // 완전 회복 기준. 부위별 현재/최대 비율.
                                      // 1.00 = 전 부위 만땅 (자기 최대치 기준)

// --- 아래는 전부 실패로 확인된 것들. 켜지 말 것 ---
static bool  g_pauseJobs = false;  // 잡을 끄면 침대로 가는 경로까지 끊긴다
static bool  g_skipJobs  = false;  // 잡 후보를 막으면 침대 평가도 같이 멈춘다
static float g_forceGoal = 0.0f;   // 목표를 꽂아도 게임이 되돌린다. 왕복만 생긴다
static int   g_goalEvery = 300;    // forceGoal 이 0 이면 의미 없음
static int   g_goalPriority = 3;   // forceGoal 이 0 이면 의미 없음
static bool  g_skipSelected = false; // 선택한 채 저장하면 그 캐릭터가 영구 제외된다

// ---------------------------------------------------------------------------
//  로그 — 카테고리별로 따로 세고, 직전과 같은 줄은 접는다.
//  점수 함수는 초당 여러 번 호출되므로 이게 없으면
//  상한이 똑같은 줄로 순식간에 차버린다.
// ---------------------------------------------------------------------------
enum LogCat { LC_INIT = 0, LC_BED, LC_GETUP, LC_FINDBED, LC_ORDER, LC_JOB, LC_MOVE, LC_GOAL, LC_PROBE, LC_HEX, LC_JOBSW, LC_SEL, LC_INJECT, LC_PARTS, LC_COUNT };

static int  g_written[LC_COUNT] = { 0 };
static char g_last[LC_COUNT][512] = { { 0 } };
static int  g_dup[LC_COUNT] = { 0 };

// 밀려 있던 반복 횟수를 전부 흘린다.
// v3 는 같은 카테고리에 다음 줄이 와야만 흘려서, 마지막 묶음이 통째로 사라졌다.
static void FlushDups(FILE* f, int except_cat)
{
    for (int i = 0; i < LC_COUNT; ++i)
    {
        if (i == except_cat || g_dup[i] <= 0) continue;
        fprintf(f, "        (앞선 다른 줄 %d회 반복)\n", g_dup[i]);
        g_dup[i] = 0;
    }
}

static void Log(int cat, const char* fmt, ...)
{
    if (g_logLimit <= 0 || cat < 0 || cat >= LC_COUNT) return;
    // debug=0 이면 시작 정보만 남긴다.
    // 예전에는 g_debug 를 개별 호출부에만 걸어서, 끄고도 동작 기록
    // (침대점수 하한, 아직 회복 중 등)이 계속 쌓였다.
    // 단, v33 의 진단(LC_PROBE)은 debug=0 에서도 통과시킨다 — 평소 플레이
    // 중에 우클릭 한 번만 하면 되도록 만든 것이라 로그를 켤 필요가 없다.
    if (!g_debug && cat != LC_INIT && cat != LC_PROBE) return;
    if (g_written[cat] >= g_logLimit) return;

    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);

    if (strcmp(buf, g_last[cat]) == 0) { ++g_dup[cat]; return; }   // 같은 줄 반복은 접는다

    FILE* f = NULL;
    if (fopen_s(&f, "SleepFix.log", "a") != 0 || !f) return;
    if (g_dup[cat] > 0) { fprintf(f, "        (위 줄 %d회 반복)\n", g_dup[cat]); g_dup[cat] = 0; }
    FlushDups(f, cat);
    fprintf(f, "%s\n", buf);
    fclose(f);

    strcpy_s(g_last[cat], sizeof(g_last[cat]), buf);
    ++g_written[cat];
}

// ---------------------------------------------------------------------------
//  [일회 진단] 후크들이 한 스레드에서만 불리는지 확인한다.
//  LC_INIT 이라 debug=0 에도 찍힌다. 한 줄만 찍히면 단일 스레드 —
//  락 없는 전역(g_ids, g_last 등)이 전부 안전하다는 확정 증거다.
//  "[주의]" 줄이 뜨면 그때 락을 고민한다. 첫 확인 이후 비용은
//  스레드ID 조회 + 비교 몇 번이라 상시 켜 둬도 무해하다.
// ---------------------------------------------------------------------------
static void LogThreadOnce(const char* where)
{
    static DWORD ids[4] = { 0 };
    static int   n = 0;
    DWORD id = GetCurrentThreadId();
    for (int i = 0; i < n; ++i)
        if (ids[i] == id) return;
    if (n < 4) ids[n++] = id;
    Log(LC_INIT, "  스레드 확인: %s = %lu%s", where, (unsigned long)id,
        n > 1 ? "  [주의] 스레드가 하나가 아니다 — 락 필요" : "");
}

// ---------------------------------------------------------------------------
//  설정 기록
//
//  이제 cfg 를 읽지 않는다. 값은 전부 위의 전역 변수에 박혀 있다.
//
//  전에는 cfg 를 읽었는데, 파일에 새 항목이 없거나 예전 값이 남아 있어서
//  "코드는 고쳤는데 동작은 그대로"인 상황이 네 번 났다
//  (skipJobs 미적용, forceGoal 미적용, goalPriority 미적용, getUpMult 되돌아감).
//  값을 코드 한 곳에만 두면 그 부류의 사고가 사라진다.
//
//  SleepFix.cfg 는 이제 "지금 무슨 값으로 돌고 있는지" 확인용으로만 쓴다.
//  매 실행마다 덮어쓰며, 손으로 고쳐도 반영되지 않는다.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  설정
//
//  조정할 만한 항목만 cfg 에서 읽는다.
//  파일에 없는 항목은 코드 기본값을 쓰므로, 항목이 늘어도 조용히 죽지 않는다.
//
//  실패로 확인된 넷(pauseJobs / skipJobs / forceGoal / skipSelected)은
//  cfg 에 뭐라고 적혀 있든 무시하고 코드값(전부 꺼짐)으로 고정한다.
//  실수로 켜면 기능이 되레 망가지기 때문이다. cfg 에는 참고용으로만 적는다.
//
//  읽은 값은 시작 로그에 그대로 찍힌다. 반영 여부는 로그 첫 줄로 확인한다.
// ---------------------------------------------------------------------------
static void WriteConfigSnapshot();

static void LoadConfig()
{
    FILE* f = NULL;
    if (fopen_s(&f, "SleepFix.cfg", "r") != 0 || !f)
    {
        WriteConfigSnapshot();          // 없으면 기본값으로 하나 만들어 둔다
        return;
    }

    char line[256] = { 0 };
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        char key[64] = { 0 }; float val = 0.0f;
        if (sscanf_s(line, "%63[^=]=%f", key, (unsigned)sizeof(key), &val) != 2) continue;

        if      (strcmp(key, "debug")         == 0) g_debug         = (val != 0.0f);
        else if (strcmp(key, "dumpParts")     == 0) g_dumpParts     = (val != 0.0f);
        else if (strcmp(key, "dumpHex")       == 0) g_dumpHex       = (val != 0.0f);
        else if (strcmp(key, "probeOrder")    == 0) g_probeOrder    = (val != 0.0f);
        else if (strcmp(key, "autoBedOrder")  == 0) g_autoBedOrder  = (val != 0.0f);
        else if (strcmp(key, "bedOrderCooldownMs") == 0) g_bedOrderCooldownMs = (int)val;
        else if (strcmp(key, "logLimit")      == 0) g_logLimit      = (int)val;
        else if (strcmp(key, "healThreshold") == 0) g_healThreshold = val;
        else if (strcmp(key, "forceBed")      == 0) g_forceBed      = val;
        else if (strcmp(key, "bedMult")       == 0) g_bedMult       = val;
        else if (strcmp(key, "getUpMult")     == 0) g_getUpMult     = val;
        else if (strcmp(key, "stayInBed")     == 0) g_stayInBed     = (val != 0.0f);
        else if (strcmp(key, "clearBedOrder") == 0) g_clearBedOrder = (val != 0.0f);
        else if (strcmp(key, "skipCombat")    == 0) g_skipCombat    = (val != 0.0f);
        // 그 밖의 키는 의도적으로 무시한다 (실패로 확인된 항목들).
    }
    fclose(f);

    // 너무 짧으면 명령을 다시 내느라 경로탐색이 계속 초기화돼
    // 캐릭터가 출발을 못 한다 (WASD 소스가 기록한 never-starts stutter).
    if (g_bedOrderCooldownMs < 1000 || g_bedOrderCooldownMs > 600000)
        g_bedOrderCooldownMs = 15000;
}

// --- v37 cfg 핫리로드. hookUpdate4 (게임 스레드) 에서만 부른다 ---
static void CheckConfigReload()
{
    static unsigned __int64 lastCheck = 0;
    static unsigned __int64 lastMtime = 0;

    unsigned __int64 now = GetTickCount64();
    if (now - lastCheck < 3000) return;
    lastCheck = now;

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA("SleepFix.cfg", GetFileExInfoStandard, &fad)) return;
    ULARGE_INTEGER u;
    u.LowPart  = fad.ftLastWriteTime.dwLowDateTime;
    u.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    if (u.QuadPart == 0) return;
    // 첫 확인은 기준점만 잡는다 — 시작 시 WriteConfigSnapshot 이 파일을
    // 다시 쓰므로, 그 mtime 을 기준으로 삼아야 헛 리로드가 없다.
    if (lastMtime == 0) { lastMtime = u.QuadPart; return; }
    if (u.QuadPart == lastMtime) return;
    lastMtime = u.QuadPart;

    LoadConfig();   // 클램프 포함 (위 함수 안에 있다). 스냅샷은 다시 쓰지 않는다.
    Log(LC_INIT, "[cfg 재적용] autoBedOrder=%d bedOrderCooldownMs=%d healThreshold=%.2f stayInBed=%d clearBedOrder=%d skipCombat=%d",
        g_autoBedOrder ? 1 : 0, g_bedOrderCooldownMs, g_healThreshold,
        g_stayInBed ? 1 : 0, g_clearBedOrder ? 1 : 0, g_skipCombat ? 1 : 0);
    Log(LC_INIT, "[cfg 재적용] forceBed=%.1f bedMult=%.2f getUpMult=%.2f debug=%d dumpParts=%d dumpHex=%d probeOrder=%d logLimit=%d",
        g_forceBed, g_bedMult, g_getUpMult, g_debug ? 1 : 0,
        g_dumpParts ? 1 : 0, g_dumpHex ? 1 : 0, g_probeOrder ? 1 : 0, g_logLimit);
}

static void WriteConfigSnapshot()
{
    FILE* f = NULL;
    if (fopen_s(&f, "SleepFix.cfg", "w") != 0 || !f) return;

    fprintf(f, "# SleepFix 설정 - 저장하면 몇 초 안에 자동 반영된다 (게임 켠 채로).\n");
    fprintf(f, "# 아래 항목만 읽는다. (고정) 표시는 파일을 고쳐도 무시된다.\n\n");

    fprintf(f, "# --- 회복 기준 ---\n");
    fprintf(f, "healThreshold=%.3f   # 완전 회복 기준. 화면에 뜨는 부위 퍼센트 그대로다.\n", g_healThreshold);
    fprintf(f, "                     # 1.000 = 전 부위 100%%. 회복이 길면 0.95 로 낮춘다\n");
    fprintf(f, "stayInBed=%d           # 완전 회복 전에는 침대에서 안 나온다\n", g_stayInBed ? 1 : 0);
    fprintf(f, "clearBedOrder=%d       # 회복되면 수동 취침 명령을 풀어 하던 일로 돌려보낸다\n\n", g_clearBedOrder ? 1 : 0);

    fprintf(f, "# --- 침대로 보내기 ---\n");
    fprintf(f, "forceBed=%.2f        # 침대 점수 하한. 점수 0 인 캐릭터를 후보로 올린다\n", g_forceBed);
    fprintf(f, "bedMult=%.2f         # 침대 점수 배율 (하한이 더 커서 거의 안 쓰인다)\n", g_bedMult);
    fprintf(f, "getUpMult=%.2f         # 기상 점수 배율. 1 이 맞다 (올리면 조기 기상)\n", g_getUpMult);
    fprintf(f, "skipCombat=%d          # 전투 중에는 보정하지 않는다\n\n", g_skipCombat ? 1 : 0);

    fprintf(f, "# --- 로그 ---\n");
    fprintf(f, "debug=%d               # 0 이면 진단 로그를 끈다 (평소에는 0)\n", g_debug ? 1 : 0);
    fprintf(f, "dumpParts=%d           # 부위 목록 덤프\n", g_dumpParts ? 1 : 0);
    fprintf(f, "dumpHex=%d             # 캐릭터 메모리 덤프 (이름 오프셋 조사용)\n", g_dumpHex ? 1 : 0);
    fprintf(f, "logLimit=%d          # 카테고리별 줄 수 상한\n\n", g_logLimit);

    fprintf(f, "# --- 자동 취침 명령 (v34, 실험) ---\n");
    fprintf(f, "autoBedOrder=%d       # 다친 아군에게 침대 명령을 대신 낸다.\n", g_autoBedOrder ? 1 : 0);
    fprintf(f, "                      # 연구·채굴 같은 상시 잡 보유자도 눕는다.\n");
    fprintf(f, "                      # 명령은 사람이 우클릭하는 것과 같은 경로다.\n");
    fprintf(f, "                      # 회복되면 clearBedOrder 가 알아서 풀어준다.\n");
    fprintf(f, "                      # v38 부터 기본 1 (인게임 검증 완료).\n");
    fprintf(f, "bedOrderCooldownMs=%d # 같은 사람에게 다시 낼 때까지 최소 간격.\n", g_bedOrderCooldownMs);
    fprintf(f, "                      # 짧으면 경로탐색이 계속 초기화돼 출발을 못 한다.\n\n");
    fprintf(f, "probeOrder=%d          # 우클릭 명령 인자와 침대를 기록 (debug=0 에도)\n\n", g_probeOrder ? 1 : 0);

    fprintf(f, "# --- 실패로 확인된 것 (고정, 파일을 고쳐도 안 읽는다) ---\n");
    fprintf(f, "# pauseJobs=%d         잡을 끄면 침대로 가는 경로까지 끊긴다\n", g_pauseJobs ? 1 : 0);
    fprintf(f, "# skipJobs=%d          잡 후보를 막으면 침대 평가도 같이 멈춘다\n", g_skipJobs ? 1 : 0);
    fprintf(f, "# forceGoal=%.2f       목표를 꽂아도 게임이 되돌린다. 왕복만 생긴다\n", g_forceGoal);
    fprintf(f, "# skipSelected=%d      선택한 채 저장하면 그 캐릭터가 영구 제외된다\n", g_skipSelected ? 1 : 0);
    fclose(f);
}

// ---------------------------------------------------------------------------
//  후크 원본 포인터
// ---------------------------------------------------------------------------
typedef float (*ScoreFn)(AI*, const hand*, const Ogre::Vector3*);
typedef void  (*MoveOrderFn)(Character*, Building*, void*, const Ogre::Vector3*);
typedef void  (*AddJobFn)(Character*, int, void*, bool, bool, const Ogre::Vector3*);
typedef void  (*AddOrderFn)(Character*, Building*, int, void*, bool, bool, const Ogre::Vector3*);
typedef float (*FindBedFn)(AI*, const hand*, hand*, bool);

static ScoreFn     origGoToBed   = 0;
static ScoreFn     origGetUp     = 0;
static MoveOrderFn origMoveOrder = 0;
static AddJobFn    origAddJob    = 0;
static AddOrderFn  origAddOrder  = 0;
static FindBedFn   origFindBed   = 0;

// ---------------------------------------------------------------------------
//  캐릭터 이름
//
//  RootObjectBase::displayName 이 Character + 0x18 에 있다
//  (Character : RootObject : RootObjectBase, 둘 다 오프셋 0).
//  켄시의 std::string 은 40바이트다 — VS2010 계열이라 앞에 프록시 포인터가 붙는다.
//    +0x00 프록시   +0x08 SSO버퍼(16) 또는 힙 포인터   +0x18 길이   +0x20 용량
//  우리 컴파일러의 std::string 과 레이아웃이 다르므로 직접 읽는다.
//  틀렸을 때를 대비해 SEH 로 감싸고, 실패하면 포인터를 이름 대신 쓴다.
// ---------------------------------------------------------------------------
// 스쿼드 76인 > 64칸이라 v31 까지는 뒤에 등록되는 12명 이상이 영구 미등록이었다.
static const int  MAXCH = 128;
static Character* g_ids[MAXCH] = { 0 };
static int        g_idCount = 0;
static unsigned   g_lastSeen[MAXCH] = { 0 };  // 마지막으로 보인 틱 (재사용 판단)
// v34: 캐릭터별로 마지막에 찾은 침대와, 마지막으로 명령을 낸 시각.
// findOwnedFreeBed 후크가 침대를 채우고, hookUpdate4 가 그것으로 명령을 낸다.
static Building*        g_bedOf[MAXCH]     = { 0 };
static unsigned __int64 g_lastOrder[MAXCH] = { 0 };
// v35: 침대에 누워 있는가. hookGetUp 이 채우고 자동 명령이 참조한다.
// 침대 관련 상황에서만 갱신되므로 tick 으로 신선도를 함께 본다.
static bool             g_inBed[MAXCH]     = { false };
static unsigned __int64 g_inBedTick[MAXCH] = { 0 };
static int              g_orderCount[MAXCH] = { 0 };   // 이번 부상 동안 몇 번 냈나
static unsigned   g_now = 0;                  // hookUpdate4 마다 증가
static void ResetSlot(int s);                 // 정의는 슬롯 배열들 아래에

// v4 에서 displayName 직접 읽기가 "[? 존]" 처럼 앞이 깨졌다. 오프셋이 틀린 것이다.
// 이름 추측을 반복하는 대신, 캐릭터마다 앞 0x60 바이트를 한 번만 덤프해 둔다.
// 그 줄에서 문자열 위치를 확정한 뒤 다음 판에 이름을 되살린다.
static void DumpBytesOnce(Character* c, const char* id)
{
    char line[192];
    int  n = 0;
    n += sprintf_s(line + n, sizeof(line) - n, "[%s] HEX ", id);
    __try
    {
        const unsigned char* p = (const unsigned char*)c;
        for (int i = 0x10; i < 0x40 && n < 170; ++i)
            n += sprintf_s(line + n, sizeof(line) - n, "%02X", p[i]);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        sprintf_s(line + n, sizeof(line) - n, "(읽기 실패)");
    }
    Log(LC_HEX, "%s", line);
}

// 캐릭터 이름.
//   Character + 0x18 부터 std::string (32바이트, 앞에 프록시 없음)
//     +0x00 SSO 버퍼 16바이트 / 용량>=16 이면 힙 포인터
//     +0x10 길이   +0x18 용량
// HEX 덤프로 확정한 배치다 (미우/삑/꼬부랑 존/Basusu 전부 정상 복원 확인).
// 그래도 버전이 다를 수 있으니 SEH 로 감싸고, 실패하면 C1, C2 … 로 떨어진다.
static const char* CharName(Character* c, char* out, int outLen)
{
    if (!c) { strcpy_s(out, outLen, "널"); return out; }

    int id = 0;
    for (int i = 0; i < g_idCount; ++i)
        if (g_ids[i] == c) { g_lastSeen[i] = g_now; id = i + 1; break; }
    if (!id)
    {
        // 빈 칸이 없으면 가장 오래 안 보인 슬롯을 재사용한다.
        // 세이브를 (게임 재시작 없이) 다시 불러오면 Character 가 전부 새 주소로
        // 만들어지는데, v31 까지는 테이블이 옛 주소로 꽉 찬 채 남아서
        // 그 뒤로는 아무도 등록되지 못했다 (clearBedOrder 등이 통째로 죽음).
        // 산 캐릭터의 슬롯이 잘못 재사용돼도 hookUpdate4 가 매 프레임
        // 부상·명령 플래그를 다시 계산하므로 한 프레임 안에 자가 교정된다.
        int slot = -1;
        if (g_idCount < MAXCH)
            slot = g_idCount++;
        else
        {
            unsigned oldest = 0xFFFFFFFFu;
            for (int i = 0; i < MAXCH; ++i)
                if (g_lastSeen[i] < oldest) { oldest = g_lastSeen[i]; slot = i; }
            ResetSlot(slot);               // 이전 주인의 플래그를 지운다
        }
        g_ids[slot] = c;
        g_lastSeen[slot] = g_now;
        id = slot + 1;
        if (g_dumpHex)
        {
            char tmp[16]; sprintf_s(tmp, sizeof(tmp), "C%d", id);
            DumpBytesOnce(c, tmp);
        }
    }

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

    if (id) sprintf_s(out, outLen, "C%d", id);
    else    sprintf_s(out, outLen, "C@%p", (void*)c);
    return out;
}

// 지금 이 캐릭터가 쓸 수 있는 침대를 찾을 수 있는 상태인지 직접 물어본다.
// justAsking = true 라 상태를 바꾸지 않는다(이름이 그렇게 돼 있다 — 미검증).
// 후크가 아니라 트램펄린(origFindBed)을 호출하므로 재귀하지 않는다.
static float ProbeBed(AI* self, const hand* h)
{
    if (!origFindBed || !self || !h) return -999.0f;   // 후킹 실패 시 호출하지 않는다
    hand out;
    return origFindBed(self, h, &out, true);
}

// 완전 회복 판정.
//
// getOverallHealthRating() 은 쓸 수 없다. 캐릭터 자신의 최대치 대비 비율이
// 아니라 고정 기준 대비 값으로 보인다 — 실측: 부위 최대 75 인 삑은
// 전 부위 만땅인데도 평점이 0.74 에 머물러 영원히 회복 판정을 못 받았다.
// 반면 최대 100 인 스노우 화이트는 98 에서 0.99 를 넘겨 일어났다.
//
// 그래서 부위별로 현재/최대 를 직접 비교한다. 최대치가 낮은 캐릭터도
// 자기 기준으로 판정되므로 종족·부위 편차에 영향받지 않는다.
//
// HealthPartStatus::flesh(현재 체력)는 익스포트가 없어 +0x40 오프셋으로 읽는다.
// 값이 이상하면(음수이거나 최대치를 넘으면) 오프셋이 틀린 것으로 보고
// 게임의 원래 판정으로 물러난다.
static float WorstPartRatio(MedicalSystem* med, bool* ok)
{
    *ok = false;
    if (!med) return 1.0f;
    int n = med->getPartCount();
    if (n <= 0 || n > 64) return 1.0f;

    float worst = 1.0f;
    for (int i = 0; i < n; ++i)
    {
        MedicalSystem::HealthPartStatus* part = med->getPart((unsigned __int64)i);
        if (!part) continue;
        // derivedFleshHealthPercent (+0x60). 화면에 뜨는 퍼센트 그 자체다.
        // 실측 대조: 머리 화면 9 -> 0.09, 복부 58 -> 0.59, 기상 후 머리 51 -> 0.51.
        // 예전에 쓰던 flesh/maxHealth 는 전혀 다른 값이었다
        // (화면 9%일 때 79/100 이었고, 도로롱은 217/200 처럼 최대치를 넘겼다).
        float r = *(const float*)((const unsigned char*)part + 0x60);
        if (r < -0.01f || r > 1.05f) return 1.0f;      // 오프셋 의심
        if (r < worst) worst = r;
    }
    *ok = true;
    return worst;
}

static bool FullyHealed(MedicalSystem* med)
{
    if (!med) return true;
    bool ok = false;
    float worst = WorstPartRatio(med, &ok);
    if (!ok) return med->isFullyRested();          // 부위를 못 읽으면 원래 판정
    return worst >= (g_healThreshold - 0.0005f);
}

// 부위 목록을 캐릭터당 한 번 통째로 남긴다.
//
// 실측 제보: 스노우 화이트는 머리 50, 다른 부위 80 인데도 완전 회복으로
// 판정돼 침대에서 나갔다. 부위를 도는 방식이 그 부위들을 못 보고 있다는 뜻이다.
// getPartCount 가 실제 개수와 다르거나, index 기반 getPart 가 다른 것을
// 가리키거나, flesh 오프셋이 부위 종류마다 다를 수 있다.
// 추측하지 말고 실제 값을 그대로 찍어 확인한다.
static void DumpPartsTagged(MedicalSystem* med, const char* who, const char* tag)
{
    if (!med) return;
    int n = med->getPartCount();
    bool  ok = false;
    float worst = WorstPartRatio(med, &ok);
    char line[512];
    int  w = sprintf_s(line, sizeof(line), "[%s] %s 부위 %d개 최저=%.4f 읽기%s 판정=%s | ",
                       who, tag, n, worst, ok ? "OK" : "실패",
                       FullyHealed(med) ? "회복완료" : "부상");
    static const char* PT[] = { "TORSO", "LEG", "ARM", "HEAD" };
    static const char* SD[] = { "-", "L", "R", "B" };

    for (int i = 0; i < n && i < 24 && w < 400; ++i)
    {
        MedicalSystem::HealthPartStatus* part = med->getPart((unsigned __int64)i);
        if (!part) { w += sprintf_s(line + w, sizeof(line) - w, "%d:널 ", i); continue; }
        const unsigned char* q = (const unsigned char*)part;

        // 부위가 자기 정체를 들고 있다. 인덱스 순서를 추측할 필요가 없다.
        //   +0x08 whatAmI (TORSO=0 LEG=1 ARM=2 HEAD=3)
        //   +0x20 side    (없음=0 좌=1 우=2 양쪽=3)
        // FCS 의 LOCATIONAL_DAMAGE "body part type" 과 값이 일치한다.
        int   ty   = *(const int*)(q + 0x08);
        int   side = *(const int*)(q + 0x20);
        float flesh = *(const float*)(q + 0x40);
        float maxv  = *(const float*)(q + 0x54);
        float derived = *(const float*)(q + 0x60);

        const char* tn = (ty >= 0 && ty <= 3) ? PT[ty] : "?";
        const char* sn = (side >= 0 && side <= 3) ? SD[side] : "?";
        w += sprintf_s(line + w, sizeof(line) - w, "%d:%s%s %.0f/%.0f P%.2f  ",
                       i, tn, sn, flesh, maxv, derived);
    }
    Log(LC_PARTS, "%s", line);
}

static void DumpPartsOnce(MedicalSystem* med, const char* who, int slot)
{
    static bool done[MAXCH] = { false };
    if (!g_dumpParts || !med || slot < 0 || slot >= MAXCH || done[slot]) return;
    done[slot] = true;
    DumpPartsTagged(med, who, "최초");
}



// 아군인지 (NPC 는 손대지도, 기록하지도 않는다)
static bool IsPlayerChar(Character* c)
{
    return c && !c->isDead() && c->isPlayerCharacter();
}
static bool IsPlayerAI(AI* a)
{
    return a && IsPlayerChar(a->getCharacter());
}

static const char* TASK_NAMES[] = {
    "NULL_TASK","MOVE_ON_FREE_WILL","BUILD","PICKUP","MELEE_ATTACK","FOCUSED_MELEE_ATTACK",
    "EQUIP_WEAPON","UNEQUIP_WEAPON","FIND_WEAPON","CHOOSE_ENEMY_AND_ATTACK",
    "CHOOSE_ATTACKER_OF_ALLY","ATTACK_CHARACTERS_ATTACKER","PLAYER_TALK_TO","ATTACK_ATTACKERS_OF",
    "IDLE","PROTECT_ALLIES","ATTACK_ENEMIES","PROTECTION","RAID_TOWN","GO_HOMEBUILDING",
    "STAND_AT_SHOPKEEPER_NODE","ATTACK_ENEMIES_AND_NEUTRALS","PATROL","ATTACK_TOWN","WANDERER",
    "FIRST_AID_ORDER","LOOT_TARGET","CROUCH","STAND_UP","MOVE_CUS_ORDERED","HOLD_POSITION",
    "STAY_CLOSE_TO_TARGET","SELF_PRESERVATION","QUELL_AGGRESSION","ATTACK_TROUBLE_MAKERS",
    "RUN_AWAY","PATROL_TOWN","WANDER_TOWN","STAND_AT_GUARD_NODE_HOMEBUILDING_IN_OUT",
    "WANDERING_TRADER","GET_NEAR_TO","ATTACK_ENEMIES_OF_MY_SLAVEMASTER","NOT_BE_UNARMED",
    "STAY_IN_HOME","FOLLOW_PLAYER_ORDER","BODYGUARD","CHASE","STAND_AT_GENERAL_NODE",
    "STAND_AT_DEFENSIVE_NODE","STAND_AT_BUILDING_GUARD_NODE","STAND_AT_BUILDING_DEFENSIVE_NODE",
    "STAND_AT_NODE","GET_UP_STAY_DOWN_THOUGH","TRAVEL_TO_TARGET_TOWN","REST","RECRUIT_AT_JOBCENTER",
    "SWITCH_FOLLOW_ME_MODE_ON","JOB_REPAIR_ROBOT","JOB_MEDIC","GET_READY_FOR_ACTION",
    "FIRST_AID_ROBOT","UNPROVOKED_FOCUSED_MELEE_ATTACK","STAND_STILL","SQUAD_WAIT_FOR_ME",
    "MAKE_TARGET_STAND_STILL","GET_UP_STAND_UP","FORCE_GET_UP","MOVE_ON_FREE_WILL_FAST",
    "LIFT_PERSON","PUT_DOWN_OBJECT","PUT_DOWN_CHARACTER_IN_BED","ADD_MATERIALS_TO_BUILDING",
    "OPEN_DOOR","CLOSE_DOOR","OPEN_DOOR_HERE","CLOSE_DOOR_HERE","PICK_LOCK","LOCK_DOOR",
    "UNLOCK_DOOR","LOCK_DOOR_HERE","UNLOCK_DOOR_HERE","BASH_DOOR","MOVE_TO_BUILDING_DOOR",
    "MOVE_TO_CURRENT_LOCATION_BUILDING_DOOR","OPEN_DOOR_FOR_CURRENT_LOCATION",
    "OPEN_DOOR_FOR_DESTINATION","OPEN_UP_SHOP_DOORS","OPERATE_MACHINERY","DELIVER_RESOURCES",
    "JOB_KEEP_EVERYTHING_RUNNING","UNJAM_ALL_MACHINES","UNJAM_MACHINE","COLLECT_OUTPUT_RESOURCE",
    "FILL_MACHINE","WANT_TO_FILL_MACHINE","REPAIR","DISMANTLE","USE_TRAINING_DUMMY","USE_BED",
    "PUT_SOMEONE_IN_BED","GET_PUT_IN_BED","DEFEAT_SQUAD","SEEK_AND_TALK_AND_SEND_SIGNAL",
    "MAKE_ANNOUNCEMENT","ALWAYS_IMPOSSIBLE_TASK","FIND_AND_RESCUE","FIND_BED_AND_PUT_IN","USE_CAGE",
    "PUT_IN_CAGE","KNOCKOUT_PRISONER","RELEASE_PRISONER","BREAKOUT_PRISONER","FIND_CAGE_AND_PUT_IN",
    "EMPTY_MACHINE_OUTPUTS","GET_RID_OF_RESOURCES_IN_MY_INVENTORY","FIND_SOME_BUILDING_MATERIALS",
    "GET_OUT_OF_BED","FIND_A_SHOP","SHOPPING","BUY_SHIT","MOVE_INSIDE_BUILDING",
    "MOVE_TO_FORTIFICATION_GATE","OPEN_FORTIFICATION_GATE","BASH_GATE","OPERATE_STORAGE",
    "JOB_BUILDER","TALKTO_NEAREST_PLAYER_CHARACTER","RUN_AWAY_HOMETOWN","RETREAT_HOMETOWN",
    "MAKE_ANNOUNCEMENT_FAST","TRAVEL_TO_TARGET_TOWN_FAST","LOOT_FOOD_AND_STUFF","FIND_AND_KIDNAP",
    "GET_OUT_OF_CAGE_LEGIT","KILL_CAGE_OCCUPANT","KILL_A_RANDOM_CAGE_OCCUPANT",
    "FEED_CORPSE_INTO_MACHINE","DEAD_GUYS_GO_IN_THE_POT","FIND_A_DEAD_GUY",
    "EAT_A_RANDOM_CAGE_OCCUPANT","UNLOCK_DOOR_PLAYER_ORDER","FOLLOW_SQUADLEADER",
    "FIND_AND_RESCUE_LEADER","PROTECT_OWN_SQUAD","TERRITORIAL_AGGRESSION_BUT_DONT_LEAVE_HOME",
    "GET_RE_EQUIPPED","USE_TURRET","STUMBLE_TASK_FORCED","FIND_AND_RESCUE_IF_THERES_BEDS",
    "MAN_A_TURRET","PROSPECTING","EMPTYING_MACHINE","OPERATE_AUTOMATIC_MACHINERY",
    "GO_HOME_AND_GO_TO_BED","GO_TO_THE_BAR_AND_DRINK","LOCK_ALL_MY_DOORS","ENTER_BUILDING",
    "STAND_AT_GUARD_NODE_HOMETOWN_OUTSIDE","SHOO_STRANGERS_OUT_OF_MY_BUILDING",
    "SEND_DIALOGUE_SIGNAL","SEND_DIALOGUE_SIGNAL_REPEAT","SEND_DIALOGUE_SIGNAL_WITHOUT_MOVING",
    "LOCK_DOOR_FROM_INSIDE","MOVE_TO_BUILDING_DOOR_INSIDEPOS","FOLLOW_WHILE_TALKING","TOWN_STALKER",
    "CHAIN_TARGET","CAPTURE_NEW_SLAVES","CARRY_WOUNDED_SLAVES",
    "PUT_DOWN_CARRIED_DUDE_IF_THEY_CAN_WALK","LIFT_OBJECT_BUT_HEAL_FIRST","FOLLOW_SLAVEMASTER",
    "SLAVE_GET_IN_MY_MASTERS_CAGE","GATHER_SLAVES_FROM_CAGES","GET_SLAVE","SLEEP_ON_FLOOR",
    "HUNTING_BLOODSMELL","LOOT_THE_DEAD","LOOT_TO_REPLACE_MISSING_WEAPON","HUNT_MY_THIEF",
    "MAN_THE_GATE","STRIP_TARGETS_WEAPONS","PROCESS_AND_STRIP_NEW_SLAVE","SLAVE_WATCHING",
    "PUT_LOOT_IN_STORAGE","CUT_SHACKLES","BRUTE_FORCE_SHACKLES","_SLAVE_OBEDIENCE",
    "WORK_THE_SLAVES","AUTO_LABOURING_MINES","AUTO_LABOURING_MINES_PRETEND","GO_TO_NEAREST_HQ",
    "GO_TO_SOMEWHERE_FOR_DELIVERING_SLAVES","CAPTURE_ESCAPING_SLAVES","GIVE_ALL_MY_SLAVES_TO",
    "LOCK_ALL_THE_CAGES","BEAT_CAGE_OCCUPANT","LOCK_ALL_MY_DOORS_FROM_OUTSIDE",
    "LOCK_DOOR_FROM_OUTSIDE","MOVE_TO_BUILDING_DOOR_OUTSIDEPOS","LEAVE_BUILDING",
    "PICK_LOCK_ON_SHACKLES","TOTAL_ESCAPE","ARREST_TARGET","HUNT_BOUNTIES",
    "ARREST_TARGETS_CARRIED_PERSON","FIND_CAGE_AND_PUT_IN_IF_BOUNTY","GET_OUT_OF_CAGE_ESCAPE",
    "GET_OUT_OF_BED_IF_ITS_EMERGENCY","INVESTIGATE_ALARMS","INVESTIGATE_ALARMS_ALLIES_ONLY",
    "POLICE_FREE_PRISONERS_WHEN_DONE","LOOT_STOLEN_GOODS","LIFT_PERSON_SNATCHING_ALLOWED",
    "RELAX_IN_TOWN_PACKAGE","TRAVEL_TO_TARGET_PACKAGE","RUN_AROUND_TOWN_LOOKING_FOR_PEOPLE",
    "GATHER_SLAVES_FROM_CAGES_IF_ITS_AN_EXPORT_TOWN","GIVE_ALL_MY_SLAVES_TO_IF_ITS_AN_IMPORT_TOWN",
    "TAKE_OFF_MY_SHACKLES","EAT_TARGET_ALIVE","PRETEND_TO_OPERATE_MACHINERY",
    "MAN_A_TURRET_ON_BUILDING","PICKUP_INTRUDERS_BUILDING","TAKE_INTRUDER_OUTSIDE",
    "LIFT_PERSON_PLAYER_ORDER","BASH_DOOR_PLAYER_ORDER","MELEE_ATTACK_ANIMAL","STEALTH_KNOCKOUT",
    "STEALTH_KILL","EAT_A_RANDOM_DEAD_BODY","EAT_CROPS","FIND_CROPS_TO_EAT","EAT_A_RANDOM_KO_BODY",
    "MAN_A_TURRET_PLAYER_JOB","SHOOT_AT_TARGET","WORSHIP_TARGET","FOGMAN_WORSHIP_VICTIM",
    "LOOT_ANIMALS_JOB","GO_HOME_AND_GO_TO_BED_SECURE","LIFT_PERSON_SNATCHING_ALLOWED_IN_TOWN_ONLY",
    "LOOT_RESOURCE_ITEMS_WE_HAVE_STORAGE_FOR","DITCH_ALL_RESOURCES","AQUIRE_FOOD_AT_HOMEBASE",
    "GRAB_ONE_FOOD","GATHER_PRISONERS_FROM_CAGES_IF_FEMALE_OR_BEAST","KIDNAP_ORDER",
    "COLLECT_OUTPUT_RESOURCE_BUILD_MATS","DEFEAT_SQUAD_LIMIT_CHASE_RANGE","SPLINT_ORDER",
    "SPLINT_JOB","ESCAPE_KIDNAP","ESCAPE_KIDNAP_STR","FOLLOW_URGENT_ESCAPE",
    "FINAL_KIDNAPPER_CAGE_JOB","SIT_ON_THRONE","GET_OUT_OF_CAGE_OPPORTUNISTIC",
    "GET_OUT_OF_BED_ONCE_HEALED","USE_BED_ORDER","EAT_FOOD_ON_GROUND","NEW_SLAVE_PROCESSING",
    "SLEEP_ON_FLOOR_FAKE_AMBUSH","RANGED_ATTACK","RANGED_ATTACK_FOCUSED","EQUIP_CROSSBOW",
    "UNEQUIP_CROSSBOW","RANGED_ATTACK_FOCUSED_UNPROVOKED","MOVE_IN_BOW_RANGE",
    "STAND_AT_GUARD_NODE_HOMEBUILDING_INDOORS_ONLY","HEAL_MY_LEGS",
    "ASSAULT_FORTIFICATIONS_PREFER_GATES","ASSAULT_FORTIFICATIONS_PREFER_WALLS","SMASH_BUILDING",
    "PICKUP_INTRUDERS_TOWN","TAKE_INTRUDER_OUTSIDE_TOWN","SIT_AROUND","LIBERATE_ALL_THE_PRISONERS",
    "ANIMAL_FETCH_A_LIMB","PLAY_BECAUSE_I_HAVE_A_LIMB_IN_MOUTH","CHASE_ALLY_DOGS_WITH_MOUTH_LIMBS",
    "RUN_AWAY_FORCED","FIND_CAGE_AND_PUT_DEADGUY_IN","EAT_A_RANDOM_CAGE_OCCUPANT_MEASURED_RATE",
    "SHOO_STRANGERS_OUT_OF_MY_BUILDING_IF_PRIVATE","LOOT_CONTAINER","CUT_LOCK","BRUTE_FORCE_LOCK",
    "BASH_DOOR_HERE","PROTECT_ALLIES_STAY_IN_TOWN","STAY_CLOSE_TO_TARGET_ANIMAL",
    "BASH_GATE_PLAYER_ORDER",
};
static const int TASK_NAMES_COUNT = 291;

static const char* TaskName(int t)
{
    return (t >= 0 && t < TASK_NAMES_COUNT) ? TASK_NAMES[t] : "?";
}

// ---------------------------------------------------------------------------
//  현재 목표 덤프
//
//  멤버 오프셋은 KenshiLib 헤더(AITaskSystem.h) 기준이다.
//    OrdersReceiver::currentGoalScore    0x208  float
//    OrdersReceiver::currentGoalPriority 0x20C  enum taskPriority (0~4)
//    AITaskSytem::autoSleepTask          0x290  Tasker*
//    AITaskSytem::autoGetupTask          0x298  Tasker*
//  헤더 버전이 게임과 어긋나면 값이 엉뚱하게 나온다.
//  priority 가 0~4 를 벗어나면 오프셋이 틀린 것이므로 그렇게 표시한다.
// ---------------------------------------------------------------------------
static const char* PRI_NAMES[] = { "JUST_ACTION", "FLUFF", "NON_URGENT", "URGENT", "OBEDIENCE" };

// ---------------------------------------------------------------------------
//  부상 중 잡 일시정지
//
//  잡은 URGENT 버킷에서 뽑히고 침대 목표는 NON_URGENT 버킷이라
//  점수로는 절대 못 이긴다 (버킷이 점수보다 우선 — v7 로그로 실측).
//  그래서 다친 동안 OrdersReceiver::doJobsEnabled(+0x35, bool) 를 꺼 둔다.
//  이 값은 게임 UI 의 "잡" 토글과 같은 스위치라 화면에서도 꺼진 게 보인다.
//  내가 끈 캐릭터만 g_jobsPaused 로 기억했다가 다 나으면 되돌린다.
//  원래 꺼져 있던(수동으로 잡을 끈) 캐릭터는 건드리지 않는다.
//
//  [주의] 게임이 이 상태로 저장되면 스위치도 꺼진 채 저장된다.
//         복구 전에 플러그인을 빼면 그 캐릭터의 잡은 UI 에서 손으로 켜야 한다.
// ---------------------------------------------------------------------------
static bool g_jobsPaused[MAXCH] = { false };
static bool g_selected[MAXCH]   = { false };   // 사용자가 선택 중인 캐릭터

// AITaskSytem* -> 슬롯 대응표. choosePermaJob 후크는 AITaskSytem* 만 받는데
// 거기서 Character 로 가는 익스포트 함수가 없다. 오프셋을 추측하는 대신,
// AI 쪽 후크에서 본 값을 그대로 기록해 두고 조회한다.
static void* g_tsOf[MAXCH]    = { 0 };
static bool  g_injured[MAXCH] = { false };
static bool  g_bedOrderWhileHurt[MAXCH] = { false };  // 다친 채로 걸린 취침 명령인가

static int CharSlot(Character* c)
{
    for (int i = 0; i < g_idCount; ++i)
        if (g_ids[i] == c) return i;
    return -1;
}

static void PauseJobs(AI* self, Character* c, const char* who)
{
    AITaskSytem* ts = self->getTaskSystem();
    int slot = CharSlot(c);
    if (!ts || slot < 0) return;
    if (!ts->isJobsEnabled())
    {
        // 이미 꺼져 있다. 내가 끈 게 아니면(플래그 없음) 관여하지 않는다.
        return;
    }

    // 켜져 있고 부상 중이다 — 끈다. 부상 중에 다시 켜져도 매번 다시 끈다.
    *((unsigned char*)ts + 0x35) = 0;              // doJobsEnabled = false
    ts->_NV_setNeedGOAP();
    if (!g_jobsPaused[slot])
    {
        g_jobsPaused[slot] = true;
        Log(LC_JOBSW, "[%s] 잡 일시정지 (부상)", who);
    }
    else
    {
        // 내가 꺼둔 게 밖에서 다시 켜졌던 것이다. 같은 줄은 접힌다.
        Log(LC_JOBSW, "[%s] 잡 재정지 (부상 중 재활성 감지)", who);
    }
}

static void ResumeJobs(AI* self, Character* c, const char* who)
{
    AITaskSytem* ts = self->getTaskSystem();
    int slot = CharSlot(c);
    if (!ts || slot < 0 || !g_jobsPaused[slot]) return;

    *((unsigned char*)ts + 0x35) = 1;              // doJobsEnabled = true
    g_jobsPaused[slot] = false;
    ts->_NV_setNeedGOAP();
    Log(LC_JOBSW, "[%s] 잡 재개 (회복 완료)", who);
}

static void DumpGoal(AI* self, const char* who, float bedOrig)
{
    AITaskSytem* ts = self->getTaskSystem();
    if (!ts) { Log(LC_GOAL, "[%s] 목표덤프  getTaskSystem() 이 널", who); return; }

    const unsigned char* p = (const unsigned char*)ts;
    float goalScore = *(const float*)(p + 0x208);
    int   goalPri   = *(const int*)(p + 0x20C);

    const TaskMatch& g = ts->getCurrentGoal();
    int  key  = g.getTaskData() ? (int)g.key() : -1;

    // 이름이 안 나와도 이걸로 판정된다. 1 이면 침대 목표가 이긴 것.
    Tasker* sleepTask = *(Tasker* const*)((const unsigned char*)ts + 0x290);
    int isBedGoal = (sleepTask && g.sameAs(sleepTask)) ? 1 : 0;

    Log(LC_GOAL,
        "[%s] 목표=%s(%d) 점수=%.2f 우선순위=%s(%d) | 침대목표=%d 침대점수=%.3f | 명령=%d/%d 잡=%d/%d",
        who, key >= 0 ? TaskName(key) : "(없음)", key, goalScore,
        (goalPri >= 0 && goalPri <= 4) ? PRI_NAMES[goalPri] : "오프셋틀림",
        goalPri, isBedGoal, bedOrig,
        ts->hasPlayerOrders() ? 1 : 0, ts->hasOrders() ? 1 : 0,
        ts->isJobsEnabled() ? 1 : 0, ts->getPermajobCount());
}

// 보정해도 되는 상황인지
static bool Eligible(AI* self)
{
    if (!self) return false;
    Character* c = self->getCharacter();
    if (!c) return false;
    if (c->isDead())             return false;  // 시체
    if (c->isBeingCarried())     return false;  // 실려가는 중 — 옮기는 쪽을 방해하지 않는다
    if (!c->isPlayerCharacter()) return false;  // 아군만
    if (g_skipCombat)
    {
        // 인자 의미가 확정되지 않아 두 조합 모두 확인한다.
        // 하나라도 전투로 보이면 보정하지 않는다 (안전한 쪽으로).
        if (c->isInCombatMode(true, true))   return false;
        if (c->isInCombatMode(false, false)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  점수 후크
// ---------------------------------------------------------------------------
static float hookGoToBed(AI* self, const hand* h, const Ogre::Vector3* v)
{
    if (!origGoToBed) return 0.0f;
    LogThreadOnce("GoToBed");
    float orig = origGoToBed(self, h, v);
    if (!IsPlayerAI(self)) return orig;

    Character* c = self->getCharacter();

    // 부상 판정은 의료 기준(전 부위 100%) 하나만 쓴다.
    MedicalSystem* med = c ? c->getMedical() : 0;
    bool medRested = FullyHealed(med);

    // 이름은 로그에만 쓴다. 이 함수는 초당 수백 번 불리고 Log() 는
    // debug=0 이면 어차피 아무것도 쓰지 않는다. v31 까지는 여기서
    // 매번 CharName(64칸 선형 탐색 + 문자열 읽기)을 돌렸다.
    // 슬롯 등록은 hookUpdate4 의 CharName 이 맡고 있어 생략해도 된다.
    char who[64]; who[0] = 0;
    if (g_debug) CharName(c, who, sizeof(who));

    if (medRested)
    {
        // 다 나았다. 내가 잡을 껐던 캐릭터면 되돌린다.
        if (g_pauseJobs) ResumeJobs(self, c, who);
        return orig;
    }

    // 선택 중 예외는 기본 꺼짐. 선택 상태와 무관하게 회복하러 가는 것이 목표다.
    if (g_skipSelected)
    {
        int slot = CharSlot(c);
        if (slot >= 0 && g_selected[slot])
        {
            if (g_pauseJobs) ResumeJobs(self, c, who);
            return orig;
        }
    }

    bool elig = Eligible(self);

    if (g_debug)
    {
        // aiRested(AI 기준, 사지 8~90% 면 참)와 평점은 진단에만 쓴다.
        // v31 까지는 debug=0 에도 매번 계산했다 — 게임 함수 호출 2회 낭비.
        bool  aiRested = (h && v) ? self->isFullyRested(*h, *v) : true;
        float health   = med ? med->getOverallHealthRating() : -1.0f;
        // 인자 평가 순서는 정해져 있지 않다. MSVC 는 오른쪽부터 처리해서
        // pok 이 함수 호출 전에 읽혔고, 늘 "실패"로 찍혔다. 먼저 계산해 둔다.
        bool  pok = false;
        float worst = WorstPartRatio(med, &pok);
        Log(LC_BED, "[%s] GoToBed orig=%.3f  aiRested=%d 최저부위=%.4f(읽기%s) 평점=%.4f eligible=%d",
            who, orig, aiRested ? 1 : 0, worst, pok ? "OK" : "실패", health, elig ? 1 : 0);
        DumpGoal(self, who, orig);
    }

    if (!elig) return orig;

    if (g_pauseJobs) PauseJobs(self, c, who);   // 켜지 말 것

    // 배율을 먼저 적용하고, 그래도 하한에 못 미치면 하한으로 끌어올린다.
    // 잡 점수가 12~14 이므로 하한이 그보다 높아야 같은 버킷에서 이긴다.
    // (버킷 자체는 FCS 에서 Go home go to bed 를 URGENT 로 올려야 한다)
    float result = orig * g_bedMult;

    if (g_forceBed > 0.0f && result < g_forceBed)
    {
        float probe = ProbeBed(self, h);        // 빈 침대가 있을 때만 올린다
        if (g_debug) Log(LC_PROBE, "[%s] 침대탐색  결과=%.3f", who, probe);
        if (probe > 0.0f)
        {
            Log(LC_BED, "[%s] 침대점수  %.3f -> %.3f (하한)", who, orig, g_forceBed);
            return g_forceBed;
        }
    }

    if (result != orig)
        Log(LC_BED, "[%s] 침대점수  %.3f -> %.3f (배율)", who, orig, result);
    return result;
}

static float hookGetUp(AI* self, const hand* h, const Ogre::Vector3* v)
{
    if (!origGetUp) return 0.0f;
    float orig = origGetUp(self, h, v);
    if (!IsPlayerAI(self)) return orig;

    bool inBed = (h && v) ? self->isInBed(*h, *v) : false;

    // v35: 침대에 누워 있는지를 슬롯에 남긴다.
    // 자동 명령이 "이미 자고 있는 사람"에게 또 나가는 것을 막는 데 쓴다.
    // 이 후크는 침대에 관련된 상황에서만 불리므로, 여기서 얻은 값이
    // update4 가 알 수 있는 가장 신선한 정보다.
    {
        Character* bc = self->getCharacter();
        int bs = CharSlot(bc);
        if (bs >= 0) { g_inBed[bs] = inBed; g_inBedTick[bs] = GetTickCount64(); }
    }

    // 침대에 있지도 않고 점수도 0 이면 아무 정보가 없다. 기록하지 않는다.
    if (!inBed && orig <= 0.0f) return orig;

    Character* mc = self->getCharacter();
    char who[64]; who[0] = 0;
    if (g_debug) CharName(mc, who, sizeof(who));   // 이름은 로그에만 쓴다
    bool elig   = Eligible(self);
    MedicalSystem* med2 = mc ? mc->getMedical() : 0;
    bool medRested = FullyHealed(med2);

    // rested=0 인데 점수가 붙는다면, 다 낫기 전에 일어나려 한다는 뜻이다.
    // 그 상태에서 getUpMult 를 크게 잡으면 침대에서 튕겨 나온다.
    if (g_debug)
    {
        // aiRested 는 진단에만 쓴다. v31 까지는 debug=0 에도 매번 계산했다.
        bool rested = (h && v) ? self->isFullyRested(*h, *v) : true;
        Log(LC_GETUP, "[%s] GetUpHealed orig=%.3f inBed=%d medRested=%d aiRested=%d eligible=%d",
            who, orig, inBed ? 1 : 0, medRested ? 1 : 0, rested ? 1 : 0, elig ? 1 : 0);
    }

    if (g_pauseJobs && medRested) ResumeJobs(self, self->getCharacter(), who);

    // 완전 회복 전에 일어나는 것을 막는다.
    //
    // scoreGetOutOfBedOnceFullyHealed 는 이름과 달리 100% 이전에도 점수를 낸다.
    // 실측: medRested=0 인데 orig=2.000 (게임은 AI 기준 8~90% 면 "다 쉬었다"로 본다).
    // 그래서 사지가 덜 나은 채로 침대에서 나온다.
    // 침대로 보내는 쪽만 의료 기준으로 바꾸고 나오는 쪽은 그대로 뒀던 탓이다.
    //
    // 갇힐 걱정은 없다. 비상 기상은 별도 태스크이고
    // (get out of bed when in trouble, TaskType 208) 바닐라에서 이미 URGENT 다.
    // 습격이 오면 그쪽으로 일어난다.
    if (g_stayInBed && inBed && !medRested && elig && orig > 0.0f)
    {
        if (g_debug)
        {
            bool  pok2 = false;
            float worst2 = WorstPartRatio(med2, &pok2);
            Log(LC_GETUP, "[%s] 아직 회복 중 (최저부위 %.4f / 기준 %.4f) — 기상 점수 %.3f -> 0",
                who, worst2, g_healThreshold, orig);
        }
        return 0.0f;
    }

    // 침대에 있는데 통과시킨다면 완전 회복으로 판정했다는 뜻이다.
    // 그 순간의 부위 상태를 남긴다 (캐릭터당 한 번).
    if (inBed && medRested && orig > 0.0f)
    {
        static bool logged[MAXCH] = { false };
        int sl = CharSlot(mc);
        if (sl >= 0 && sl < MAXCH && !logged[sl])
        {
            logged[sl] = true;
            DumpPartsTagged(med2, who, "기상허용시점");
        }
    }

    if (orig <= 0.0f || !elig) return orig;

    float boosted = orig * g_getUpMult;
    // v31 까지 조건이 !g_debug 로 뒤집혀 있었다. Log() 가 debug=0 이면
    // 어차피 안 쓰므로 어느 쪽이든 안 찍히는 죽은 코드였다 — 바로잡는다.
    if (g_debug) Log(LC_GETUP, "[%s] GetUpHealed  %.3f -> %.3f", who, orig, boosted);
    return boosted;
}

// ---------------------------------------------------------------------------
//  관찰용 후크 (동작은 바꾸지 않는다)
//  침대 탐색이 호출되지 않거나 결과가 나쁘면 점수를 아무리 올려도 소용없다.
//  수동 명령이 addOrder 를 거치는지 addJob 을 거치는지도 여기서 갈린다.
// ---------------------------------------------------------------------------
static void hookMoveOrder(Character* self, Building* b, void* root, const Ogre::Vector3* pos)
{
    if (!origMoveOrder) return;
    if (g_debug && IsPlayerChar(self))
    {
        const float* v = (const float*)pos;
        char who[64]; CharName(self, who, sizeof(who));
        Log(LC_MOVE, "[%s] 우클릭명령  building=%p  root=%p  pos=(%.1f, %.1f, %.1f)",
            who, (void*)b, root, v ? v[0] : 0.0f, v ? v[1] : 0.0f, v ? v[2] : 0.0f);
    }
    origMoveOrder(self, b, root, pos);
}

static void hookAddJob(Character* self, int task, void* root, bool f1, bool f2,
                       const Ogre::Vector3* pos)
{
    if (!origAddJob) return;
    if (g_debug && IsPlayerChar(self))
    {
        char who[64]; CharName(self, who, sizeof(who));
        Log(LC_JOB, "[%s] addJob  %s(%d)  flags=%d,%d", who, TaskName(task), task, f1 ? 1 : 0, f2 ? 1 : 0);
    }
    origAddJob(self, task, root, f1, f2, pos);
}

// 플레이어가 침대에 우클릭했을 때 찍히는 TaskType 값이 곧 "침대 사용" 코드다.
// 2안(직접 명령)으로 갈 때 이 값이 반드시 필요하다.
//
// v33: 전 인자를 찍는다. v32 는 root 와 pos 를 빠뜨렸는데 2안에서
// 우리가 채워야 할 것이 정확히 그 둘이다. b 와 root 의 vtable 을 함께
// 남겨 "같은 객체를 두 번 넘기는가, 다른 것인가"를 가른다.
static void hookAddOrder(Character* self, Building* b, int task, void* root,
                         bool f1, bool f2, const Ogre::Vector3* pos)
{
    if (!origAddOrder) return;
    LogThreadOnce("addOrder");   // 수동 명령 경로 — UI 스레드일 가능성을 여기서 가른다

    if (g_probeOrder && g_probeLines < 60 && IsPlayerChar(self))
    {
        char who[64]; CharName(self, who, sizeof(who));
        int slot = CharSlot(self);          // 동명이인 구분용 (#N)
        const void* vtB = 0; const void* vtR = 0;
        float px = 0, py = 0, pz = 0; int hasPos = 0;
        __try
        {
            if (b)    vtB = *(const void* const*)b;
            if (root) vtR = *(const void* const*)root;
            // v34 부터 Ogre::Vector3 이 완전 정의라 pos->x 로도 읽을 수 있지만,
            // 이 자리는 SEH 안이고 널일 수 있어 원시 읽기를 유지한다.
            if (pos)
            {
                const float* v = (const float*)pos;
                px = v[0]; py = v[1]; pz = v[2]; hasPos = 1;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }

        ++g_probeLines;
        Log(LC_PROBE, "[명령] %s#%d  %s(%d)  building=%p(vt=%p)  root=%p(vt=%p)  %s  f1=%d f2=%d  pos=%s(%.1f,%.1f,%.1f)",
            who, slot, TaskName(task), task,
            (void*)b, vtB, root, vtR,
            ((void*)b == root) ? "b==root" : "b!=root",
            f1 ? 1 : 0, f2 ? 1 : 0,
            hasPos ? "있음" : "널", px, py, pz);
    }

    origAddOrder(self, b, task, root, f1, f2, pos);
}

static float hookFindBed(AI* self, const hand* h, hand* out, bool b)
{
    if (!origFindBed) return 0.0f;
    float r = origFindBed(self, h, out, b);

    // v33: out(hand)에서 침대 건물을 꺼내본다.
    // 우클릭 때 addOrder 로 넘어간 building 과 같은 포인터면
    // 2안에서 "어느 침대를 지정할지"가 풀린다.
    // 결과가 0 이면 침대를 못 찾은 것이므로 볼 필요가 없다.
    if (g_probeOrder && g_probeLines < 60 && r > 0.0f && out && IsPlayerAI(self))
    {
        Character* c = self->getCharacter();
        char who[64]; CharName(c, who, sizeof(who));
        Building* bed = 0;
        const void* vt = 0;
        __try
        {
            bed = out->getBuilding();
            if (bed) vt = *(const void* const*)bed;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { bed = 0; }

        // 같은 침대를 계속 찍지 않도록 캐릭터별 마지막 값과 다를 때만 남긴다.
        int s = CharSlot(c);
        static Building* lastBed[MAXCH] = { 0 };
        if (s < 0 || lastBed[s] != bed)
        {
            if (s >= 0) lastBed[s] = bed;
            ++g_probeLines;
            Log(LC_PROBE, "[침대] %s#%d  결과=%.3f  justAsking=%d  침대=%p(vt=%p)",
                who, s, r, b ? 1 : 0, (void*)bed, vt);
        }
    }

    // v34: 찾은 침대를 기억해 둔다. hookUpdate4 가 명령을 낼 때 쓴다.
    // 이 후크는 게임이 부를 때만 도니, 여기서 얻은 침대는 그 시점에 살아 있다.
    if (g_autoBedOrder && r > 0.0f && out && IsPlayerAI(self))
    {
        Character* c2 = self->getCharacter();
        int s2 = CharSlot(c2);
        if (s2 >= 0)
        {
            __try { g_bedOf[s2] = out->getBuilding(); }
            __except (EXCEPTION_EXECUTE_HANDLER) { g_bedOf[s2] = 0; }
        }
    }

    if (g_debug && IsPlayerAI(self))
    {
        char who[64]; CharName(self->getCharacter(), who, sizeof(who));
        Log(LC_FINDBED, "[%s] findOwnedFreeBed  결과=%.3f  (arg=%d)", who, r, b ? 1 : 0);
    }
    return r;
}

// 같은 사유를 캐릭터마다 한 번씩만 남긴다.
// v14 는 매 호출마다 찍어서 로그 상한 200 줄을 건너뛴 이유로 다 채웠고,
// 정작 보려던 "침대 목표 주입" 줄이 밀려났다.
enum SkipReason { SKIP_SELECTED = 1, SKIP_NOTS = 2, SKIP_NOTASK = 4, SKIP_ALREADY = 8,
                  SKIP_JOBS = 16 };
static int g_skipLogged[MAXCH] = { 0 };

static void LogOnce(int slot, int reason, const char* fmt, const char* who)
{
    if (slot < 0 || slot >= MAXCH) return;
    if (g_skipLogged[slot] & reason) return;
    g_skipLogged[slot] |= reason;
    Log(LC_INJECT, fmt, who);
}

// ---------------------------------------------------------------------------
//  잡 선택 건너뛰기 (skipJobs)
//
//  실측: 침대는 10점 @ NON_URGENT, 잡은 13점 @ URGENT.
//  점수로도 버킷으로도 못 이긴다. 점수를 올려도 그 값이 목표로 안 간다.
//  그래서 경쟁을 이기려 하지 않고, 부상 중에는 잡 후보를 아예 안 만든다.
//
//  잡 목록 자체는 손대지 않는다. addPermajob 이 없어서 뺀 잡은 되돌릴 방법이
//  없기 때문이다. 이 방식은 매 프레임 후보 생성만 건너뛰므로
//  게임 상태도 세이브도 바뀌지 않는다. cfg 로 끄면 즉시 원상복구된다.
//
//  주의: 침대로 가기가 잡 경로를 타고 나온다면 이것도 같이 막힌다.
//        그러면 아무 변화가 없거나 오히려 나빠진다. 그때는 skipJobs=0.
// ---------------------------------------------------------------------------
typedef void (*ChooseJobFn)(AITaskSytem*, void*, void*, bool, bool);
static ChooseJobFn origChooseJob = 0;

static void hookChooseJob(AITaskSytem* ts, void* goals, void* out, bool a, bool b)
{
    if (!origChooseJob) return;

    if (g_skipJobs && ts)
    {
        for (int i = 0; i < g_idCount; ++i)
        {
            if (g_tsOf[i] != (void*)ts) continue;
            if (g_injured[i])
            {
                char who[64];
                CharName((Character*)g_ids[i], who, sizeof(who));
                LogOnce(i, SKIP_JOBS, "[%s] 부상 중 — 잡 후보 생성 건너뜀", who);
                return;                       // 원본을 부르지 않는다
            }
            break;
        }
    }

    origChooseJob(ts, goals, out, a, b);
}


//  선택 중 = 사용자가 직접 조종 중이므로 침대 강제를 하지 않는다.
// ---------------------------------------------------------------------------
typedef void (*SelectFn)(Character*);
static SelectFn origSelect   = 0;
static SelectFn origUnselect = 0;

static void hookSelect(Character* self)
{
    if (!origSelect) return;
    origSelect(self);
    if (!IsPlayerChar(self)) return;
    char who[64]; CharName(self, who, sizeof(who));
    int slot = CharSlot(self);
    if (slot >= 0 && !g_selected[slot])
    {
        g_selected[slot] = true;
        MedicalSystem* med = self->getMedical();
        if (g_debug && med && !med->isFullyRested())
            Log(LC_SEL, "[%s] 선택됨 (부상 중)%s", who,
                g_skipSelected ? " — 강제 중단" : "");
    }
}

static void hookUnselect(Character* self)
{
    if (!origUnselect) return;
    origUnselect(self);
    if (!IsPlayerChar(self)) return;
    char who[64]; CharName(self, who, sizeof(who));
    int slot = CharSlot(self);
    if (slot >= 0 && g_selected[slot])
    {
        g_selected[slot] = false;
        MedicalSystem* med = self->getMedical();
        if (g_debug && med && !med->isFullyRested())
            Log(LC_SEL, "[%s] 선택 해제 (부상 중)", who);
    }
}

// ---------------------------------------------------------------------------
//  목표 직접 주입 (forceGoal)
//
//  잡은 URGENT(3) 버킷에서 뽑히고 침대 목표는 NON_URGENT(2) 라
//  점수로는 이길 수 없다. 그래서 점수 경쟁을 하지 않고,
//  AITaskSytem 이 이미 들고 있는 autoSleepTask 를
//  setCurrentGoal(task, 점수, TP_URGENT) 로 현재 목표에 직접 넣는다.
//
//  주입 위치는 update4Frame 이다. 점수 함수 안에서 목표를 갈아끼우면
//  게임이 목표를 고르는 도중에 판을 엎는 셈이라 위험하다.
//
//  autoSleepTask 는 AITaskSytem + 0x290 (헤더 기준). 널이면 아무것도 안 한다.
// ---------------------------------------------------------------------------
typedef void (*Update4Fn)(AI*, float);
static Update4Fn origUpdate4 = 0;
static int g_tick[MAXCH] = { 0 };
static int g_injCount[MAXCH] = { 0 };

// 슬롯을 재사용할 때 이전 캐릭터의 흔적을 지운다 (CharName 의 축출 경로가 부른다).
// hookGetUp / DumpPartsOnce 의 함수 내부 "한 번만" 플래그는 못 지우지만
// 그건 디버그 덤프 1회 생략일 뿐이라 무해하다.
static void ResetSlot(int s)
{
    if (s < 0 || s >= MAXCH) return;
    g_jobsPaused[s] = false;   g_selected[s] = false;
    g_tsOf[s] = 0;             g_injured[s] = false;
    g_bedOrderWhileHurt[s] = false;
    g_tick[s] = 0;  g_injCount[s] = 0;  g_skipLogged[s] = 0;
    g_bedOf[s] = 0; g_lastOrder[s] = 0;   // v34
    g_inBed[s] = false; g_inBedTick[s] = 0; g_orderCount[s] = 0;   // v35
}

static void hookUpdate4(AI* self, float t)
{
    if (!origUpdate4) return;
    origUpdate4(self, t);

    CheckConfigReload();   // v37. 자체 스로틀(3초) — 매 호출 비용은 u64 비교 하나.

    if (!IsPlayerAI(self)) return;
    LogThreadOnce("update4Frame");
    ++g_now;   // 슬롯 재사용 판단용 틱. 아군 프레임마다 증가하면 충분하다.

    // forceGoal 이 꺼져 있어도 여기까지는 온다.
    // choosePermaJob 후크가 쓸 대응표와 부상 플래그를 여기서 갱신한다.
    {
        Character* pc = self->getCharacter();
        char tmp[64];
        CharName(pc, tmp, sizeof(tmp));              // 슬롯 등록
        int s = CharSlot(pc);
        if (s >= 0)
        {
            AITaskSytem* ts0 = self->getTaskSystem();
            g_tsOf[s] = (void*)ts0;
            MedicalSystem* m = pc ? pc->getMedical() : 0;
            g_injured[s] = (m && !FullyHealed(m));

            // 부위 덤프는 여기서 한다.
            // GoToBed 후크에 두면 선택 중이거나 명령이 걸린 캐릭터는
            // 그 후크 자체가 안 불려서 영원히 안 찍힌다 (스노우 화이트가 그랬다).
            // 이 자리는 매 프레임 아군 전원에 대해 돌아간다.
            DumpPartsOnce(m, tmp, s);

            // 수동으로 침대에 눕히면 그 명령(USE_BED_ORDER)이 계속 남는다.
            // 명령은 TP_OBEDIENCE 라 목표 평가 자체가 멈춘다 — 실측: 명령 이후
            // 그 캐릭터의 GoToBed / GetUpHealed 로그가 통째로 사라졌다.
            // 그래서 다 나아도 안 일어나고 일도 재개하지 않는다.
            //
            // 다만 "다친 상태에서 걸린 명령"만 풀어야 한다.
            // v23 은 그 구분이 없어서, 이미 회복된 캐릭터를 사용자가 눕히는 족족
            // 그 자리에서 명령을 취소해버렸다 (로그에 눕히기/해제가 여섯 번 반복).
            // 그래서 부상 중 취침 명령을 본 순간에만 표시해 두고,
            // 회복된 뒤 그 표시가 있을 때만 해제한다.
            bool hasBedOrder = ts0 && ts0->hasPlayerOrder((TaskType)258);

            if (g_injured[s] && hasBedOrder)
                g_bedOrderWhileHurt[s] = true;      // 다친 채로 눕혔다 — 나중에 풀어준다

            if (g_clearBedOrder && ts0 && !g_injured[s] &&
                hasBedOrder && g_bedOrderWhileHurt[s])
            {
                // 푸는 그 순간의 부위 상태를 그대로 남긴다.
                // 한 번만 찍는 최초 덤프로는 "해제 시점에 정말 만땅이었나"를 알 수 없다.
                DumpPartsTagged(m, tmp, "명령해제시점");
                ts0->clearOrders();
                g_bedOrderWhileHurt[s] = false;
                Log(LC_ORDER, "[%s] 회복 완료 — 수동 취침 명령 해제", tmp);
            }
            if (!hasBedOrder) g_bedOrderWhileHurt[s] = false;
            if (!g_injured[s]) g_orderCount[s] = 0;   // v35: 부상이 끝나면 카운터를 접는다

            // ---------------------------------------------------------------
            //  v34 : 우클릭을 대신 낸다 (2안)
            //
            //  점수로 잡을 이기려는 시도는 다섯 번 다 실패했다(2절).
            //  명령은 TP_OBEDIENCE 라 잡보다 위이므로, 상시 잡 보유자도 눕는다.
            //  회복 후 복귀는 바로 위 clearBedOrder 가 이미 처리한다.
            //
            //  발행 조건을 좁게 잡은 이유는 WASD 소스의 경고 때문이다:
            //  명령을 매 프레임 다시 내면 경로탐색이 계속 초기화돼
            //  캐릭터가 아예 출발하지 못한다.
            // ---------------------------------------------------------------
            if (g_autoBedOrder && pc && ts0 && g_injured[s] && !hasBedOrder
                && !g_selected[s] && Eligible(self))
            {
                unsigned __int64 now = GetTickCount64();
                Building* bed = g_bedOf[s];

                // v35: 이미 침대에 누워 있으면 명령을 내지 않는다.
                // v34 로그에서 같은 사람이 침대를 갈아타며 계속 명령을 받았다.
                // 이유로 보이는 것: 침대에 도착하면 명령이 완료되어 사라지고
                // (hasBedOrder=false), 아직 다쳤으니 다시 발행 조건이 성립한다.
                // 그러면 자던 사람을 일으켜 다른 침대로 보내게 된다.
                // 침대 정보는 hookGetUp 이 채우는데 그 후크가 한동안 안 불릴 수
                // 있으므로, 오래된 값(20초 초과)은 믿지 않는다.
                bool inBedNow = g_inBed[s] &&
                                (now - g_inBedTick[s] <= 20000);

                // 한 프레임에 한 명만. 여러 명이 동시에 명령을 받으면
                // 어느 것이 문제였는지 로그로 못 가린다.
                static unsigned __int64 lastAnyOrderTick = 0;
                bool frameFree = (now != lastAnyOrderTick);

                if (!inBedNow && bed && frameFree &&
                    (g_lastOrder[s] == 0 ||
                     now - g_lastOrder[s] >= (unsigned __int64)g_bedOrderCooldownMs))
                {
                    Building* dest = 0;
                    Ogre::Vector3 loc = { 0, 0, 0 };
                    bool ok = false;
                    __try
                    {
                        // 침대가 정말 가구인지 먼저 확인한다. 아니면 건드리지 않는다.
                        if (bed->isFurniture())
                        {
                            dest = bed->furnitureParentBuilding();
                            loc  = ((RootObjectBase*)bed)->_NV_getPosition();
                            ok   = true;
                        }
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) { ok = false; }

                    if (ok && dest)
                    {
                        // 직전 발행으로부터 실제로 몇 초가 지났는지 함께 남긴다.
                        // v34 로그에는 시각이 없어서 "도배인지 정상 간격인지"를
                        // 가릴 수 없었다 — 내가 도배라고 단정했던 부분이다.
                        double sinceSec = g_lastOrder[s] ? (double)(now - g_lastOrder[s]) / 1000.0 : -1.0;
                        ++g_orderCount[s];

                        g_lastOrder[s] = now;
                        lastAnyOrderTick = now;

                        // 발행 직전에 인자 전부를 남긴다.
                        // v33 의 [명령] 줄(사람이 우클릭한 것)과 눈으로 대조할 것.
                        // 이름은 동명이인이 있다 (먼지 도적 등 포로가 여럿).
                        // 슬롯 번호(#N)는 캐릭터 포인터 하나당 하나라 그것으로 가른다.
                        // v34 로그를 "한 명이 침대를 갈아탄다"고 읽었는데,
                        // 실은 동명이인 여러 명이 각자 한 번씩이었을 수 있다.
                        Log(LC_PROBE,
                            "[자동] %s#%d  USE_BED_ORDER(258)  dest=%p  subject=%p  "
                            "shift=0 clear=1  pos=(%.1f,%.1f,%.1f)  직전발행=%.1f초전  이번부상중 %d번째",
                            tmp, s, (void*)dest, (void*)bed, loc.x, loc.y, loc.z,
                            sinceSec, g_orderCount[s]);

                        __try
                        {
                            pc->addOrder(dest, (TaskType)258, (RootObject*)bed,
                                         false, true, loc);
                        }
                        __except (EXCEPTION_EXECUTE_HANDLER)
                        {
                            Log(LC_PROBE, "[자동] %s#%d  addOrder 에서 예외 — 이 경로는 안전하지 않다", tmp, s);
                            g_autoBedOrder = false;   // 한 번 터지면 더 시도하지 않는다
                        }
                    }
                    else if (g_debug)
                    {
                        Log(LC_PROBE, "[자동] %s#%d  건너뜀 (isFurniture=%d dest=%p)",
                            tmp, s, ok ? 1 : 0, (void*)dest);
                    }
                }
            }
        }
    }

    if (g_forceGoal <= 0.0f) return;

    Character* c = self->getCharacter();
    MedicalSystem* med = c ? c->getMedical() : 0;
    if (!med || FullyHealed(med)) return;            // 멀쩡하면 관여 안 함

    // 여기부터는 "부상당한 아군"이 확정된 상태다.
    char who[64];
    CharName(c, who, sizeof(who));                   // 슬롯 등록도 겸한다
    int slot = CharSlot(c);
    if (slot < 0) return;

    // 선택 검사는 기본으로 하지 않는다.
    // 선택한 채로 저장하면 불러온 뒤에도 계속 선택 상태로 남아,
    // 그 캐릭터만 영구히 제외되는 함정이 된다 (실측).
    if (g_skipSelected && g_selected[slot])
    {
        LogOnce(slot, SKIP_SELECTED, "[%s] 주입 건너뜀: 선택 중", who);
        return;
    }

    AITaskSytem* ts = self->getTaskSystem();
    if (!ts) { LogOnce(slot, SKIP_NOTS, "[%s] 주입 건너뜀: 태스크시스템 널", who); return; }

    Tasker* sleepTask = *(Tasker* const*)((const unsigned char*)ts + 0x290);
    if (!sleepTask)
    {
        LogOnce(slot, SKIP_NOTASK, "[%s] 주입 건너뜀: autoSleepTask 널", who);
        return;
    }

    // 이미 침대가 목표면 손대지 않는다. 계속 꽂으면 목표가 리셋돼서
    // 제자리걸음이 된다. 자기제한이 걸리는 지점이다.
    if (ts->getCurrentGoal().sameAs(sleepTask))
    {
        g_tick[slot] = g_goalEvery;                  // 뺏기면 곧바로 되찾도록
        LogOnce(slot, SKIP_ALREADY, "[%s] 이미 침대 목표 — 관여 안 함", who);
        return;
    }

    // 침대가 목표가 아니다 = 잡이나 다른 것이 가져갔다. 최소 간격만 지키고 되찾는다.
    if (++g_tick[slot] < g_goalEvery) return;
    g_tick[slot] = 0;

    taskPriority pri = (taskPriority)g_goalPriority;
    ts->_NV_setCurrentGoal(sleepTask, g_forceGoal, pri);

    // 주입은 초당 여러 번 일어날 수 있다. 매번 찍으면 로그가 그것만으로 찬다.
    // 10 회마다 한 줄씩, 누적 횟수와 함께 남긴다.
    if (++g_injCount[slot] % 10 == 1)
        Log(LC_INJECT, "[%s] 침대 목표 주입 (우선순위=%d, %.1f)  누적 %d회",
            who, g_goalPriority, g_forceGoal, g_injCount[slot]);
}

// ---------------------------------------------------------------------------
//  후킹 설치
// ---------------------------------------------------------------------------
static void Install(const char* name, void* addr, void* hook, void** orig)
{
    if (!addr)
    {
        Log(LC_INIT, "  %-16s 주소 해석 실패 — 후킹 안 함", name);
        return;
    }
    int st = (int)KenshiLib::AddHook(addr, hook, orig);
    Log(LC_INIT, "  %-16s addr=%p  status=%d  orig=%p", name, addr, st, *orig);
}

// ---------------------------------------------------------------------------
//  진입점  (RE_Kenshi 가 "?startPlugin@@YAXXZ" 로 찾는다 — extern "C" 금지)
// ---------------------------------------------------------------------------
__declspec(dllexport) void startPlugin();

void startPlugin()
{
    LoadConfig();
    WriteConfigSnapshot();   // 읽은 값을 그대로 되적어 현재 상태를 남긴다

    // 이 줄이 로그에 없으면 DLL 자체가 로드되지 않은 것이다.
    Log(LC_INIT, "SleepFix 시작");
    Log(LC_INIT, "  bedMult=%.2f  getUpMult=%.2f  forceBed=%.2f",
        g_bedMult, g_getUpMult, g_forceBed);
    Log(LC_INIT, "  forceGoal=%.2f  goalEvery=%d  goalPriority=%d  %s",
        g_forceGoal, g_goalEvery, g_goalPriority,
        g_forceGoal > 0.0f ? "(목표 주입 켜짐)" : "(목표 주입 꺼짐)");
    Log(LC_INIT, "  stayInBed=%d  clearBedOrder=%d  healThreshold=%.3f  skipJobs=%d",
        g_stayInBed ? 1 : 0, g_clearBedOrder ? 1 : 0, g_healThreshold, g_skipJobs ? 1 : 0);
    Log(LC_INIT, "  skipCombat=%d  skipSelected=%d  pauseJobs=%d  logLimit=%d  debug=%d",
        g_skipCombat ? 1 : 0, g_skipSelected ? 1 : 0, g_pauseJobs ? 1 : 0,
        g_logLimit, g_debug ? 1 : 0);
    Log(LC_INIT, "  probeOrder=%d  (1이면 우클릭 명령 인자와 침대를 debug=0 에도 기록)", g_probeOrder ? 1 : 0);
    Log(LC_INIT, "  autoBedOrder=%d  bedOrderCooldownMs=%d  %s",
        g_autoBedOrder ? 1 : 0, g_bedOrderCooldownMs,
        g_autoBedOrder ? "(다친 아군에게 침대 명령을 대신 낸다)" : "(꺼짐)");

    Install("GoToBed",
            (void*)KenshiLib::GetRealAddress(PMF(&AI::_NV_scoreGoToBed)),
            (void*)&hookGoToBed, (void**)&origGoToBed);
    Install("GetUpHealed",
            (void*)KenshiLib::GetRealAddress(PMF(&AI::_NV_scoreGetOutOfBedOnceFullyHealed)),
            (void*)&hookGetUp, (void**)&origGetUp);
    Install("findOwnedFreeBed",
            (void*)KenshiLib::GetRealAddress(PMF(&AI::findOwnedFreeBed)),
            (void*)&hookFindBed, (void**)&origFindBed);
    Install("addOrder",
            (void*)KenshiLib::GetRealAddress(PMF(&Character::addOrder)),
            (void*)&hookAddOrder, (void**)&origAddOrder);
    Install("addJob",
            (void*)KenshiLib::GetRealAddress(PMF(&Character::addJob)),
            (void*)&hookAddJob, (void**)&origAddJob);
    Install("우클릭명령",
            (void*)KenshiLib::GetRealAddress(PMF(&Character::_NV_playerMoveOrderDefault)),
            (void*)&hookMoveOrder, (void**)&origMoveOrder);

    Install("선택",
            (void*)KenshiLib::GetRealAddress(PMF(&Character::_NV_select)),
            (void*)&hookSelect, (void**)&origSelect);
    Install("선택해제",
            (void*)KenshiLib::GetRealAddress(PMF(&Character::_NV_unselect)),
            (void*)&hookUnselect, (void**)&origUnselect);

    Install("프레임갱신",
            (void*)KenshiLib::GetRealAddress(PMF(&AI::_NV_update4Frame)),
            (void*)&hookUpdate4, (void**)&origUpdate4);

    // choosePermaJob 은 인자에 Ogre 할당자를 쓴 std::map 이 들어간다.
    // 그 타입을 흉내내면 컴파일러마다 깨지므로, 맹글링 이름으로 직접 찾는다.
    // KenshiLib 의 익스포트 주소를 GetRealAddress 에 넣으면 게임 주소가 나온다.
    {
        static const char* CHOOSE_JOB =
            "?choosePermaJob@AITaskSytem@@QEAAXAEAV?$map@MPEAVTasker@@U?$less@M@std@@"
            "V?$STLAllocator@U?$pair@$$CBMPEAVTasker@@@std@@V?$CategorisedAllocPolicy@$0A@"
            "@Ogre@@@Ogre@@@std@@AEAVTaskMatch@@_N2@Z";
        HMODULE lib = GetModuleHandleA("KenshiLib.dll");
        void* stub = lib ? (void*)GetProcAddress(lib, CHOOSE_JOB) : 0;
        Install("잡선택", stub ? (void*)KenshiLib::GetRealAddress(stub) : 0,
                (void*)&hookChooseJob, (void**)&origChooseJob);
    }

    Log(LC_INIT, "설치 완료. orig 이 0 인 항목은 후킹 실패이며 그 후크는 아무 일도 하지 않는다.");
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) { /* startPlugin 에서 처리 */ }
    return TRUE;
}
