// ============================================================================
//  FurnaceSlots  —  RE_Kenshi / KenshiLib 플러그인
//
//  아이템 용광로의 입력 칸을 늘린다.
//
//  왜 필요한가
//    시체에서 회수한 무기·방어구는 스택이 안 되어 창고를 빠르게 채운다.
//    용광로에 넣어 철로 바꾸면 스택되지만, 입력 칸이 84개(12x7)뿐이라
//    전투 한 번 분량이면 금방 찬다.
//
//  v23.3 — 격자 훅 은퇴 (기본값). E 실험으로 격자가 FCS 로 이관됐다:
//    BasusuFurnaceSplit 의 용광로 레코드에 functionality flags=1 +
//    storage size 24x14 를 넣으면 KEP 가 그 크기로 in1 을 만든다 (인게임 검증,
//    바닐라 아이템 용광로의 18x18 이 이 경로의 원조였다).
//    matchWidth 기본 12 → 0. cfg 를 지워도 훅이 부활하지 않는다.
//    훅 코드 자체는 남긴다 — dumpSections 진단이 실려 있고, FCS 값이
//    깨졌을 때 12 로 되살리는 비상용이다.
//    스택 잭팟은 기각: KEP 가 min=10 을 기본 주입하는데도 장비가 쌓인 적
//    없고(장기 실측), mult=10 수동 실험 불발, 전리품은 품질·내구 분산이라
//    병합 조건이 실전에서 성립하지 않는다. 336칸 체제가 최종이다.
//
//  v23.2 — 코드 기본값을 "완성 상태"로 뒤집는다. cfg 가 없어도 검증 완료된
//    최적 동작으로 뜬다: listWindow=200 cutMin=1000 keepActive=1, 계측·진단
//    (measure/fillHook/classify/debug/dumpSections) 전부 0. 이전 기본은 계측판
//    시절 것이라, cfg 를 지우면 자르기가 꺼진 채 시작하는 함정이 있었다.
//
//  v23.1 — 설치 조건 버그 수정. 자르기가 measure=1 에 묶여 있었다.
//    TruncateList 는 목록생성 후크 안에서 도는데, 그 후크가 if(g_measure)
//    블록에서만 설치됐다 — 상시 설정(measure=0)으로 바꾸는 순간 listWindow 가
//    소리 없이 죽는 함정. 목록생성 후크는 이제 무조건 설치한다.
//    공급처찾기 후크는 (measure || classify) 로, 자원확인은 measure 전용으로 분리.
//    상시 권장 설정: measure=0 fillHook=0 classify=0 debug=0 listWindow=200 — 이
//    조합에서 남는 후크는 기본 3종 + 목록생성 하나, 자르기는 정상 작동한다.
//
//  v23 — cfg 핫리로드. 게임을 끄지 않고 cfg 를 고칠 수 있다 (BetterLooting 의
//    설정창에서 착안하되, 창 대신 파일 감시로 축소). 매 프레임 훅에서 3초에
//    한 번 cfg 수정 시각을 보고, 바뀌었으면 LoadConfig 를 다시 돌린 뒤
//    반영값을 전문 로그로 남긴다 (cfg 미반영 헛수고 4회의 재발 방지).
//    게임 스레드에서만 돌므로 락 불필요 — 후크 실행 중간에 값이 안 바뀐다.
//    감시 비용: 프레임당 u64 비교 1회 + 3초마다 mtime 조회 1회 (~0.0001%).
//    [고정 항목] measure / fillHook 은 시작 시 훅 설치를 결정하므로
//    재적용해도 효과가 없다 — 이 둘만은 재시작이 필요하다.
//
//  v22.1 — 파싱 실측 반영. 자르기 로직 불변, 가드 하나와 표기 정정.
//    zip 파싱(바닐라 4 + 활성 모드 303, 로드 순서 오버라이드 접기)으로 확정:
//      WEAPON 197 / ARMOUR 6958 — 로그의 두 숫자와 레코드 단위 정확 일치.
//      즉 비쌌던 것은 방어구 용광로다. 여태 "무기 용광로 6958"이라 부른 것은
//      반대였다 (모드 목록이 의류·방어구 위주라 방어구 정의만 35배로 불었다).
//      그 외 전수: CROSSBOW 112, LIMB 148, CONTAINER 257, NEST 22, MAP 20.
//    변경:
//      cutMin (기본 1000) — 원본 개수가 이보다 작으면 자르지 않는다.
//        CONTAINER(257) 같은 싼 목록까지 잘라 발견 지연만 만드는 일을 막는다.
//        현재 환경에서 1000 을 넘는 타입은 ARMOUR(6958) 하나뿐이다.
//      주석·cfg 문구의 무기↔방어구 정정.
//
//  v22 — 목록을 잘라서 돌려준다 (성능) + 공급처가 누구인지 센다 (관찰).
//    v21 실측으로 확정된 것:
//      비용 ∝ 목록 개수. 항목당 ~2.4us 로 두 용광로가 일치한다.
//        [개수197] 496~657us   [개수6958] 16,307us  (35배 개수 → 31배 비용)
//      목록생성 자체는 그중 4%(214ms/5,422ms). 나머지 96%는 부르는 쪽이
//        6,958개를 하나씩 훑는 비용이다 → **캐싱이 아니라 개수 줄이기가 레버.**
//      잡채움 A/B 기각 (최대 2.0ms, 대개 0.1ms 이하).
//      드랍은 공급처찾기 하나로 사실상 전부 설명된다. 최악 창에서 그것을
//        빼면 585프레임 → 7.9ms/프레임 = 127fps, 같은 세션 한가 135fps 와 붙는다.
//      (v19 때의 "미귀속 40%" 는 세션 간 베이스라인을 섞은 오판이었다.
//       fps 대조는 반드시 같은 세션 안에서 할 것.)
//
//    [완화] listWindow — 목록을 K개짜리 창으로 잘라 돌려준다. 기본 0=끔.
//      방법: 원본이 채운 뒤 count 필드(+0x8)를 K 로 덮는다. 버퍼는 그대로,
//        재할당이 없다 — v14 의 힙 불일치는 "키울 때" 나는 것이라 원리상 없다.
//      회전: 호출마다 시작 위치를 K 씩 민다. 어떤 항목도 영구 배제되지 않고
//        발견이 몇 틱 늦어질 뿐이다.
//      폐기된 스로틀과 다른 점: **빈 답이 아니다.** 돌려주는 항목은 전부
//        진짜 "필요한 것"이다. 스로틀은 "넣을 것 없음"이라 거짓말해서
//        일꾼 8명을 세웠다 (v15 실측). 그 구조가 여기엔 없다.
//      그래도 위험은 남는다: 일꾼이 특정 무기를 늦게 집을 수 있다.
//        **켠 뒤 무기가 계속 용광로로 옮겨지는지 눈으로 확인할 것.**
//      버킷 키는 자르기 **전** 개수를 쓴다. 그래야 [개수6958] 평균이
//        자르기 전후로 직접 비교된다.
//
//    [관찰] classify — 공급처찾기가 돌려준 hand 가 건물인지 캐릭터인지 센다.
//      사용자 관찰: 여럿이 한 아이템으로 달려가다 누가 줍자 그 사람을 따라갔다.
//      hand 의 getBuilding/getCharacter/getItem/isNull 은 익스포트에 있다
//      (추측이 아니라 헤더 확인). 세기만 하고 답은 건드리지 않는다.
//
//  v21 — 다시 계측판. 동작 변경 0. v19 가 남긴 구멍 둘을 닫는다.
//    v19 로 확정된 것:
//      히치(>33ms)의 범인은 공급처찾기 — 최악 단발 28.2ms, 호출당 평균이
//      70초 사이 822→1730us 로 두 배 성장 (뭔가 쌓이는 것을 스캔한다).
//      자원확인은 무죄 확정 (평균 1.4~1.7us, 5,547회에도 0.08%).
//    안 닫힌 것 둘:
//      1) 잡평가 합계(최대 27%)를 전부 빼도 산수상 ~135fps — 한가 267 의 절반.
//         하역 중 프레임 시간의 40~48%가 미계측이고, 스파이크 없이 매끈하다.
//      2) 공급처찾기 비용이 "안에서 만든 목록 개수"에 비례하는가?
//         같은 세션에 6958짜리와 197짜리 용광로가 둘 다 있다 — 자연 A/B.
//    그래서 이번 판이 재는 것:
//      - 공급처찾기를 목록 개수 버킷(생성없음/197/6958/...)별로 평균·최대.
//        비례가 나오면 목록 축소(제자리 줄이기) 공격이 유효,
//        안 나오면 범인은 월드 스캔이고 축소는 헛수고다.
//      - 잡 채움(Task_FillMachine) 둘의 횟수·시간. 익스포트가 아니라서
//        KEP 의 1.0.65 RVA 표(0x340EB0/0x343720)로 찾는다. KEP 가 이미
//        후킹한 자리라 주소는 실전에서 검증돼 있다. 미귀속 부하의
//        용의자를 싸게 확정하거나 기각한다.
//      - 프레임·목록생성·자원확인은 v19 형식 유지 (창 간 비교용).
//    측정이 끝나면 cfg 에서 measure=0, fillHook=0 으로 끈다.
//
//  v20 — 측정 장비를 걷어낸다. 결론이 났으므로 더 잴 것이 없다.
//    v18~v19 로 프레임 드랍의 답이 나왔다:
//      목록생성(6,958개)은 전체의 1.5% 뿐 — 범인이 아니었다.
//      진짜는 잡 평가 경로 전체(findResourceSourceForMe 포함)로 최대 27%.
//      **우리가 안전하게 손댈 자리가 없다.** 남은 대응은 운영이다
//      (하역 인원 줄이기 / 배속 낮추기).
//    그래서 결론에 쓰이지 않는 것을 전부 제거했다:
//      후크 3개 — getResourcesNeededBecauseEmpty / haveSomeResourcesFor /
//                 findResourceSourceForMe (계측 + 실패한 캐시)
//      완화책 3개 — resourceCacheMs / resEmptyThrottleMs / resourceSourceCacheMs
//                   (전부 실패 확정. 이유는 지식 문서 9절, 재시도 금지)
//      RVA 스캐너 (findRva, StubTarget, ResolveRva) — 답을 얻었다
//      프레임·시간 계측 (QPC), 10초 요약, 진단 카운터 일체
//    아이러니하게도 성능을 재던 코드가 남아서 성능을 먹고 있었다.
//    81,624 -> 49,649 바이트, 후크 9개 -> 6개.
//    남긴 기능은 둘뿐이다: 격자 확장(24x14), 물건이 들어오면 활성화.
//    activeWhenEmpty(=v8 무조건 켜두기)는 되돌리기용으로 남긴다.
//
//  v19 — 목록생성은 범인이 아니었다. 잡 평가 경로 전체를 잰다.
//    v18 실측 (5배속, 잡 8명):
//      목록생성 비용 = 평균 651~737us, 최대 2000us,
//      **10초 중 합계 9.6~152.7ms = 0.10~1.53%뿐이다.**
//      개별 호출은 무겁지만 전체 점유율은 미미하다.
//      → v10~v17 여덟 판을 전체의 1.5%짜리에 쏟았다.
//        "6958개니까 무겁겠지"를 근거로 밀어붙인 것이 잘못이었다.
//    진짜 신호는 프레임 줄에 있었다:
//      한가할 때 2668프레임/10초 (267fps), 느림 0
//      잡 도는 중 886프레임/10초 (89fps), 느림 47
//      = 3분의 1로 떨어진다. 드랍은 실재한다.
//      같이 오른 것: 자원확인 108->4246회, 공급처찾기 31->960회.
//      즉 비용은 **잡 평가 경로 전체**이고 목록생성은 그 안의 일부였다.
//    이번 판: 이미 후킹해 둔 두 함수의 시간을 잰다. 동작 변경 없음.
//      findResourceSourceForMe (공급처찾기)
//      haveSomeResourcesFor    (자원확인)
//      각각 10초 중 몇 %인지 나오면 방향이 갈린다:
//        합쳐서 20~30% -> 잡 평가 자체가 무겁다. 목록생성만 고쳐선 소용없다.
//        합쳐서 5% 미만 -> 드랍은 잡 평가 밖에 있다. 용광로를 놓고 딴 데를 본다.
//    확정된 것도 하나 (CorpseLoot v21 로그, 14회 전부):
//      "lektor 소유권: stuff=유지" -> 게임이 우리 버퍼를 그대로 쓴다.
//      v9 의 누수 대책이 옳았고 누수는 없다. 미검증 항목 하나 해소.
//
//  v18 — 아무것도 바꾸지 않고 재기만 한다. 세 가지를 잰다.
//    v17 결과: 후킹은 됐는데 캐시가 0번 걸렸다.
//      "공급처찾기 787회 (물어만봄 0, 캐시적중 0)" — justAsking 이 전부 false 다.
//      내가 "물어만 보는 호출이 대부분일 것"이라 가정한 것이 정반대였다.
//      전부 false 라는 것은 이 함수가 실제로 뭔가를 정하는 용도로만 불린다는
//      뜻이므로, 여기를 캐싱하는 것은 위험하다. 이 길도 접는다.
//    지금까지 막힌 세 자리:
//      목록 자체 캐싱   - lektor 를 우리가 키우면 힙이 깨진다 (v14)
//      목록 스로틀      - 빈 답은 거짓말이라 일꾼이 멈춘다 (v14 실측)
//      공급처찾기 캐싱  - justAsking 이 전부 false (v17 실측)
//    그런데 숫자에 앞뒤가 안 맞는 데가 있다:
//      호출자는 findResourceSourceForMe +0xB0 **하나뿐**인데(v16 확정),
//      그 함수 787회에 목록생성은 141회다. 안에 조건 분기가 있다.
//      그 조건을 모르는 채로 세 자리를 시도했고 다 막혔다.
//    그리고 더 근본적으로 — **한 번의 실제 비용을 아직 안 쟀다.**
//      "6958개니까 무겁겠지"를 근거로 다섯 판을 썼다. 만약 한 번에
//      20마이크로초라면 초당 141회는 0.3%뿐이고 범인은 딴 데 있다.
//    그래서 이번 판은 재기만 한다:
//      1. 목록생성 한 번의 소요 시간 (QPC, 평균·최대·10초간 총점유율)
//      2. 프레임 시간 — 느린 프레임 횟수. 체감이 아니라 숫자로 본다.
//         (v10~v12 의 오판이 전부 체감 판정에서 나왔다)
//      3. 목록생성이 나는 조건 — 그때의 용광로·활성상태·개수
//    동작 변경 없음. 캐시·스로틀은 전부 기본 꺼짐으로 둔다.
//
//  v17 — 진짜 자리를 찾았다. findResourceSourceForMe 를 캐싱한다.
//    v16 이 답을 줬다:
//      #1  시작RVA=0x59CF20  +0xB0 지점  AI::findResourceSourceForMe
//      (#2 는 0xA60 떨어져 있어 남남. #1 만 후보다)
//    이름 그대로 "내가 가져올 자원 공급처를 찾는다" — 하역 일꾼이
//    "어디서 뭘 가져다 넣지?"를 판단하는 함수다. 그 안에서 용광로마다
//    "뭐가 필요하냐"를 묻다가 6,958개짜리 목록이 만들어진다.
//    바로 옆(0x59C570)에 AIResultsCacher::getResourceSource 가 있다
//    = 게임 자신도 이 계열 결과를 캐싱하려고 만든 클래스가 있다는 뜻.
//
//    ?findResourceSourceForMe@AI@@QEAAMAEBVhand@@AEAV2@_N@Z
//      float findResourceSourceForMe(const hand& 대상, hand& 결과, bool justAsking)
//    돌려주는 것이 float 점수 + hand(0x20바이트 값 손잡이)뿐이라
//    통째로 기억했다 그대로 돌려줄 수 있다. v14 에서 막혔던
//    "lektor 를 우리가 키우면 힙이 깨진다" 문제가 여기엔 없다.
//
//    스로틀(폐기)과 결정적으로 다른 점: 일꾼에게 거짓말을 하지 않는다.
//    빈 답이 아니라 **직전의 진짜 답**을 준다. 조금 낡았을 뿐 사실이다.
//    그래서 일꾼이 잡을 놓고 교대하듯 멈추는 일이 없어야 한다.
//
//    [안전] justAsking=true 인 호출만 캐싱한다.
//      false 는 실제로 상태를 바꿀 수 있어 그대로 통과시킨다.
//      측정 줄에 참/거짓 비율을 찍는다 — 참이 대부분이어야 효과가 난다.
//    resourceSourceCacheMs=0 이면 캐싱하지 않고 세기만 한다.
//
//  v16 — RVA 찾기가 게임을 멈춰 세웠다. GetRealAddress 를 쓰지 않게 고쳤다.
//    v15 사고: 익스포트 9,789개를 전부 GetRealAddress 에 넣었더니
//      "Incorrect address in KenshiLib::GetRealAddress()
//       The function you are trying to hook appears to be a non-stub
//       KenshiLib function." assert 창이 떴다 (KenshiLib.dll+0x1550).
//      GetRealAddress 는 **게임 함수로 가는 stub 에만** 쓸 수 있는데
//      KenshiLib 자체 함수·데이터까지 무차별로 집어넣은 것이 원인이다.
//      → 남의 API 를 "아마 되겠지" 하고 전수로 먹인 것. 확인 안 된 전제였다.
//    수정: stub 의 점프 대상을 우리가 직접 읽는다.
//      x64 stub 은 보통 아래 둘 중 하나다.
//        E9 xx xx xx xx        jmp rel32      -> 다음명령 + rel32
//        FF 25 xx xx xx xx     jmp [rip+rel32] -> 그 자리에 든 값
//      두 모양이 아니면 그 심볼은 조용히 건너뛴다. assert 를 부를 일이 없다.
//      결과 주소가 게임 모듈 범위 밖이면 그것도 버린다.
//    이 방식은 읽기만 하므로 실패해도 창이 안 뜨고 게임에 영향이 없다.
//
//  v15 — 호출자가 무슨 함수인지 게임이 스스로 찾아 찍는다.
//    v14 결과:
//      최대칸이 매번 10 이고 버퍼도 매번 새것 = 상대 lektor 재사용이 아니다.
//      → 우리가 채워 넣는 정식 캐싱은 힙 불일치로 불가능. 그 길은 닫혔다.
//      호출자 RVA 가 0x59CFD0 과 0x59CFE7 둘뿐이고 항상 짝으로 나온다.
//      23바이트 차이 = **같은 함수 안에서 연달아 두 번** 부르는 것이다
//      (아마 BecauseEmpty 다음에 BecauseNotFull).
//    스로틀(resEmptyThrottleMs)은 폐기한다. 인게임 실측:
//      프레임 드랍은 거의 사라졌지만 일꾼 8명이 교대하듯 멈췄다 서기를 반복했다.
//      빈 목록을 "넣을 것 없음"으로 받아 잡을 놓기 때문이다.
//      로그상 진짜 답 60회 대 빈 답 1300회 — 스무 번에 한 번만 사실을 말한 셈.
//      기능을 망치므로 쓰지 않는다. 코드는 남기되 기본 0 유지.
//    이번 판이 하는 일:
//      KenshiLib 익스포트 전부(9,789개)를 훑어 실제 게임 주소로 환산하고,
//      목표 RVA 보다 작으면서 가장 가까운 것 세 개를 찍는다.
//      = "0x59CFD0 은 XXX 함수 시작 + 0x120 지점" 을 게임이 스스로 알려준다.
//      그 함수를 후킹해 결과를 통째로 재사용하면 목록을 만들 필요가 없어지고,
//      빈 목록으로 거짓말할 일도 없으니 일꾼이 멈추지 않는다.
//      찾기는 시작할 때 한 번만 한다 (findRva=0 이면 안 한다).
//
//  v14 — 범인 확정. 이제 "어떻게 고칠 수 있는가"를 잰다.
//    v13 측정 결과 (인게임, 할 일 없는 상태에서 수십 초):
//      목록생성 초당 17~48회, 한 번에 **6,958개**.
//      lektor 은 10칸에서 두 배씩 늘어나므로 6958개를 담으려면
//      재할당 11회 + 포인터 복사 약 1만 회. 초당 30회면
//      초당 33만 회 복사 + 330회의 힙 할당/해제(최대 80KB 덩어리).
//      → 프레임 드랍의 원인으로 확정.
//      자원확인 캐시는 80% 적중 중인데도 목록생성이 줄지 않았다
//      = haveSomeResourcesFor 말고 다른 곳에서도 부른다. 그 캐시로는 못 잡는다.
//    정식 캐싱을 하려면 상대가 넘긴 lektor 에 6958개를 채워야 하는데,
//    우리가 키우면 게임 힙에서 만든 버퍼를 우리 힙으로 바꾸는 셈이라
//    게임이 해제할 때 죽는다 (reserve/push_back 은 익스포트에 없다).
//    빠져나갈 구멍: 상대가 버퍼를 재사용한다면 이미 6958칸이 확보돼 있으므로
//    키우지 않고 채우기만 하면 된다 — 그건 안전하다.
//    그래서 이번 판은 두 가지를 잰다:
//      1. 들어올 때의 maxSize (재사용인가 매번 새것인가)
//      2. 호출자 주소 (_ReturnAddress). KEP RVA 표의 Task_FillMachine
//         (0x340EB0 / 0x343720) 이면 더 위에서 한 번에 막을 수 있다.
//         게임 베이스 주소를 함께 찍어 RVA 로 환산할 수 있게 한다.
//    임시 완화도 넣되 기본은 꺼둔다:
//      resEmptyThrottleMs — 같은 용광로가 이 시간 안에 또 물으면
//      목록을 만들지 않고 빈 채로 돌려준다. 초당 30회를 4회로 줄인다.
//      부작용 가능성: 일꾼이 "넣을 것 없음"으로 보고 지나칠 수 있다.
//      **무기가 계속 용광로로 옮겨지는지 반드시 눈으로 확인할 것.**
//
//  v13 — 측정 먼저. 그리고 고치는 코드는 넣되 꺼둔다.
//    v12 로그로 내 가설이 틀린 것이 드러났다:
//      "용광로 투입 (이미 활성)" 이 한 줄도 안 나왔다 = 투입 20회 전부
//      꺼진 상태에서였다. 용광로는 대부분 꺼져 있었는데도 프레임 드랍이 났다.
//      두 번 다 조건은 "잡 대기열에 남아 있을 때"였다.
//      → 범인은 활성 플래그가 아니라 잡 평가 자체다. 꺼져 있어도
//        일꾼이 "여기 뭘 넣지?"를 계속 묻고, 그때마다 KEP 가
//        무기 레코드를 전수 열거한다.
//      (전에 keepActive=0 으로 드랍이 사라진 것은 그때 잡이 안 걸려
//       있었던 것으로 보인다. 그것을 인과로 단정한 것이 v10~v12 의 뿌리였다.)
//    v10·v11·v12 를 추측으로 날렸으므로 이번엔 숫자를 먼저 본다.
//      측정 (measure=1, 기본 켬): 두 함수의 호출 횟수를 10초마다 한 줄로.
//        FurnaceBuilding::getResourcesNeededBecauseEmpty — 돌려주는 목록
//          개수까지 찍는다. 수천 개면 그 자리에서 확정.
//        AI::haveSomeResourcesFor — 그것을 부르는 쪽.
//      완화 (resourceCacheMs, 기본 0=끔): haveSomeResourcesFor 의 답을
//        잠깐 기억했다가 재사용한다. 참/거짓 하나만 다루므로 게임 메모리에
//        쓰지 않는다 — v10·v11 이 낸 부류의 사고가 구조적으로 안 난다.
//        측정으로 확인된 뒤 cfg 에서 500 정도로 켠다 (재빌드 불필요).
//      참고: 게임 자신도 AIResultsCacher 를 갖고 있다. 이 계열 결과를
//        캐싱하는 것은 원래 있는 패턴이다.
//    v12 의 투입 시 활성화는 그대로 둔다 (해가 없고 편의가 있다).
//
//  v12 — 상태를 읽지 않는다. "물건이 들어온 순간"에 켠다.
//    v10·v11 이 연달아 실패한 이유는 같다: "입력이 비었는가"를 알아내려고
//    확인 안 된 함수를 골라 썼다.
//      v10 isAnyInputsEmpty   — 늘 false. "비었다"가 한 번도 안 나왔다.
//      v11 getInputValueTotal — 격자 내용이 아니라 가동 중 녹인 재료량이었다.
//        로그 실측: 입력총량=0.00 인데 isAnyInputsEmpty=0 (서로 모순).
//        꺼져 있으면 안 녹으니 총량 0 -> 우리가 끔 -> 영영 안 켜짐.
//        손으로 켜도 다음 프레임에 총량 0 이라 즉시 꺼져 진행바가 안 올랐다.
//    그래서 판정을 버린다. 대신 Inventory::addItem 을 후킹해서
//    용광로 인벤토리에 물건이 실제로 들어오는 순간에만 활성 바이트를 1 로 쓴다.
//      - 매 프레임 쓰지 않는다. 사건이 있을 때 한 번만.
//      - 다 녹이면 게임이 알아서 끈다. 우리는 끄지 않는다.
//        (v11 이 끄려다 사고 났다. 끄는 일은 게임에게 맡긴다)
//      - 빈 용광로가 켜진 채 남지 않으므로 프레임 드랍의 조건이 성립 안 한다.
//    소유 객체는 Inventory +0x80 에서 얻고, 그 vtable 이 용광로와 같을 때만
//    건드린다. 오프셋이 틀렸으면 vtable 이 안 맞아 아무 일도 안 일어난다
//    — 틀려도 손해가 없는 구조다.
//    activeWhenEmpty=1 로 두면 v8 동작(매 프레임 무조건 켜둠)으로 되돌아간다.
//
//  v11 — 빈 용광로를 우리가 직접 끈다. v10 은 반쪽이었다.
//    증상: v10 을 넣어도 비어 있는데 활성화 버튼이 켜진 채 남고 프레임 드랍 재발.
//    원인: v10 은 "비면 켜지 않는다"만 했고 "끄지는" 않았다.
//      게임이 용광로를 꺼주는 것은 다 녹인 그 프레임 딱 한 번뿐인데,
//      하필 그때 우리가 "아직 안 비었다"고 판단해 켜버리면
//      게임에게 다시 끌 기회가 없다. 우리도 안 끄니 영구히 켜진 채 남는다.
//      (또 하나의 가능성: isAnyInputsEmpty 가 늘 false 를 준다.
//       v10 로그에서 "비었나=1" 을 한 번도 못 봤다. 아직 못 가렸다.)
//      어느 쪽이든 "비면 우리가 끈다"로 증상이 사라진다.
//    바뀐 것:
//      1. 판정을 getInputValueTotal() 로 바꿨다 (float, 익스포트 확인).
//         참/거짓 하나뿐인 isAnyInputsEmpty 와 달리 숫자라 화면과 대조된다.
//         isAnyInputsEmpty 는 참고용으로 같이 찍기만 한다 — 다음 판에
//         둘이 일치하는지 보고 어느 쪽이 맞는지 결론 낸다.
//      2. 비었으면 끈다. 다만 emptyFramesToOff 프레임 연속으로 0 일 때만.
//         녹는 도중 순간적으로 0 이 스칠 때 꺼버리지 않으려는 것.
//      3. 로그를 "값이 바뀔 때만" 으로. v10 은 처음 5줄만 찍어서
//         정작 비어가는 과정을 못 봤다.
//    되돌리기: activeWhenEmpty=1 이면 v8 동작(무조건 켜둠), keepActive=0 이면 관여 안 함.
//
//  v10 — 빈 용광로를 켜두지 않는다 (프레임 드랍 수정).
//    증상: 무기·방어구 용광로에 녹일 게 없는데 잡이 걸려 있으면 프레임 드랍.
//    원인 (KEP ItemFurnaceExtension.cpp 실물 확인):
//      KEP 는 FurnaceBuilding::getResourcesNeededBecauseEmpty 를 후킹하는데,
//      itemtype limit 이 WEAPON/ARMOUR 인 용광로는 특별 분기를 탄다:
//        ou->gamedata.getDataOfType(out, specialItemTypesOnly);
//      즉 "뭘 넣어야 하나" 물을 때마다 게임의 무기(또는 방어구) 레코드를
//      전수 열거해 새 목록을 만든다. 모드 300개면 수천 개고,
//      lektor 이 10칸에서 두 배씩 늘어나므로 재할당·복사가 여러 번 붙는다.
//      바닐라 용광로용 빠른 우회로(specialItemTypesOnly == ITEM)에는
//      우리 용광로가 안 걸려서 매번 비싼 길로 간다.
//      함수 이름 그대로 "비었을 때" 물어보는 것이라, v8 의 keepActive 가
//      빈 용광로를 영구 활성으로 붙들어두면서 이 질문이 멎지 않았다.
//    확인: keepActive=0 으로 두니 프레임 드랍 소멸 (인게임 대조).
//    수정: 활성 유지를 "입력이 비어 있지 않을 때만" 한다.
//      ?_NV_isAnyInputsEmpty@ProductionBuilding@@QEBA_NXZ (익스포트 확인)
//      비면 게임이 알아서 끄도록 두고, 물건이 들어오면 다시 켜준다.
//      → 편의(자동 재활성)는 유지하면서 빈 상태 질문 폭주만 없앤다.
//    activeWhenEmpty=1 로 두면 v8 동작(무조건 켜둠)으로 되돌아간다.
//
//  v9 — 섹션 매칭을 크기 + 이름(in1) 동시 조건으로.
//    v8 까지는 크기(12x7)만 봐서, 활성 모드 300여 개 중 우연히 같은 크기를
//    쓰는 다른 섹션이 있으면 그 창까지 같이 커질 수 있었다.
//    용광로 입력 섹션 이름이 in1 인 것은 실측 확정 (지식 문서 7절).
//    이름 읽기(ReadStr)는 후보(크기 일치)이거나 dumpSections 일 때만 한다
//    — v8 은 모든 섹션 생성마다 무조건 읽었다 (캐릭터 하나에 섹션 십수 개).
//    스레드 확인 로그 한 줄 추가.
//
//  v8 — 활성화 플래그는 +0x490 이 맞았다 (실측: 버튼 조작 시 00 <-> 01).
//    안 켜졌던 진짜 원인은 cfg 파싱 목록에 keepActive / dumpActive /
//    activeOffset 세 항목이 빠져 있어서, 파일에 1 이라고 적어도 무시되고
//    기본값(꺼짐)이 쓰인 것이었다. 오프셋 문제가 아니었다.
//    → 설정을 추가할 때는 파싱 목록도 함께 넣었는지 시작 로그로 확인할 것.
//
//  v7 — 덤프를 "바뀐 바이트만" 찍는 방식. 이걸로 +0x490 을 확정했다.
//
//  v5 — 활성화 플래그 위치를 다시 찾는다.
//    update 는 용광로에 대해 정상적으로 불린다 (vtable 일치 확인).
//    그런데 +0x490 을 읽으면 이미 0 이 아니어서 아무 일도 안 했다.
//    화면 버튼은 꺼져 있는데도 그렇다 = 그 자리가 활성화가 아니다.
//    dumpActive 로 주변을 찍어, 버튼을 껐다 켤 때 바뀌는 바이트를 찾는다.
//
//  v4 — v3 에서 활성화 유지가 한 줄도 안 찍혔다. 원인을 가리기 위해
//    ProductionBuilding::update 로 들어오는 vtable 을 종류별로 기록한다.
//    용광로 vtable 은 정보창 경로(실제 배치된 건물)를 우선으로 삼는다.
//
//  v3 — 용광로 활성화 유지 (keepActive, 기본 꺼짐).
//    바닐라는 다 녹이면 활성화가 꺼진다. 운반 잡이 넣는 속도가 더 빨라
//    꺼져 있으면 입력 칸이 금방 찬다 (실측: 2초마다 눌러야 했다).
//    ProductionBuilding::update 에서 active(+0x490)를 켜둔다.
//    그 함수는 제작대에서도 불리므로 vtable 로 용광로만 가려낸다.
//
//  v2 — ins/outs 는 칸수가 아니라 "화면 구역 개수"였다 (실측: 원래 값 1, 1).
//    84 를 넣으니 없는 입력 패널을 만들려 해서 Input 2 라는 빈 창이 생겼다.
//    ProductionBuilding 에 input1Panel / input2Panel 만 있으니 최대 2개다.
//    진짜 격자 크기는 Inventory::initialiseNewSection(name, w, h, ...) 이 정한다.
//    v2 는 그 호출을 전부 기록만 한다. 용광로 입력 섹션의 이름과 크기를
//    확인한 뒤에 바꾼다.
//
//  칸수가 FCS 값이 아니라는 것
//    BUILDING 레코드의 storage size 18x18 은 이 건물에서 쓰이지 않는다.
//    실제 칸수는 코드가 정한다:
//      FurnaceInventoryLayout(const std::string& title, int ins, int outs)
//    아이템 용광로는 ins=84, outs=6 으로 만들어진다 (화면 실측과 일치).
//    그래서 FCS 로는 못 바꾸고, 이 생성자를 후킹해 ins 를 갈아끼운다.
//
//  안전한 이유
//    문자열은 받은 그대로 넘기고 정수 하나만 바꾼다.
//    켄시의 std::string 은 레이아웃이 달라 우리가 만들면 위험한데,
//    이 방식은 만들 일이 없다.
//
//  [주의] 칸을 너무 늘리면 창이 화면 밖으로 나갈 수 있다.
//    켄시 인벤토리 창에는 스크롤이 없다 (보관함에서 실측 확인).
//    조금씩 올려가며 확인할 것.
//
//  빌드: VS2022 x64 Release, 배포판 KenshiLib.lib 링크
//  설치: 활성 모드 폴더에 DLL + RE_Kenshi.json {"Plugins":["FurnaceSlots.dll"]}
// ============================================================================

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <intrin.h>     // _ReturnAddress — 호출자 주소를 얻어 RVA 로 환산한다

