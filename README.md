# MotionBase — SporTrack : Baseball

비착용형(Non-HMD) XR 야구 훈련 콘텐츠. 2026 AI·가상융합(XR) 서비스 개발자 경진대회 / 과제6(㈜뉴작) 출품작.

플레이어는 **헤드셋을 쓰지 않는다.** 베이스 스테이션이 손에 쥔 컨트롤러(배트)와 신체를 추적하고,
화면은 바닥·공간 투사로 본다. 스윙 궤적을 실제로 측정해 **정확도·효율·일관성 3축**으로 채점한다.

| | |
|---|---|
| 엔진 | Unreal Engine 5.8 (C++ + Blueprint) |
| XR | OpenXR 플러그인 + HTC Vive Pro (구형 SteamVR 플러그인은 UE 5.1 폐기) |
| 저장 | 로컬 SaveGame — 별도 백엔드/DB 없음 |
| 모드 | 타격 · 반응속도 · 수비 · 베이스 러닝 · 신체 인식 · 기능성 피트니스 (6종) |

---

## 실행 방법

**필요**: Windows · Unreal Engine 5.8 · Visual Studio (C++ 워크로드)

1. `MotionBase.uproject` 우클릭 → **Generate Visual Studio project files**
2. 생성된 솔루션에서 `Development Editor | Win64` 빌드
3. 에디터가 열리면 **PIE(Play) 실행** — 시작 화면(모드 선택)부터 시작한다

> ⚠️ **`.uproject`가 엔진을 못 찾으면** `EngineAssociation` 때문이다. 현재 값은 특정 PC에
> 등록된 머신별 GUID라 다른 환경에서는 연결되지 않는다. `.uproject`를 우클릭 →
> *Switch Unreal Engine version*으로 각자 엔진을 다시 지정할 것.

### 조작

| 화면 | 키 |
|------|-----|
| 모드 선택 | `↑`/`↓`(또는 `W`/`S`) 이동 · `Enter` 시작 · `V` Vive 브링업 진단 |
| 타격 훈련 | `Space` 스윙 · `R` 세션 리셋 · `M` 모드 선택 복귀 |
| Vive 진단 | `H` 좌/우손 전환 · `R` 리셋 · `M` 복귀 |

Vive 없이도 전부 동작한다 — 타격은 키보드 타이밍 입력으로 채점까지 그대로 통과한다.

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
[결과]     UModeManager → SaveGame / AI 피드백
```

| 경로 | 역할 |
|------|------|
| `Source/MotionBase/Data/` | 계층 간 계약 구조체·열거형 |
| `Source/MotionBase/Analysis/` | `USwingAnalyzer` — 궤적 → 물리 지표 |
| `Source/MotionBase/Scoring/` | `UScoringService` — 지표 → 3축 점수 |
| `Source/MotionBase/Input/` | 입력 추상화 (`Mock` / `Vive` / `LiDAR` provider) |
| `Source/MotionBase/Actors/` | `ABat`, `APitchingZone` |
| `Source/MotionBase/Core/` | 게임 모드·모드 매니저·시작 화면 |
| `Source/MotionBase/UI/` | Canvas HUD (UMG 에셋 불필요) |
| `Source/MotionBase/Testing/` | PC 테스트 폰 · Vive 브링업 진단 하네스 |

---

## 현재 상태

- **동작함**: 시작 화면(모드 선택) · 타격 훈련(투구 → 타이밍 판정 → 3축 채점)
- **미검증**: Vive 경로(`ABat`, `UViveMotionInputProvider`) — 작성됐으나 SteamVR 미연동으로
  아직 실행된 적 없음. `V` 키의 브링업 진단 하네스가 이걸 확인하기 위한 도구다.
- **미구현**: 타격 외 5개 모드 (LiDAR 장비 필요)

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
