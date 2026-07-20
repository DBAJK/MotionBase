# MotionBase — SporTrack : Baseball

비착용형(Non-HMD) XR 야구 콘텐츠. **Unreal Engine 단독(C++ + Blueprint)**, HTC Vive Pro 기반.
2026 AI·가상융합(XR) 서비스 개발자 경진대회 / 과제6 (㈜뉴작).

> **정본 브리프**: [history/CLAUDE_1.md](history/CLAUDE_1.md) — 프로젝트 개요·기술 결정·측정/점수 체계·열린 이슈의 원본.
> ([history/claude.md](history/claude.md)는 초기 버전.)

---

## 아키텍처 (UE 내부 계층 분리 — 서버 분리 아님)

```
[Actor 계층]    ABat / APitchingZone          입력 수집·연출·충돌·UI
      │  raw 궤적(FSwingSample 배열)
      ▼
[계산 계층]     USwingAnalyzer                순수 로직, UE 렌더/액터 비의존 → 단위 테스트 가능
      │  FSwingMetrics
      ▼
[점수 계층]     UScoringService               3축(정확도·효율·일관성) 가중합 → FScoreResult
      │
      ▼
[결과]          UMotionBaseSaveGame(로컬) / UAIFeedbackService(확장)
```

**핵심 원칙**: 계산 계층은 숫자 입력 → 숫자 출력. 헤드셋·Vive 없이 테스트해 계산 버그와 연출 버그를 분리.

## 소스 트리

| 경로 | 역할 |
|------|------|
| `Source/MotionBase/Data/` | 계층 간 계약 구조체: `FSwingSample`, `FSwingMetrics`, `FScoreResult`, 열거형 |
| `Source/MotionBase/Analysis/` | `USwingAnalyzer` — BatTip 미분·컨택 감지·피크 속도 (순수 로직) |
| `Source/MotionBase/Scoring/` | `UScoringService` — 3축 가중합, `FScoringConfig` 캘리브레이션 상수 |
| `Source/MotionBase/Actors/` | `ABat`(Vive 컨트롤러=배트, 링버퍼), `APitchingZone`(투구 타이밍) |
| `Source/MotionBase/Core/` | `UModeManager`(6개 모드 전환), `AMotionBaseGameMode` |
| `Source/MotionBase/Save/` | `UMotionBaseSaveGame` — 로컬 기록/기준선 (DB 없음) |
| `Source/MotionBase/AI/` | `UAIFeedbackService` — UE HTTP 기반 생성형 AI 피드백 (확장) |
| `Content/` | 맵·Blueprint·Input·UI 에셋 (에디터에서 작성) |

## 개발 우선순위 (1인 · 12주)

1. **MVP**: UE 셋업 + Vive 연동 + `타격` + `반응속도` + 로컬 저장
2. `수비` + `베이스 러닝` (LiDAR 전신 추적)
3. `기능성 피트니스` + 생성형 AI 피드백
4. **확장**: 멀티플레이·랭킹

## 규칙 / 주의

- 계산 계층에 UE 액터/렌더 의존성 넣지 말 것 (순수 함수 유지).
- 점수 기준 상수(σt, d_max, v_min, v_target 등)는 **실측 캘리브레이션 대상 — 하드코딩 확정 금지** (`FScoringConfig`).
- ⚠️ `EngineAssociation`은 로컬 UE 5.8을 가리키는 **머신별 GUID**(`{1559C6AB-...}`) — 이 PC의 HKCU 등록 기준.
  런처가 `5.8`을 HKLM에 등록 안 해서 버전 문자열로는 연결 실패 → GUID 사용. 다른 PC/뉴작 환경에선 재연결 필요.
  - UE 5.7+에서 `Get Motion Controller Data` 폐기 → `Get Motion Controller State` 사용 (Vive 속도 추출 시 주의).
- Vive 연동은 **OpenXR 플러그인** 사용 (구형 SteamVR 플러그인은 UE 5.1 폐기).
- AI API 키 등 비밀값은 `Config/Secrets.ini`(gitignore)로 분리.