// ---------------------------------------------------------------------------
//  설정  (cfg 에서 읽는다. v23 부터 저장하면 몇 초 안에 자동 반영.
//         단 measure / fillHook 은 훅 설치 항목이라 재시작 필요)
// ---------------------------------------------------------------------------
static int  g_inputSlots  = 0;     // 레이아웃 구역 수. 0=건드리지 않음 (칸수가 아니다)
static int  g_outputSlots = 0;     // 0=건드리지 않음
static bool g_dumpSections = false; // 인벤토리 섹션 생성을 전부 기록 (진단용)
// 아이템 용광로 입력 섹션 확장. 이 네 값은 확정된 것이므로 기본값으로 박아 둔다.
//   용광로 in1 은 12x7=84칸 (실측). 다른 생산건물의 in1 은 25칸 이하라 안 겹친다.
//   24x14=336칸까지 2K 화면에서 잘리지 않는 것을 확인했다.
static int  g_matchW      = 0;     // 이 크기로 들어오는 섹션만 바꾼다. 0=끔.
                                   // v23.3 부터 기본 0 = 훅 은퇴. 격자는 FCS 가 만든다
                                   // (BasusuFurnaceSplit: functionality flags=1 +
                                   //  storage size 24x14 — 인게임 검증 완료).
                                   // FCS 값이 깨졌을 때만 12 로 되살린다.
