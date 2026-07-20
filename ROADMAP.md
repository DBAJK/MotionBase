# MotionBase 개발 로드맵

SporTrack : Baseball — 비착용형 XR 야구 콘텐츠. 정본 브리프: [history/CLAUDE_1.md](history/CLAUDE_1.md).
원칙: **입력 스텁 우선**(Vive/LiDAR 없이 로직 먼저) · **한 단계 = 실행 가능한 상태**.

범례: `[x]` 완료 · `[~]` 뼈대만(스텁) · `[ ]` 미착수

---

## Phase 0 — 기반 (거의 완료)

- [x] UE5 C++ 스캐폴드 (Actor/계산/점수/결과 계층 분리)
- [x] 빌드 파이프라인 + 엔진 연결 (UE 5.8, GUID 연동)
- [x] 데이터 계약 구조체 (`FSwingSample`·`FSwingMetrics`·`FScoreResult`·열거형)
- [x] 계산 계층 `USwingAnalyzer` (속도 미분·피크·컨택 감지)
- [x] 점수 계층 `UScoringService` (3축 가중합 + 세션 표준편차)
- [x] 화면 테스트 하네스 `ASwingTestPawn` (Vive 없이 스윙→점수 검증, 공 연출)
- [ ] 계산 계층 **자동 단위 테스트** (UE Automation `IMPLEMENT_SIMPLE_AUTOMATION_TEST`)
- [ ] `USwingAnalyzer` 스윙 평면각 계산 (현재 TODO=0)
- [ ] **git 초기 커밋** (현재 전부 untracked)

## Phase 1 — 타격 MVP (화면만, Vive 불필요) ★ 다음 목표

- [x] `APitchingZone` — 마운드→플레이트 실제 비행, 자동 연속 투구, 타구 연출
- [x] 구종별 궤적 (직구 직선 / 변화구 휨) + 코스 분산 + 구속 범위
- [x] 타이밍 판정 루프 — 키 입력 시각 vs 공 도달 시각으로 실제 타이밍 채점
- [ ] 난이도 **동적 조정** (기록 기반으로 구속·변화구 비율 자동 상승)
- [ ] 스트라이크/볼 판정 (현재는 코스만 분산, 존 판정 없음)
- [ ] 가상 타구 결과 모델 (비거리/방향 → 효율 점수 반영)
- [ ] UMG HUD (디버그 텍스트 → 정식 점수/피드백 UI)

## Phase 2 — Vive 연동 (하드웨어)

- [ ] SteamVR 설치 + 룸 셋업 + **OpenXR 런타임 지정** ← 현재 유일한 블로커
- [ ] VR 폰 (카메라 + 양손 MotionController)
- [x] `UViveMotionInputProvider` — BatTip 월드좌표 미분 (폐기 API 미사용)
- [x] `ABat` provider 경유로 리팩터링 — `InputSource` 설정으로 Mock/Vive 전환
- [ ] 트리거 입력 배선 (Enhanced Input → `BeginSwingCapture`/`End` 호출) ← 아직 호출부 없음
- [ ] BatTip 실측 속도 검증 (실기 연결 후)
- [ ] 비착용형(null-driver) 구동 실험 (헤드셋 없이 base station만)
- [ ] **실측 캘리브레이션** (σt, d_max, v_min, v_target — 하드코딩 금지)

## Phase 3 — 반응속도 + 로컬 저장

- [ ] 반응속도 모드 (바닥 타깃 점등/밟기, 콤보) — 입력 스텁(키보드)부터
- [~] `UMotionBaseSaveGame` — 저장/로드 실배선 (구조체만 있음)
- [~] `UModeManager` — 모드 전환 + 결과 누적 → 저장 연결
- [ ] 개인 기준선 데이터 (신체 인식 셋업 결과)

## Phase 4 — LiDAR 모드 (뉴작 장비)

- [x] 입력 **추상화 인터페이스** `UMotionInputProvider` + `UMockMotionInputProvider` (시간 기반 재생)
- [ ] `ULiDARMotionInputProvider` 구현 (뉴작 SDK 확인 후)
- [ ] 신체 인식(셋업) — 위치·키·범위 스캔 → 난이도 기준선
- [ ] 수비 훈련 — 전신 자세(좌우 이동·점프·숙이기)
- [ ] 베이스 러닝 — 위치·이동·타이밍(세이프/아웃)
- [ ] 기능성 피트니스 — 관절각·반복 카운트
- [ ] ⚠️ 뉴작 LiDAR SDK/데이터 포맷 확인 후 실장

## Phase 5 — 생성형 AI 피드백

- [~] `UAIFeedbackService` — HTTP 실구현 (async, 프롬프트만 있음)
- [ ] `Config/Secrets.ini` API 키 분리 (gitignore)
- [ ] 누적 데이터 → 프롬프트 → 코칭 텍스트 표시

## Phase 6 — 확장

- [ ] 멀티플레이 (1~10명)
- [ ] 랭킹
- [ ] 교육 커리큘럼 모드 (2023 개정 체육)

---

## 상시(Cross-cutting) / 착수 전 확인

- [ ] **뉴작 문의**: 요구 UE 버전 · SporTrack 연동 스펙 · LiDAR SDK · 멀티플레이 프레임워크 제공 여부
- [ ] Vive vs 비착용형 정체성 — 심사 설명 논리 준비
- [ ] 점수 기준 상수 실측 캘리브레이션 (`FScoringConfig`)
- [ ] `EngineAssociation` — 뉴작 확정 버전으로 교체 (현재 로컬 5.8 GUID)

---

## 우선순위 요약 (브리프 §7)

1. **MVP**: Phase 1(타격) + Phase 3(반응속도·저장) — 핵심 재미 + 입력 파이프라인 검증
2. Phase 4 일부(수비·베이스 러닝)
3. Phase 5(피트니스·AI 피드백)
4. Phase 6(멀티·랭킹)
