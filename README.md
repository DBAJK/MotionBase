# MotionBase — SporTrack : Baseball

비착용형(Non-HMD) XR 야구 훈련 콘텐츠. 2026 AI·가상융합(XR) 서비스 개발자 경진대회 / 과제6(㈜뉴작) 출품작.

플레이어는 **헤드셋을 쓰지 않는다.** 베이스 스테이션이 손에 쥔 컨트롤러(배트)와 신체를 추적하고,
화면은 바닥·공간 투사로 본다. 스윙 궤적을 실제로 측정해 **정확도·효율·일관성 3축**으로 채점한다.

| | |
|---|---|
| 엔진 | Unreal Engine 5.8 (C++ + Blueprint) |
| XR | OpenXR 플러그인 + HTC Vive Pro (구형 SteamVR 플러그인은 UE 5.1 폐기) |
| 저장 | 로컬 SaveGame — 별도 백엔드/DB 없음 |
| AI | 생성형 코칭 피드백 — UE HTTP 기반 Anthropic Messages API (키는 `Config/Secrets.ini`) |
| 모드 | 타격 · 수비 · 반응속도 · 베이스 러닝 · 신체 인식 · 기능성 피트니스 (6종) |

---

## 실행 방법

**필요**: Windows · Unreal Engine 5.8 · Visual Studio (C++ 워크로드)

1. `MotionBase.uproject` 우클릭 → **Generate Visual Studio project files**
2. 생성된 솔루션에서 `Development Editor | Win64` 빌드
3. 에디터가 열리면 **PIE(Play) 실행** — 시작 화면(모드 선택)부터 시작한다

> ⚠️ **`.uproject`가 엔진을 못 찾으면** `EngineAssociation` 때문이다. 현재 값은 특정 PC에
> 등록된 머신별 GUID라 다른 환경에서는 연결되지 않는다. `.uproject`를 우클릭 →
> *Switch Unreal Engine version*으로 각자 엔진을 다시 지정할 것.

### 흐름

```
시작 화면(모드 선택)
   ├─ 타격 → 난이도(초보/아마추어/프로) → 타석(우타/좌타) → 타격 훈련
   └─ 수비 → 세부 종목(포구/송구/백업) ──────────────────→ 해당 수비 훈련
                                          [M] 로 언제든 시작 화면 복귀
```

Vive 없이도 전부 동작한다 — 타격·수비 모두 키보드 입력으로 채점까지 그대로 통과한다.

### 조작

| 화면 | 키 |
|------|-----|
| 모드/메뉴 선택 | `↑`/`↓`(또는 `W`/`S`) 이동 · `Enter`/`Space` 확정 · `←`/`Backspace` 뒤로 · `V` Vive 브링업 진단 |
| 타격 훈련 | `Space` 스윙 · `R` 세션 리셋 · `F` AI 피드백 요청 · `M` 시작 화면 복귀 |
| 수비 — 포구 | `1`~`4` 타구 유형(땅볼/뜬공/라인드라이브/혼합) · `W`·`A`·`S`·`D` 이동 · `Space` 포구 · `M` 복귀 |
| 수비 — 송구 | `Space` 누름/뗌 = 파워 게이지 충전/발사 · `M` 복귀 |
| 수비 — 백업 | `1`~`4`(또는 `W`/`S` + `Enter`) 선택 · `M` 복귀 |
| Vive 진단 | `H` 좌/우손 전환 · `R` 리셋 · `M` 복귀 |

---

## 구조

계산 로직을 UE 액터·렌더에서 떼어내, **헤드셋 없이 단위 테스트 가능한 순수 함수**로 유지한다.
계산 버그와 연출 버그를 분리해서 잡기 위한 구조다.

```
[Actor]    ABat / APitchingZone      입력 수집·연출·충돌
    │ 원시 궤적 (FSwingSample[])
[계산]     USwingAnalyzer            속도 미분·컨택 감지·피크    ← UE 비의존
    │ FSwingMetrics
[점수]     UScoringService           3축 가중합                  ← UE 비의존
    │ FScoreResult
[결과]     UModeManager → SaveGame(로컬 기록) / UAIFeedbackService(코칭 텍스트)
```