static int  g_matchH      = 7;
static int  g_sectionW    = 24;    // 바꿀 격자 가로. 0=그대로
static int  g_sectionH    = 14;    // 바꿀 격자 세로. 0=그대로
static bool g_dumpActive  = false; // 활성화 플래그 위치를 찾기 위한 메모리 덤프
static int  g_activeOffset = 0x490; // 활성화 플래그. 실측 확인됨 — 버튼을 껐다 켜면
                                    // 이 바이트가 00 <-> 01 로 바뀐다.
                                    // cfg 에 0 을 넣으면 이 기본값을 쓴다 (십진 1168).
static bool g_keepActive  = true;  // 활성화가 꺼지지 않게 유지 (입력이 있을 때만)
static bool g_activeWhenEmpty = false; // 1 이면 매 프레임 무조건 켜둔다 = v8 동작.
                                   // 켜지 말 것 — 빈 용광로가 영구 활성으로 남고
                                   // KEP 가 무기 전체 목록을 매번 새로 만들어
                                   // 프레임 드랍이 난다 (인게임 확인).
static bool g_debug       = false;
// --- v21 계측 (동작 변경 없음) ---
static bool g_measure  = false;   // 10초마다 측정 요약. 계측이 필요할 때만 1 로.
static int  g_listWindow = 200;    // >0 이면 목록을 이 개수짜리 창으로 잘라 돌려준다.
                                 // 0=끔(원본 그대로). 상한 512.
                                 // 200 이면 방어구(6958)만 잘리고 무기(197)는 안 걸린다.
static int  g_cutMin = 1000;     // 원본 개수가 이보다 작으면 자르지 않는다.
                                 // 싼 목록(무기 197, 석궁 112, 배낭 257 등)까지 잘라
                                 // 발견 지연만 만드는 일을 막는다. 현 환경에서 이 문턱을
                                 // 넘는 타입은 방어구(6958) 하나뿐이다 (zip 파싱 실측).
static bool g_classify  = false;  // 공급처찾기 결과가 건물인지 캐릭터인지 센다.
static bool g_fillHook = false;   // 잡 채움(Task_FillMachine) 계측 후크.
                                 // 대상 주소가 게임 1.0.65 전용 RVA 라서
                                 // (KEP ExternalFunctions 표: 0x340EB0/0x343720)
                                 // 게임 버전이 다르면 반드시 0 으로 끌 것.
static int g_lines = 0;

static void Log(const char* fmt, ...)
{
    if (g_lines >= 600) return;   // 요약이 한 판에 6줄씩 쌓인다 (v12 150 -> v13 300 -> v19 600)
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);

    FILE* f = NULL;
    if (fopen_s(&f, "FurnaceSlots.log", "a") != 0 || !f) return;
    fprintf(f, "%s\n", buf);
    fclose(f);
    ++g_lines;
}

// [일회 진단] 후크가 어느 스레드에서 불리는지. 한 줄이면 단일 스레드 =
// 락 없는 전역(g_seen, g_furnaceVtable 등)이 안전하다는 뜻이다.
// 첫 확인 이후 비용은 스레드ID 조회 + 비교 하나라 상시 켜 둬도 무해하다.
static DWORD g_tid0 = 0;
static bool  g_tidWarned = false;
static void ThreadCheck(const char* where)
{
    DWORD id = GetCurrentThreadId();
    if (!g_tid0) { g_tid0 = id; Log("스레드 확인: %s = %lu", where, (unsigned long)id); }
    else if (id != g_tid0 && !g_tidWarned)
    {
        g_tidWarned = true;
        Log("[주의] %s 가 다른 스레드 %lu 에서 불림 — 락 필요", where, (unsigned long)id);
    }
}

// ---------------------------------------------------------------------------
//  v23 cfg 핫리로드 — 게임 스레드 전용. 3초에 한 번 mtime 만 본다.
// ---------------------------------------------------------------------------
static void LoadConfig();   // 아래 설정 파일 절에 정의

static unsigned __int64 CfgMtime()
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA("FurnaceSlots.cfg", GetFileExInfoStandard, &fad))
        return 0;
    ULARGE_INTEGER u;
    u.LowPart  = fad.ftLastWriteTime.dwLowDateTime;
    u.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    return u.QuadPart;
}

static void CheckConfigReload()
{
    static unsigned __int64 lastCheck = 0;
    static unsigned __int64 lastMtime = 0;

    unsigned __int64 now = GetTickCount64();
    if (now - lastCheck < 3000) return;
    lastCheck = now;

    unsigned __int64 mt = CfgMtime();
    if (mt == 0) return;                          // 파일 없음/접근 실패 — 건드리지 않는다
    if (lastMtime == 0) { lastMtime = mt; return; }   // 첫 확인은 기준점만 잡는다
    if (mt == lastMtime) return;
    lastMtime = mt;

    LoadConfig();
    // startPlugin 의 검증 클램프를 그대로 재적용한다.
    if (g_activeOffset <= 0 || g_activeOffset > 0x800) g_activeOffset = 0x490;
    if (g_listWindow < 0 || g_listWindow > 512) g_listWindow = 0;
    if (g_cutMin < 0) g_cutMin = 1000;

    // 반영값 전문 출력 — "고쳤는데 반영 안 됨"을 로그로 즉시 가려낸다.
    Log("[cfg 재적용] keepActive=%d activeWhenEmpty=%d activeOffset=0x%X match=%dx%d->%dx%d 구역=%d/%d",
        g_keepActive ? 1 : 0, g_activeWhenEmpty ? 1 : 0, g_activeOffset,
        g_matchW, g_matchH, g_sectionW, g_sectionH,
        g_inputSlots, g_outputSlots);
    Log("[cfg 재적용] debug=%d dumpSections=%d dumpActive=%d classify=%d listWindow=%d cutMin=%d (measure/fillHook/classify 설치는 시작 시 고정)",
        g_debug ? 1 : 0, g_dumpSections ? 1 : 0, g_dumpActive ? 1 : 0,
        g_classify ? 1 : 0, g_listWindow, g_cutMin);
}

// ---------------------------------------------------------------------------
//  v21 계측 상태 — 재기만 한다. 어떤 답도 바꾸지 않는다.
// ---------------------------------------------------------------------------
static unsigned __int64 g_qpcFreq = 0;          // 초당 카운트

static unsigned __int64 QpcNow()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (unsigned __int64)li.QuadPart;
}

// 프레임 (v18 방식: ProductionBuilding::update 가 매 프레임 불리는 것을 이용)
static unsigned __int64 g_lastFrameQpc = 0;
static unsigned __int64 g_frames = 0;
static unsigned __int64 g_slowFrames = 0;       // 33ms(30fps) 초과
static unsigned __int64 g_verySlowFrames = 0;   // 100ms 초과
static unsigned __int64 g_frameMaxTick = 0;

// 목록생성 (getResourcesNeededBecauseEmpty)
static unsigned __int64 g_resCalls = 0, g_resTicks = 0;
static unsigned g_resLastCount = 0, g_resMaxCount = 0;

// 자원확인 (haveSomeResourcesFor) — v19 에서 무죄 확정. 활동량 지표로만 유지.
static unsigned __int64 g_haveCalls = 0, g_haveTicks = 0;

// 공급처찾기 (findResourceSourceForMe) — 이번 판의 핵심.
// 호출 하나하나를 "그 안에서 만들어진 목록 개수"로 버킷에 나눠 담는다.
static unsigned __int64 g_frsCalls = 0, g_frsTicks = 0, g_frsMaxTick = 0;

struct GenBucket
{
    unsigned         genCount;   // 그 호출 안에서 만들어진 목록 개수 (0 = 생성 없음)
    unsigned __int64 calls, ticks, maxTick;
};
static GenBucket g_bk[8];
static int       g_bkN = 0;
static unsigned  g_curGenCount = 0;   // fRSFM 진입 시 0. 안에서 목록생성이 나면 그 개수.
                                      // 단일 스레드 확정(지식 문서 9절)이라 전역으로 충분.
static int g_identLines = 0;          // 새 개수를 처음 볼 때 용광로 포인터를 남긴다

static GenBucket* BucketFor(unsigned genCount)
{
    for (int i = 0; i < g_bkN; ++i)
        if (g_bk[i].genCount == genCount) return &g_bk[i];
    if (g_bkN < 8)
    {
        g_bk[g_bkN].genCount = genCount;
        return &g_bk[g_bkN++];
    }
    return &g_bk[7];   // 넘치면 마지막 칸에 뭉뚱그린다
}

// 공급처 분류 (v22 관찰). 답은 건드리지 않고 세기만 한다.
static unsigned __int64 g_srcNull = 0, g_srcBuilding = 0, g_srcChar = 0,
                        g_srcItem = 0, g_srcOther = 0;
static int g_srcLines = 0;

// 목록 자르기 (v22 완화)
static unsigned __int64 g_cutCalls = 0;     // 실제로 자른 횟수
static unsigned __int64 g_cutFrom = 0;      // 마지막으로 자르기 전 개수
static int g_cutLines = 0;

// 잡 채움 (Task_FillMachine 둘). v19 미귀속 부하(프레임 시간의 40~48%)의
// 용의자였다가 "부하가 매끈하다"는 이유로 순위를 내린 자리 — 싸게 확정한다.
static unsigned __int64 g_fillACalls = 0, g_fillATicks = 0, g_fillAMax = 0;
static unsigned __int64 g_fillBCalls = 0, g_fillBTicks = 0, g_fillBMax = 0;

static int g_summaryLines = 0;