| 경로 | 역할 |
|------|------|
| `Source/MotionBase/Data/` | 계층 간 계약 구조체·열거형 (`FSwingSample`·`FSwingMetrics`·`FScoreResult`·`FSessionResult` 등) |
| `Source/MotionBase/Analysis/` | `USwingAnalyzer`(궤적→지표) · `UHitModel`(가상 타구) · `UBodyMechanicsAnalyzer` · `UWeaknessDetector` |
| `Source/MotionBase/Scoring/` | `UScoringService` — 지표 → 3축 점수 (`FScoringConfig` 캘리브레이션 상수) |
| `Source/MotionBase/Input/` | 입력 추상화 (`Mock` / `Vive` / `Camera` provider) |
| `Source/MotionBase/Actors/` | `ABat`, `APitchingZone` |
| `Source/MotionBase/Core/` | 게임 모드·모드 매니저·시작 화면 |
| `Source/MotionBase/Core/Defense/` | 수비 훈련 — `CatchBall`(포구) · `Throw`(송구) · `Cover`(백업). 종목별 Pawn/Judge/HUD |
| `Source/MotionBase/Save/` | `UMotionBaseSaveGame` — 세션 기록·기준선 영속화 |
| `Source/MotionBase/AI/` | `UAIFeedbackService`(생성형 코칭) · `UDrillCatalog` |
| `Source/MotionBase/UI/` | Canvas HUD (UMG 에셋 불필요) |
| `Source/MotionBase/Testing/` | PC 테스트 폰(`ASwingTestPawn`) · VR 타격 폰 · Vive 브링업 진단 하네스 |

---

## 현재 상태

**플레이 가능한 모드 (키보드/Mock 입력으로 채점까지 동작)**

- **타격 훈련** — 투구(직구/변화구) → 실제 비행 → 타이밍 판정 → 3축 채점.
  난이도 동적 조정, 스트라이크/볼·볼카운트, 좌타/우타 타석, 가상 타구 결과(홈런/안타/파울)까지.
- **수비 훈련** — 세부 3종:
  - **포구** — 땅볼/뜬공/라인드라이브 타구를 이동해 잡기 (낙구지점·타이밍 판정)
  - **송구** — 파워 게이지로 타겟까지 정확히 던지기 (명중/짧음/넘김)
  - **백업(커버)** — 경기 상황별 올바른 백업 위치 4지선다 판단

**공통 배선 완료**

- 로컬 저장(`UMotionBaseSaveGame`) — 세션 기록·모드별 최고/평균/추세 집계
- 생성형 AI 코칭 피드백(`UAIFeedbackService`, 타격 `F` 키) — Anthropic Messages API 실호출

**미검증 / 미구현**

- **Vive 경로**(`ABat`, `UViveMotionInputProvider`) — 작성됐으나 SteamVR 미연동으로 아직 실기 검증 전.
  `V` 키의 브링업 진단 하네스가 이걸 확인하기 위한 도구다.
- **LiDAR 기반 모드**(반응속도·베이스 러닝·신체 인식·기능성 피트니스) — 뉴작 장비/SDK 확인 후 실장.
  수비 훈련도 현재는 키보드 입력이며, LiDAR 전신 추적 연결 시 실제 동작으로 대체된다.

점수 기준 상수(σt, d_max, v_min, v_target)는 **실측 캘리브레이션 전의 예시값**이다.
보정 전까지 `FScoreResult::bUncalibrated`가 `true`로 전파된다.

진행 상황과 다음 작업은 [ROADMAP.md](ROADMAP.md) 참조.

---

## 문서

| 파일 | 내용 |
|------|------|
| [ROADMAP.md](ROADMAP.md) | Phase별 진행 상황·남은 작업 |
| [CLAUDE.md](CLAUDE.md) | 아키텍처 원칙·개발 규칙 |
| [history/CLAUDE_1.md](history/CLAUDE_1.md) | 정본 브리프 — 기술 결정·측정 방식·점수 체계의 근거 |