static void MaybeSummary()
{
    if (!g_measure || g_summaryLines >= 40) return;

    static unsigned __int64 lastTick = 0;
    static unsigned __int64 lastRes = 0, lastHave = 0, lastFrs = 0;
    static unsigned __int64 lastResT = 0, lastHaveT = 0, lastFrsT = 0;
    static unsigned __int64 lastFrames = 0, lastSlow = 0, lastVerySlow = 0;
    static unsigned __int64 lastFillA = 0, lastFillAT = 0, lastFillB = 0, lastFillBT = 0;
    static unsigned __int64 lastBkCalls[8] = { 0 };
    static unsigned __int64 lastBkTicks[8] = { 0 };

    unsigned __int64 now = GetTickCount64();
    if (lastTick == 0) { lastTick = now; return; }
    if (now - lastTick < 10000) return;
    lastTick = now;

    unsigned __int64 dRes  = g_resCalls  - lastRes;   lastRes  = g_resCalls;
    unsigned __int64 dHave = g_haveCalls - lastHave;  lastHave = g_haveCalls;
    unsigned __int64 dFrs  = g_frsCalls  - lastFrs;   lastFrs  = g_frsCalls;
    unsigned __int64 dResT  = g_resTicks  - lastResT;  lastResT  = g_resTicks;
    unsigned __int64 dHaveT = g_haveTicks - lastHaveT; lastHaveT = g_haveTicks;
    unsigned __int64 dFrsT  = g_frsTicks  - lastFrsT;  lastFrsT  = g_frsTicks;
    unsigned __int64 dFrames   = g_frames         - lastFrames;   lastFrames   = g_frames;
    unsigned __int64 dSlow     = g_slowFrames     - lastSlow;     lastSlow     = g_slowFrames;
    unsigned __int64 dVerySlow = g_verySlowFrames - lastVerySlow; lastVerySlow = g_verySlowFrames;
    unsigned __int64 dFA  = g_fillACalls - lastFillA;  lastFillA  = g_fillACalls;
    unsigned __int64 dFAT = g_fillATicks - lastFillAT; lastFillAT = g_fillATicks;
    unsigned __int64 dFB  = g_fillBCalls - lastFillB;  lastFillB  = g_fillBCalls;
    unsigned __int64 dFBT = g_fillBTicks - lastFillBT; lastFillBT = g_fillBTicks;

    if (dRes == 0 && dHave == 0 && dFrs == 0 && dFA == 0 && dFB == 0)
        return;   // 완전히 조용하면 줄을 낭비하지 않는다

    if (g_qpcFreq == 0) return;
    double usPerTick = 1000000.0 / (double)g_qpcFreq;

    ++g_summaryLines;
    Log("[측정 10초] 프레임 %llu개  느림(>33ms) %llu  매우느림(>100ms) %llu  최대 %.1fms",
        dFrames, dSlow, dVerySlow, (double)g_frameMaxTick * usPerTick / 1000.0);
    g_frameMaxTick = 0;

    double frsMs = (double)dFrsT * usPerTick / 1000.0;
    Log("            공급처찾기 %llu회  %.1fms (%.2f%%, 평균 %.1fus 최대 %.1fus)",
        dFrs, frsMs, frsMs / 100.0,
        dFrs ? ((double)dFrsT * usPerTick / (double)dFrs) : 0.0,
        (double)g_frsMaxTick * usPerTick);
    g_frsMaxTick = 0;

    // 버킷별 증분. 이번 판의 핵심 줄이다.
    {
        char line[440]; int n = 0;
        n += sprintf_s(line + n, (size_t)(sizeof(line) - n), "            공급처별:");
        for (int i = 0; i < g_bkN && n < 380; ++i)
        {
            unsigned __int64 dc = g_bk[i].calls - lastBkCalls[i];
            unsigned __int64 dt = g_bk[i].ticks - lastBkTicks[i];
            double mx = (double)g_bk[i].maxTick * usPerTick;
            lastBkCalls[i] = g_bk[i].calls;
            lastBkTicks[i] = g_bk[i].ticks;
            g_bk[i].maxTick = 0;
            if (dc == 0) continue;
            n += sprintf_s(line + n, (size_t)(sizeof(line) - n),
                           "  [개수%u] %llu회 평균 %.0fus 최대 %.0fus",
                           g_bk[i].genCount, dc,
                           (double)dt * usPerTick / (double)dc, mx);
        }
        Log("%s", line);
    }

    double resMs  = (double)dResT  * usPerTick / 1000.0;
    double haveMs = (double)dHaveT * usPerTick / 1000.0;
    Log("            목록생성 %llu회 (최근=%u 최대=%u) %.1fms  |  자원확인 %llu회 %.1fms",
        dRes, g_resLastCount, g_resMaxCount, resMs, dHave, haveMs);

    // v22 관찰·완화 상태
    {
        static unsigned __int64 lN = 0, lB = 0, lC = 0, lI = 0, lO = 0, lCut = 0;
        unsigned __int64 dN = g_srcNull - lN;         lN = g_srcNull;
        unsigned __int64 dB = g_srcBuilding - lB;     lB = g_srcBuilding;
        unsigned __int64 dC = g_srcChar - lC;         lC = g_srcChar;
        unsigned __int64 dI = g_srcItem - lI;         lI = g_srcItem;
        unsigned __int64 dO = g_srcOther - lO;        lO = g_srcOther;
        unsigned __int64 dCut = g_cutCalls - lCut;    lCut = g_cutCalls;
        Log("            공급처 결과: 널 %llu  건물 %llu  캐릭터 %llu  아이템 %llu  기타 %llu  |  자르기 %llu회 (창=%d, 직전 원본개수=%llu)",
            dN, dB, dC, dI, dO, dCut, g_listWindow, g_cutFrom);
    }

    double faMs = (double)dFAT * usPerTick / 1000.0;
    double fbMs = (double)dFBT * usPerTick / 1000.0;
    Log("            잡채움A %llu회 %.1fms (%.2f%%, 평균 %.1fus 최대 %.1fus)  잡채움B %llu회 %.1fms (평균 %.1fus 최대 %.1fus)",
        dFA, faMs, faMs / 100.0,
        dFA ? ((double)dFAT * usPerTick / (double)dFA) : 0.0,
        (double)g_fillAMax * usPerTick,
        dFB, fbMs,
        dFB ? ((double)dFBT * usPerTick / (double)dFB) : 0.0,
        (double)g_fillBMax * usPerTick);
    g_fillAMax = 0; g_fillBMax = 0;
}


// ---------------------------------------------------------------------------
//  KenshiLib
// ---------------------------------------------------------------------------
namespace KenshiLib
{
    enum HookStatus { HOOK_UNKNOWN };
    HookStatus AddHook(void* target, void* hook, void** original);
    __int64    GetRealAddress(void* func);
}

class FurnaceInventoryLayout;

// 입력이 비었는지 게임에 직접 물어본다.
// ?_NV_isAnyInputsEmpty@ProductionBuilding@@QEBA_NXZ  (비가상 직접호출판)
// 클래스 이름·const 여부가 하나라도 다르면 링크가 깨진다. 수정하지 말 것.
class ProductionBuilding
{
public:
    bool _NV_isAnyInputsEmpty() const;
};

// 공급처찾기가 돌려준 손잡이가 무엇을 가리키는지 묻는다.
// 아래 넷은 KenshiLib 익스포트 목록에서 확인한 것이다 (추측 금지 원칙).
//   ?isNull@hand@@QEBA_NXZ
//   ?getBuilding@hand@@QEBAPEAVBuilding@@XZ
//   ?getCharacter@hand@@QEBAPEAVCharacter@@XZ
//   ?getItem@hand@@QEBAPEAVItem@@XZ
// 클래스 이름·const 여부가 하나라도 다르면 링크가 깨진다. 수정하지 말 것.
class Building;
class Character;
class Item;
class hand
{
public:
    bool       isNull() const;
    Building*  getBuilding() const;
    Character* getCharacter() const;
    Item*      getItem() const;
};

// 입력 총량. 참/거짓이 아니라 숫자라 화면과 대조할 수 있다.
// ?getInputValueTotal@FurnaceBuilding@@QEAAMXZ  (QEAA — 비가상, const 아님)
class FurnaceBuilding
{
public:
    float getInputValueTotal();
};

// ---------------------------------------------------------------------------
//  후크
//
//  생성자를 후킹한다. 첫 인자는 this, 둘째는 제목 문자열(그대로 넘긴다),
//  셋째가 입력 칸수, 넷째가 출력 칸수다.
//  문자열은 들여다보지도 만들지도 않으므로 ABI 문제가 없다.
// ---------------------------------------------------------------------------
typedef FurnaceInventoryLayout* (*FurnaceLayoutCtorFn)(void*, const void*, int, int);
static FurnaceLayoutCtorFn origFurnaceCtor = 0;

static FurnaceInventoryLayout* hookFurnaceCtor(void* self, const void* title,
                                               int ins, int outs)
{
    int newIns  = (g_inputSlots  > 0) ? g_inputSlots  : ins;
    int newOuts = (g_outputSlots > 0) ? g_outputSlots : outs;

    if (g_debug)
        Log("용광로 레이아웃  입력 %d -> %d   출력 %d -> %d", ins, newIns, outs, newOuts);

    return origFurnaceCtor(self, title, newIns, newOuts);
}

// ---------------------------------------------------------------------------
//  섹션 생성 후크
//
//  Inventory::initialiseNewSection(name, w, h, slot, equipCb, container, enabled, limit)
//  격자 크기는 여기서 정해진다. 용광로뿐 아니라 모든 인벤토리가 이 함수를 쓴다.
//  그래서 이름과 크기를 먼저 찍어보고, 어느 것이 용광로 입력인지 확인한 뒤에 바꾼다.
//
//  이름은 std::string 이다. 켄시의 std::string 은 32바이트에 프록시가 없다:
//    +0x00 SSO버퍼(16) 또는 힙 포인터 / +0x10 길이 / +0x18 용량
//  (SleepFix 에서 캐릭터 이름으로 실측 확정한 배치. 읽기만 하므로 안전하다)
// ---------------------------------------------------------------------------
static const char* ReadStr(const void* str, char* out, int outLen)
{
    out[0] = 0;
    if (!str) { strcpy_s(out, outLen, "(널)"); return out; }
    __try
    {
        const unsigned char* s = (const unsigned char*)str;
        size_t len = *(const size_t*)(s + 0x10);
        size_t cap = *(const size_t*)(s + 0x18);
        const char* p = (cap < 16) ? (const char*)s : *(const char* const*)s;
        if (p && len > 0 && len <= 64)
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
    strcpy_s(out, outLen, "(읽기실패)");
    return out;
}

// 같은 (이름, 크기) 조합은 한 번만 남긴다. 캐릭터마다 섹션이 여러 개라 금방 넘친다.
static char g_seen[64][96];
static int  g_seenCount = 0;

static bool SeenBefore(const char* key)
{
    for (int i = 0; i < g_seenCount; ++i)
        if (strcmp(g_seen[i], key) == 0) return true;
    if (g_seenCount < 64)
    {
        strcpy_s(g_seen[g_seenCount], sizeof(g_seen[0]), key);
        ++g_seenCount;
    }
    return false;
}

typedef void* (*InitSectionFn)(void*, const void*, int, int, int, bool, bool, bool, int);
static InitSectionFn origInitSection = 0;

static void* hookInitSection(void* self, const void* name, int w, int h,
                             int slot, bool equipCb, bool container, bool enabled, int limit)
{
    if (!origInitSection) return 0;
    ThreadCheck("initialiseNewSection");

    int newW = w, newH = h;
    bool changed = false;

    // 이름은 후보(크기 일치)이거나 덤프를 켰을 때만 읽는다.
    // v8 은 모든 섹션 생성마다 무조건 읽었다 — 캐릭터 생성 한 번에
    // 섹션이 십수 개라 SEH + memcpy 를 쓸데없이 치렀다.
    bool sizeMatch = (g_matchW > 0 && g_matchH > 0 && w == g_matchW && h == g_matchH);
    char nm[80]; nm[0] = 0;
    if (sizeMatch || g_dumpSections)
        ReadStr(name, nm, sizeof(nm));

    // 크기 + 이름(in1) 동시 조건.
    // v8 까지는 크기만 봐서, 다른 모드가 우연히 12x7 섹션을 쓰면
    // 그 창까지 같이 커질 수 있었다. 용광로 입력 = in1 은 실측 확정.
    if (sizeMatch && strcmp(nm, "in1") == 0)
    {
        if (g_sectionW > 0) newW = g_sectionW;
        if (g_sectionH > 0) newH = g_sectionH;
        changed = (newW != w || newH != h);
    }

    if (g_dumpSections)
    {
        char key[96];
        sprintf_s(key, sizeof(key), "%s|%dx%d", nm, w, h);
        if (!SeenBefore(key))
            Log("섹션 생성  이름=\"%s\"  %dx%d = %d칸  슬롯타입=%d  제한=%d",
                nm, w, h, w * h, slot, limit);
    }

    if (changed)
        Log("섹션 변경  이름=\"%s\"  %dx%d -> %dx%d", nm, w, h, newW, newH);

    return origInitSection(self, name, newW, newH, slot, equipCb, container, enabled, limit);
}

// ---------------------------------------------------------------------------
//  용광로 활성화 유지
//
//  왜
//    바닐라는 안에 든 것을 다 녹이면 활성화가 자동으로 꺼진다.
//    운반 잡이 계속 장비를 넣는데 꺼져 있으면 입력 칸이 금방 찬다.
//    (무기·방어구는 칸을 많이 먹는다)
//
//  어떻게
//    active 는 FurnaceBuilding +0x490 에 있는 bool 이다.
//    다만 그 자리를 쓰려면 대상이 정말 FurnaceBuilding 인지 확인해야 한다.
//    ProductionBuilding::update 는 제작대 등 모든 생산 건물에서 불리는데,
//    다른 클래스의 +0x490 은 전혀 다른 값일 수 있다.
//
//    그래서 vtable 포인터로 가려낸다.
//    FurnaceBuilding 전용 함수(setupFromData / getGUIData / updateInputs)가
//    불릴 때 그 객체의 vtable 을 한 번 외워두고,
//    update 에서 vtable 이 같은 것만 건드린다.
// ---------------------------------------------------------------------------
static const void* g_furnaceVtable = 0;
static int g_activateCount = 0;
static int g_dumpCount = 0;

// 정보창 경로가 실제로 배치된 건물이므로 그쪽을 더 믿는다.
// 준비(setupFromData) 는 데이터를 읽으며 만드는 견본에도 불리는 것으로 보인다
// (CorpseLoot 조사에서 Y=-3, 인벤토리 널인 객체가 잡혔다).
// 그래서 정보창으로 들어온 vtable 이 있으면 그것으로 덮어쓴다.
static bool g_vtFromGui = false;

static void LearnVtable(void* self, const char* via, bool fromGui)
{
    if (!self) return;
    if (g_furnaceVtable && (g_vtFromGui || !fromGui)) return;
    __try
    {
        const void* vt = *(const void* const*)self;
        if (!vt) return;
        if (vt != g_furnaceVtable)
            if (g_debug) Log("용광로 vtable 확인 (%s) %p%s", via, vt,
                             g_furnaceVtable ? "  (이전 값을 덮어씀)" : "");
        g_furnaceVtable = vt;
        if (fromGui) g_vtFromGui = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
}

typedef void (*FurnaceSetupFn)(void*);
static FurnaceSetupFn origFurnaceSetup = 0;
static void hookFurnaceSetup(void* self)
{
    if (origFurnaceSetup) origFurnaceSetup(self);
    LearnVtable(self, "준비", false);
}

typedef void (*FurnaceGuiFn)(void*, void*, int);
static FurnaceGuiFn origFurnaceGui = 0;
static void hookFurnaceGui(void* self, void* panel, int cat)
{
    if (origFurnaceGui) origFurnaceGui(self, panel, cat);
    LearnVtable(self, "정보창", true);
}

// 진단용. update 가 실제로 불리는지, 어떤 vtable 로 들어오는지 본다.
// v3 에서 "활성화 유지" 로그가 한 줄도 안 나왔다. update 가 안 불리는 것인지
// vtable 이 안 맞는 것인지 구분이 되지 않아 양쪽을 다 찍는다.
static int g_updSeen = 0;
static const void* g_updVt[8] = { 0 };
static int g_updVtCount = 0;

typedef void (*ProdUpdateFn)(void*);
static ProdUpdateFn origProdUpdate = 0;
static void hookProdUpdate(void* self)
{
    if (origProdUpdate) origProdUpdate(self);
    if (!self) return;
    ThreadCheck("ProductionBuilding::update");
    CheckConfigReload();   // v23. 자체 스로틀(3초)이라 매 호출 비용은 u64 비교 하나다.

    // 프레임 시간 (v18 방식). 이 후크는 매 프레임 불리므로, 같은 건물이
    // 다시 올 때까지의 간격이 곧 한 프레임이다. 첫 건물 하나만 기준으로 삼는다.
    if (g_measure && g_qpcFreq > 0)
    {
        static void* frameRef = 0;
        if (!frameRef) frameRef = self;
        if (frameRef == self)
        {
            unsigned __int64 now = QpcNow();
            if (g_lastFrameQpc)
            {
                unsigned __int64 dt = now - g_lastFrameQpc;
                ++g_frames;
                if (dt > g_frameMaxTick) g_frameMaxTick = dt;
                if (dt > g_qpcFreq / 30) ++g_slowFrames;
                if (dt > g_qpcFreq / 10) ++g_verySlowFrames;
            }
            g_lastFrameQpc = now;
            MaybeSummary();
        }
    }

    __try
    {
        const void* vt = *(const void* const*)self;

        // 처음 보는 vtable 은 기록해 둔다. 생산 건물 종류마다 하나씩 나온다.
        if (g_debug && g_updVtCount < 8)
        {
            bool known = false;
            for (int i = 0; i < g_updVtCount; ++i)
                if (g_updVt[i] == vt) { known = true; break; }
            if (!known)
            {
                g_updVt[g_updVtCount++] = vt;
                Log("생산건물 갱신 들어옴  vtable=%p  %s", vt,
                    (vt == g_furnaceVtable) ? "<-- 용광로와 일치" : "(다른 건물)");
            }
        }
        ++g_updSeen;

        if (!g_furnaceVtable || vt != g_furnaceVtable) return;

        // --- 진단: 활성화 플래그가 실제로 어느 바이트인지 찾는다 ---
        // +0x490 을 읽었더니 이미 0 이 아니었다 (화면 버튼은 꺼져 있는데도).
        // 그 자리가 활성화가 아닐 수 있어, 주변을 주기적으로 찍어 비교한다.
        // 게임에서 버튼을 껐다 켜면 바뀌는 바이트가 그 자리다.
        // --- 진단: 활성화 플래그가 어느 바이트인지 찾는다 ---
        // 눈으로 hex 를 비교하는 대신, 직전 값과 달라진 자리만 짚어준다.
        // 게임에서 활성화 버튼을 껐다 켜면 그 자리가 로그에 뜬다.
        if (g_dumpActive)
        {
            static unsigned char prev[0x80];
            static bool  haveePrev = false;
            static void* prevSelf = 0;
            static int   tick = 0;

            if ((tick++ % 30) == 0)
            {
                const unsigned char* p = (const unsigned char*)self + 0x440;
                if (prevSelf != self) { haveePrev = false; prevSelf = self; }

                if (!haveePrev)
                {
                    memcpy(prev, p, 0x80);
                    haveePrev = true;
                    Log("바이트 감시 시작 %p  (+0x440 ~ +0x4BF)", self);
                }
                else
                {
                    char line[400]; int n = 0; int changed = 0;
                    for (int i = 0; i < 0x80; ++i)
                    {
                        if (prev[i] == p[i]) continue;
                        ++changed;
                        if (n < 300)
                            n += sprintf_s(line + n, sizeof(line) - n,
                                           "+0x%03X:%02X->%02X  ", 0x440 + i, prev[i], p[i]);
                    }
                    if (changed)
                    {
                        Log("바뀐 바이트 %d개  %s", changed, line);
                        memcpy(prev, p, 0x80);
                    }
                }
            }
        }

        if (!g_keepActive || g_activeOffset <= 0) return;

        if (g_activeWhenEmpty)
        {
            // v8 동작. 매 프레임 무조건 켜둔다.
            // 빈 용광로가 영구 활성으로 남아 프레임 드랍이 나므로 평소엔 쓰지 말 것.
            unsigned char* active = (unsigned char*)self + g_activeOffset;
            if (*active) return;
            *active = 1;
            ++g_activateCount;
            if (g_debug && g_activateCount <= 5)
                Log("용광로 활성화 유지 (%d번째) %p  오프셋 0x%X",
                    g_activateCount, self, g_activeOffset);
            return;
        }

        // 평소에는 여기서 아무것도 하지 않는다.
        // 켜는 일은 addItem 후크가(물건이 들어온 순간에) 하고,
        // 끄는 일은 게임에게 맡긴다. v10·v11 은 이 자리에서 상태를 읽어
        // 판단하려다 두 번 다 틀렸다.
        return;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
}

// ---------------------------------------------------------------------------
//  물건이 들어오면 켠다  (v12 의 핵심)
//
//  ?_NV_addItem@Inventory@@QEAA_NPEAVItem@@H_N1@Z
//    addItem(Item*, int quantity, bool dropOnFail, bool destroyOnFail)
//
//  모든 인벤토리가 이 함수를 쓴다. 그래서 "이 인벤토리가 용광로 것인가"를
//  가려야 하는데, Inventory +0x80 에 소유 객체가 있다 (CorpseLoot 에서 확인).
//  그 객체의 vtable 이 용광로와 같을 때만 건드린다.
//  오프셋이 틀렸으면 vtable 이 안 맞아 아무 일도 안 일어난다 — 틀려도 손해가 없다.
//
//  매 프레임이 아니라 물건이 들어온 그 순간에만 쓴다.
//  끄는 것은 게임에게 맡긴다. 다 녹이면 게임이 알아서 끄고, 그대로 둔다.
// ---------------------------------------------------------------------------
typedef bool (*AddItemFn)(void*, void*, int, bool, bool);
static AddItemFn origAddItem = 0;
static int g_feedSeen = 0;      // 용광로에 물건이 들어온 횟수
static int g_feedOther = 0;     // 용광로가 아닌 인벤토리 (진단용)

static void MaybeWakeFurnace(void* inv)
{
    if (!g_keepActive || g_activeWhenEmpty) return;   // v8 모드면 여기서 안 한다
    if (g_activeOffset <= 0 || !g_furnaceVtable) return;

    __try
    {
        void* owner = *(void**)((unsigned char*)inv + 0x80);
        if (!owner) { ++g_feedOther; return; }

        const void* vt = *(const void* const*)owner;
        if (vt != g_furnaceVtable) { ++g_feedOther; return; }

        unsigned char* active = (unsigned char*)owner + g_activeOffset;
        ++g_feedSeen;

        if (!*active)
        {
            *active = 1;
            ++g_activateCount;
            if (g_debug && g_activateCount <= 20)
                Log("용광로 켬 (물건 투입) %p  누적투입=%d  활성=%d",
                    owner, g_feedSeen, (int)*active);
        }
        else if (g_debug && g_feedSeen <= 20)
        {
            // 이미 켜진 채로 계속 들어온다 = 가동 중에도 투입이 된다는 뜻.
            // "녹이는 중엔 못 넣는다"가 사실인지 이 줄로 갈린다.
            Log("용광로 투입 (이미 활성) %p  누적투입=%d", owner, g_feedSeen);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
}

static bool hookAddItem(void* inv, void* item, int qty, bool dropOnFail, bool destroyOnFail)
{
    if (!origAddItem) return false;
    bool r = origAddItem(inv, item, qty, dropOnFail, destroyOnFail);
    if (r && inv) MaybeWakeFurnace(inv);
    return r;
}

// ---------------------------------------------------------------------------
//  v21 계측 후크 — 재기만 한다. 어떤 답도 바꾸지 않는다.
//
//  후크 순서: KEP 가 먼저 걸고 우리가 나중에 걸면 우리가 바깥이 된다
//  (v13 실측 — 목록생성 후크에서 KEP 가 채운 개수 6958 을 그대로 읽었다).
//  즉 우리 시계는 KEP 후크 + 바닐라를 함께 잰다. 그게 원하는 것이다.
// ---------------------------------------------------------------------------

// --- v22 완화: 목록을 K개짜리 창으로 잘라 돌려준다 ---
//  lektor 배치: count +0x8 / maxSize +0xC / stuff +0x10 (v13 실측).
//  하는 일은 둘뿐이다.
//    1. 버퍼 안에서 창에 해당하는 포인터 K개를 앞으로 당긴다 (memcpy, 크기 불변)
//    2. count 를 K 로 낮춘다
//  버퍼를 키우지 않으므로 힙 주인이 바뀌지 않는다 (v14 사고의 조건이 아니다).
//  담긴 것은 GameData* 원시 포인터라 우리가 덮어써도 소유권 문제가 없다.
//  앞뒤가 안 맞는 값이 보이면 아무것도 하지 않고 물러난다.
static void*    g_rotWho[8] = { 0 };
static unsigned g_rotOff[8] = { 0 };

static void TruncateList(void* self, void* outLektor, unsigned n)
{
    unsigned K = (unsigned)g_listWindow;
    if (K == 0 || K >= n) return;              // 창보다 작은 목록은 그대로 둔다
    if (n < (unsigned)g_cutMin) return;        // 싼 목록은 자를 이유가 없다 (v22.1)

    unsigned char* L = (unsigned char*)outLektor;
    unsigned maxSize = *(const unsigned*)(L + 0xC);
    void** stuff = *(void***)(L + 0x10);
    if (!stuff || maxSize < n) return;         // 앞뒤가 안 맞는다 — 손대지 않는다

    int slot = -1, freeSlot = -1;
    for (int i = 0; i < 8; ++i)
    {
        if (g_rotWho[i] == self) { slot = i; break; }
        if (!g_rotWho[i] && freeSlot < 0) freeSlot = i;
    }
    if (slot < 0)
    {
        if (freeSlot < 0) return;              // 용광로가 8개를 넘으면 나머지는 안 자른다
        slot = freeSlot; g_rotWho[slot] = self; g_rotOff[slot] = 0;
    }

    unsigned off = g_rotOff[slot] % n;
    static void* tmp[512];
    for (unsigned i = 0; i < K; ++i) tmp[i] = stuff[(off + i) % n];
    memcpy(stuff, tmp, (size_t)K * sizeof(void*));
    *(unsigned*)(L + 0x8) = K;

    g_rotOff[slot] = (off + K) % n;
    ++g_cutCalls; g_cutFrom = n;
    if (g_cutLines < 5)
    {
        ++g_cutLines;
        Log("[자르기] 용광로=%p  %u -> %u개  (창 시작 %u)", self, n, K, off);
    }
}

// 목록생성. 개수를 읽어 지금 도는 공급처찾기의 버킷 키로 넘긴다.
typedef void (*ResNeededFn)(void*, void*);
static ResNeededFn origResEmpty = 0;

static void hookResEmpty(void* self, void* outLektor)
{
    if (!origResEmpty) return;

    unsigned __int64 t0 = QpcNow();
    origResEmpty(self, outLektor);
    unsigned __int64 dt = QpcNow() - t0;
    ++g_resCalls;
    g_resTicks += dt;

    // lektor: count +0x8 (v13 실측 배치. 읽기만 한다)
    __try
    {
        unsigned n = *(const unsigned*)((const unsigned char*)outLektor + 0x8);
        if (n <= 1000000u)
        {
            g_resLastCount = n;
            if (n > g_resMaxCount) g_resMaxCount = n;
            g_curGenCount = n;   // 버킷 키

            // 새 개수는 처음 한 번 용광로 포인터와 함께 남긴다 —
            // 197 이 어느 건물인지 인게임에서 대조할 수 있게.
            if (g_measure && g_identLines < 6)
            {
                bool seen = false;
                for (int i = 0; i < g_bkN; ++i)
                    if (g_bk[i].genCount == n) { seen = true; break; }
                if (!seen)
                {
                    ++g_identLines;
                    Log("[식별] 목록 개수=%u 첫 관측  용광로=%p", n, self);
                }
            }

            // 자르기는 버킷 키를 정한 뒤에 한다. g_curGenCount 는 자르기 **전**
            // 개수라서 [개수6958] 평균이 자르기 전후로 그대로 비교된다.
            if (g_listWindow > 0) TruncateList(self, outLektor, n);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { }
}

// 자원확인. v19 무죄 확정 — 활동량 지표로만 센다.
typedef bool (*HaveResFn)(void*, const void*, const void*);
static HaveResFn origHaveRes = 0;

static bool hookHaveRes(void* self, const void* subject, const void* pos)
{
    if (!origHaveRes) return false;
    unsigned __int64 t0 = QpcNow();
    bool r = origHaveRes(self, subject, pos);
    g_haveTicks += QpcNow() - t0;
    ++g_haveCalls;
    return r;
}

// 공급처찾기. 이번 판의 핵심 — 안에서 만들어진 목록 개수로 버킷을 나눈다.
typedef float (*FindResSrcFn)(void*, const void*, void*, bool);
static FindResSrcFn origFindResSrc = 0;

static float hookFindResSrc(void* self, const void* subject, void* out, bool justAsking)
{
    if (!origFindResSrc) return 0.0f;

    g_curGenCount = 0;                    // 이 호출 안에서 목록생성이 나면 채워진다
    unsigned __int64 t0 = QpcNow();
    float r = origFindResSrc(self, subject, out, justAsking);
    unsigned __int64 dt = QpcNow() - t0;

    ++g_frsCalls;
    g_frsTicks += dt;
    if (dt > g_frsMaxTick) g_frsMaxTick = dt;

    GenBucket* b = BucketFor(g_curGenCount);
    ++b->calls;
    b->ticks += dt;
    if (dt > b->maxTick) b->maxTick = dt;

    // v22 관찰: 돌려준 공급처가 무엇인가. 세기만 한다.
    // 사용자 관찰(여럿이 아이템 주운 사람을 따라감)이 사실인지 여기서 갈린다.
    if (g_classify && out)
    {
        __try
        {
            const hand* h = (const hand*)out;
            if (h->isNull()) ++g_srcNull;
            else
            {
                Character* c = h->getCharacter();
                if (c)
                {
                    ++g_srcChar;
                    if (g_srcLines < 10)
                    {
                        ++g_srcLines;
                        Log("[공급처] 캐릭터=%p  점수=%.2f  요청AI=%p", (void*)c, r, self);
                    }
                }
                else if (h->getBuilding()) ++g_srcBuilding;
                else if (h->getItem())     ++g_srcItem;
                else                       ++g_srcOther;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { }
    }

    MaybeSummary();
    return r;
}

// 잡 채움 둘. 시그니처는 KEP ItemFurnaceExtension.cpp 원문 그대로:
//   int fn(Task_FillMachine*, StorageBuilding*, Inventory*)
// 답은 건드리지 않는다.
typedef int (*FillFn)(void*, void*, void*);
static FillFn origFillA = 0;
static FillFn origFillB = 0;

static int hookFillA(void* self, void* storage, void* inv)
{
    if (!origFillA) return 0;
    ThreadCheck("잡채움A");
    unsigned __int64 t0 = QpcNow();
    int r = origFillA(self, storage, inv);
    unsigned __int64 dt = QpcNow() - t0;
    ++g_fillACalls;
    g_fillATicks += dt;
    if (dt > g_fillAMax) g_fillAMax = dt;
    MaybeSummary();
    return r;
}

static int hookFillB(void* self, void* storage, void* inv)
{
    if (!origFillB) return 0;
    unsigned __int64 t0 = QpcNow();
    int r = origFillB(self, storage, inv);
    unsigned __int64 dt = QpcNow() - t0;
    ++g_fillBCalls;
    g_fillBTicks += dt;
    if (dt > g_fillBMax) g_fillBMax = dt;
    MaybeSummary();
    return r;
}

// ---------------------------------------------------------------------------
//  설정 파일
//  파일이 없을 때만 만든다. 이후에는 덮어쓰지 않으므로
//  주석을 달거나 줄 순서를 바꿔도 유지된다.
// ---------------------------------------------------------------------------
static void WriteConfig();

static void LoadConfig()
{
    FILE* f = NULL;
    if (fopen_s(&f, "FurnaceSlots.cfg", "r") != 0 || !f)
    {
        WriteConfig();
        return;
    }

    char line[256] = { 0 };
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        char key[64] = { 0 }; float val = 0.0f;
        if (sscanf_s(line, "%63[^=]=%f", key, (unsigned)sizeof(key), &val) != 2) continue;

        if      (strcmp(key, "inputSlots")   == 0) g_inputSlots   = (int)val;
        else if (strcmp(key, "outputSlots")  == 0) g_outputSlots  = (int)val;
        else if (strcmp(key, "matchWidth")   == 0) g_matchW       = (int)val;
        else if (strcmp(key, "matchHeight")  == 0) g_matchH       = (int)val;
        else if (strcmp(key, "newWidth")     == 0) g_sectionW     = (int)val;
        else if (strcmp(key, "newHeight")    == 0) g_sectionH     = (int)val;
        else if (strcmp(key, "dumpSections") == 0) g_dumpSections = (val != 0.0f);
        else if (strcmp(key, "keepActive")   == 0) g_keepActive   = (val != 0.0f);
        else if (strcmp(key, "activeWhenEmpty") == 0) g_activeWhenEmpty = (val != 0.0f);
        else if (strcmp(key, "dumpActive")   == 0) g_dumpActive   = (val != 0.0f);
        else if (strcmp(key, "activeOffset") == 0) g_activeOffset = (int)val;
        else if (strcmp(key, "debug")        == 0) g_debug        = (val != 0.0f);
        else if (strcmp(key, "measure")      == 0) g_measure      = (val != 0.0f);
        else if (strcmp(key, "fillHook")     == 0) g_fillHook     = (val != 0.0f);
        else if (strcmp(key, "listWindow")   == 0) g_listWindow   = (int)val;
        else if (strcmp(key, "cutMin")       == 0) g_cutMin       = (int)val;
        else if (strcmp(key, "classify")     == 0) g_classify     = (val != 0.0f);
    }
    fclose(f);

    // 터무니없는 값은 막는다. 음수나 거대한 값이 들어가면 게임이 어떻게 될지 모른다.
    // 터무니없는 값은 막는다.
    if (g_inputSlots  < 0 || g_inputSlots  > 2) g_inputSlots  = 0;   // 구역은 최대 2개
    if (g_outputSlots < 0 || g_outputSlots > 2) g_outputSlots = 0;
    if (g_sectionW < 0 || g_sectionW > 40) g_sectionW = 0;
    if (g_sectionH < 0 || g_sectionH > 40) g_sectionH = 0;
    // 오프셋은 객체 크기 안이어야 한다. 벗어나면 끈다.
    // 0 이하이거나 객체 범위를 벗어나면 확인된 기본값으로 되돌린다.
    if (g_activeOffset <= 0 || g_activeOffset > 0x800) g_activeOffset = 0x490;
    // 창은 정적 버퍼(512칸) 안이어야 한다. 벗어나면 끈다.
    if (g_listWindow < 0 || g_listWindow > 512) g_listWindow = 0;
    if (g_cutMin < 0) g_cutMin = 1000;
    // 너무 길게 잡으면 일꾼이 옛 답을 오래 믿는다. 5초를 넘기지 않는다.
}

static void WriteConfig()
{
    FILE* f = NULL;
    if (fopen_s(&f, "FurnaceSlots.cfg", "w") != 0 || !f) return;
    fprintf(f, "# FurnaceSlots 설정 - 저장하면 몇 초 안에 자동 반영된다 (게임 켠 채로).\n");
    fprintf(f, "# 단 measure / fillHook 은 훅 설치 항목이라 게임 재시작이 필요하다.\n\n");

    fprintf(f, "# --- 진단 ---\n");
    fprintf(f, "dumpSections=%d    # 인벤토리 섹션이 만들어질 때 이름과 크기를 기록\n", g_dumpSections ? 1 : 0);
    fprintf(f, "                  # 용광로 입력 섹션이 어떤 이름·크기인지 여기서 확인한다\n");
    fprintf(f, "debug=%d           # 로그 켜기\n", g_debug ? 1 : 0);
    fprintf(f, "measure=%d         # v21 계측. 10초마다 요약 한 묶음. 측정이 끝나면 0.\n", g_measure ? 1 : 0);
    fprintf(f, "fillHook=%d        # 잡 채움(Task_FillMachine) 계측. 게임 1.0.65 전용\n", g_fillHook ? 1 : 0);
    fprintf(f, "                  # RVA(0x340EB0/0x343720)라 버전이 다르면 반드시 0.\n");
    fprintf(f, "classify=%d        # 공급처찾기 결과가 건물인지 캐릭터인지 센다 (관찰만).\n", g_classify ? 1 : 0);
    fprintf(f, "                  # 훅 설치 항목 — 시작 시 measure 나 classify 가 켜져\n");
    fprintf(f, "                  # 있어야 하고, 켜고 끄는 것은 재시작이 필요하다.\n");
    fprintf(f, "listWindow=%d      # 프레임 드랍 완화. 0=끔. 200 권장, 상한 512.\n", g_listWindow);
    fprintf(f, "                  # 용광로가 \"뭘 넣어야 하나\" 물을 때 돌려주는 목록을\n");
    fprintf(f, "                  # 이 개수로 잘라 준다. 호출마다 창을 밀어 전체를\n");
    fprintf(f, "                  # 순회하므로 빠지는 품목은 없고, 발견이 몇 틱 늦어질 뿐이다.\n");
    fprintf(f, "                  # 비용은 목록 개수에 비례한다 (항목당 ~2.4us, 실측).\n");
    fprintf(f, "                  # 방어구 용광로는 6958종이라 한 번에 16ms 를 먹는다\n");
    fprintf(f, "                  # (무기는 197종뿐이라 원래 싸다 - zip 파싱 실측).\n");
    fprintf(f, "cutMin=%d       # 원본이 이 개수보다 작으면 자르지 않는다.\n", g_cutMin);
    fprintf(f, "                  # 싼 목록의 발견 지연을 막는 가드. 기본 1000.\n");
    fprintf(f, "                  # 자르기는 운반 완주까지 인게임 검증 완료 (v22).\n");
    fprintf(f, "# --- 활성화 유지 ---\n");
    fprintf(f, "keepActive=%d      # 용광로 활성화가 꺼지지 않게 유지한다.\n", g_keepActive ? 1 : 0);
    fprintf(f, "                  # 바닐라는 다 녹이면 자동으로 꺼진다. 운반 잡이 넣는\n");
    fprintf(f, "                  # 속도가 더 빨라서, 꺼져 있으면 입력 칸이 금방 찬다.\n");
    fprintf(f, "                  # 메모리를 직접 쓰므로 이상하면 0 으로 되돌릴 것.\n");
    fprintf(f, "activeWhenEmpty=%d # 빈 용광로도 켜둔다 (v8 동작). 켜지 말 것 —\n", g_activeWhenEmpty ? 1 : 0);
    fprintf(f, "                  # KEP 가 빈 용광로마다 무기 전체 목록을 매번 새로\n");
    fprintf(f, "                  # 만들어 프레임 드랍이 난다 (인게임 확인).\n");
    fprintf(f, "                  # 0 이면 물건이 있을 때만 켜고, 비면 꺼준다.\n");
    fprintf(f, "activeOffset=%d  # 활성화 플래그 위치. %d = 0x%X (십진으로 적는다).\n",
            g_activeOffset, g_activeOffset, g_activeOffset);
    fprintf(f, "                  # 0 을 넣으면 확인된 기본값 0x490 을 쓴다.\n");
    fprintf(f, "dumpActive=%d      # 1 이면 용광로 주변 메모리를 주기적으로 찍는다.\n", g_dumpActive ? 1 : 0);
    fprintf(f, "                  # 게임에서 활성화 버튼을 껐다 켜고, 바뀌는\n");
    fprintf(f, "                  # 바이트를 찾아 activeOffset 에 넣는다.\n\n");

    fprintf(f, "# --- 격자 크기 바꾸기 ---\n");
    fprintf(f, "# 지정한 크기로 만들어지는 섹션만 골라 바꾼다.\n");
    fprintf(f, "# 먼저 dumpSections 로그를 보고 용광로 입력 섹션의 크기를 확인할 것.\n");
    fprintf(f, "matchWidth=%d      # 이 크기로 들어오는 섹션만 대상. 0=끔.\n", g_matchW);
    fprintf(f, "                  # 기본 0 = 격자는 FCS(flags=1 + storage size)가 만든다.\n");
    fprintf(f, "                  # FCS 쪽 값이 깨졌을 때만 12/7/24/14 로 되살릴 것.\n");
    fprintf(f, "matchHeight=%d\n", g_matchH);
    fprintf(f, "newWidth=%d        # 바꿀 크기. 0=그대로\n", g_sectionW);
    fprintf(f, "newHeight=%d\n", g_sectionH);
    fprintf(f, "# 켄시 인벤토리 창에는 스크롤이 없다. 너무 키우면 화면 밖으로 잘린다.\n\n");

    fprintf(f, "# --- 레이아웃 구역 수 (칸수가 아니다. 건드리지 말 것) ---\n");
    fprintf(f, "# ins/outs 는 화면 구역 개수이며 최대 2 다. 늘리면 빈 패널이 생긴다.\n");
    fprintf(f, "inputSlots=%d      # 0=건드리지 않음\n", g_inputSlots);
    fprintf(f, "outputSlots=%d     # 0=건드리지 않음\n", g_outputSlots);
    fclose(f);
}

// ---------------------------------------------------------------------------
//  설치
// ---------------------------------------------------------------------------
__declspec(dllexport) void startPlugin();

void startPlugin()
{
    LoadConfig();

    Log("FurnaceSlots 시작  keepActive=%d  activeWhenEmpty=%d  activeOffset=0x%X  match=%dx%d -> %dx%d",
        g_keepActive ? 1 : 0, g_activeWhenEmpty ? 1 : 0,
        g_activeOffset, g_matchW, g_matchH, g_sectionW, g_sectionH);
    Log("  dumpSections=%d  dumpActive=%d  debug=%d  구역(ins/outs)=%d/%d",
        g_dumpSections ? 1 : 0, g_dumpActive ? 1 : 0, g_debug ? 1 : 0,
        g_inputSlots, g_outputSlots);
    Log("  measure=%d  fillHook=%d  classify=%d  listWindow=%d  cutMin=%d%s",
        g_measure ? 1 : 0, g_fillHook ? 1 : 0, g_classify ? 1 : 0,
        g_listWindow, g_cutMin,
        g_listWindow > 0 ? "  <-- 목록 자르기 켜짐" : "  (자르기 꺼짐)");

    QueryPerformanceFrequency((LARGE_INTEGER*)&g_qpcFreq);
    Log("  시간측정 준비: 초당 %llu 카운트", g_qpcFreq);

    // 생성자는 멤버 함수 포인터를 만들 수 없으므로 맹글링 이름으로 직접 찾는다.
    static const char* CTOR =
        "??0FurnaceInventoryLayout@@QEAA@AEBV?$basic_string@DU?$char_traits@D@std@@"
        "V?$allocator@D@2@@std@@HH@Z";

    HMODULE lib = GetModuleHandleA("KenshiLib.dll");
    void* stub = lib ? (void*)GetProcAddress(lib, CTOR) : 0;
    if (!stub)
    {
        Log("  FurnaceInventoryLayout 생성자를 찾지 못했다. 아무 일도 하지 않는다.");
        return;
    }

    void* target = (void*)KenshiLib::GetRealAddress(stub);
    KenshiLib::HookStatus st =
        KenshiLib::AddHook(target, (void*)&hookFurnaceCtor, (void**)&origFurnaceCtor);
    Log("  용광로레이아웃  addr=%p  status=%d  orig=%p",
        target, (int)st, (void*)origFurnaceCtor);
    // 섹션 생성 후크. 격자 크기는 여기서 정해진다.
    static const char* INIT_SECTION =
        "?_NV_initialiseNewSection@Inventory@@QEAAPEAVInventorySection@@AEBV?$basic_string"
        "@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@HHW4AttachSlot@@_N22H@Z";
    void* s2 = lib ? (void*)GetProcAddress(lib, INIT_SECTION) : 0;
    if (!s2)
    {
        Log("  initialiseNewSection 을 찾지 못했다.");
    }
    else
    {
        void* t2 = (void*)KenshiLib::GetRealAddress(s2);
        KenshiLib::HookStatus st2 =
            KenshiLib::AddHook(t2, (void*)&hookInitSection, (void**)&origInitSection);
        Log("  섹션생성        addr=%p  status=%d  orig=%p",
            t2, (int)st2, (void*)origInitSection);
    }

    // 물건 투입 감지. 이게 v12 의 활성화 경로다.
    {
        static const char* ADD_ITEM = "?_NV_addItem@Inventory@@QEAA_NPEAVItem@@H_N1@Z";
        void* s3 = lib ? (void*)GetProcAddress(lib, ADD_ITEM) : 0;
        if (!s3)
        {
            Log("  Inventory::addItem 을 찾지 못했다 — 자동 활성화가 동작하지 않는다.");
        }
        else
        {
            void* t3 = (void*)KenshiLib::GetRealAddress(s3);
            Log("  물건투입        addr=%p  status=%d  orig=%p", t3,
                (int)KenshiLib::AddHook(t3, (void*)&hookAddItem, (void**)&origAddItem),
                (void*)origAddItem);
        }
    }

    // v23.1 설치 조건 분리 — 목록생성 후크는 자르기(listWindow)의 필수 경로라
    // **무조건** 설치한다. v21~v23 은 이걸 measure=1 에 묶어 두어서, 계측을
    // 끄는 순간 자르기가 소리 없이 죽는 함정이 있었다.
    {
        static const char* RES_EMPTY =
            "?_NV_getResourcesNeededBecauseEmpty@FurnaceBuilding@@QEAAXAEAV?$lektor@PEAVGameData@@@@@Z";
        static const char* HAVE_RES =
            "?haveSomeResourcesFor@AI@@QEAA_NAEBVhand@@AEBVVector3@Ogre@@@Z";
        static const char* FIND_SRC =
            "?findResourceSourceForMe@AI@@QEAAMAEBVhand@@AEAV2@_N@Z";

        void* s4 = lib ? (void*)GetProcAddress(lib, RES_EMPTY) : 0;
        void* s5 = lib ? (void*)GetProcAddress(lib, HAVE_RES)  : 0;
        void* s6 = lib ? (void*)GetProcAddress(lib, FIND_SRC)  : 0;

        // 목록생성: 자르기 + (measure 시) 버킷 키. 항상.
        if (!s4) Log("  목록생성 함수를 찾지 못했다 — 자르기·버킷 불가.");
        else
        {
            void* t = (void*)KenshiLib::GetRealAddress(s4);
            Log("  목록생성(자르기) addr=%p  status=%d", t,
                (int)KenshiLib::AddHook(t, (void*)&hookResEmpty, (void**)&origResEmpty));
        }
        // 자원확인: 계측 전용.
        if (g_measure)
        {
            if (!s5) Log("  자원확인 함수를 찾지 못했다.");
            else
            {
                void* t = (void*)KenshiLib::GetRealAddress(s5);
                Log("  자원확인측정    addr=%p  status=%d", t,
                    (int)KenshiLib::AddHook(t, (void*)&hookHaveRes, (void**)&origHaveRes));
            }
        }
        // 공급처찾기: 계측(버킷) 또는 분류(classify)가 켜져 있을 때.
        if (g_measure || g_classify)
        {
            if (!s6) Log("  공급처찾기 함수를 찾지 못했다 — 버킷·분류 불가.");
            else
            {
                void* t = (void*)KenshiLib::GetRealAddress(s6);
                Log("  공급처찾기      addr=%p  status=%d", t,
                    (int)KenshiLib::AddHook(t, (void*)&hookFindResSrc, (void**)&origFindResSrc));
            }
        }
    }

    // 잡 채움 둘 (Task_FillMachine). 익스포트가 아니라서 KEP 의 1.0.65 RVA 표로
    // 찾는다 (KEP_ExternalFunctions.cpp). KEP 가 이미 이 주소를 후킹하고 있고
    // 게임이 정상 작동하므로 주소는 실전에서 검증돼 있다. 나중에 건 후크가
    // 바깥이 된다 (목록생성에서 실측 확인된 순서) — 우리 시계가 KEP 루프를 포함한다.
    // 게임 버전이 1.0.65 가 아니면 엉뚱한 자리다. fillHook=0 으로 끌 것.
    if (g_fillHook)
    {
        unsigned char* base = (unsigned char*)GetModuleHandleA(NULL);
        void* tA = base + 0x340EB0;
        void* tB = base + 0x343720;
        Log("  잡채움A측정     addr=%p  status=%d", tA,
            (int)KenshiLib::AddHook(tA, (void*)&hookFillA, (void**)&origFillA));
        Log("  잡채움B측정     addr=%p  status=%d", tB,
            (int)KenshiLib::AddHook(tB, (void*)&hookFillB, (void**)&origFillB));
    }

    // 활성화 유지용. FurnaceBuilding 을 가려내려면 vtable 을 알아야 한다.
    {
        static const char* F_SETUP = "?_NV_setupFromData@FurnaceBuilding@@QEAAXXZ";
        static const char* F_GUI   = "?_NV_getGUIData@FurnaceBuilding@@QEAAXPEAVDatapanelGUI@@H@Z";
        static const char* P_UPD   = "?_NV_update@ProductionBuilding@@QEAAXXZ";

        void* h1 = lib ? (void*)GetProcAddress(lib, F_SETUP) : 0;
        void* h2 = lib ? (void*)GetProcAddress(lib, F_GUI)   : 0;
        void* h3 = lib ? (void*)GetProcAddress(lib, P_UPD)   : 0;

        if (h1)
        {
            void* t = (void*)KenshiLib::GetRealAddress(h1);
            Log("  용광로준비      addr=%p  status=%d", t,
                (int)KenshiLib::AddHook(t, (void*)&hookFurnaceSetup, (void**)&origFurnaceSetup));
        }
        if (h2)
        {
            void* t = (void*)KenshiLib::GetRealAddress(h2);
            Log("  용광로정보창    addr=%p  status=%d", t,
                (int)KenshiLib::AddHook(t, (void*)&hookFurnaceGui, (void**)&origFurnaceGui));
        }
        if (h3)
        {
            void* t = (void*)KenshiLib::GetRealAddress(h3);
            Log("  생산건물갱신    addr=%p  status=%d", t,
                (int)KenshiLib::AddHook(t, (void*)&hookProdUpdate, (void**)&origProdUpdate));
        }
    }

    Log("설치 완료. status 가 0 이 아니면 후킹 실패다.");
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
