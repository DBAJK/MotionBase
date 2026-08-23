# MotionBase 개발 환경 구축 기록

> SporTrack : Baseball (2026 XR 경진대회 과제6) — 개발 PC 환경 설정 전 과정.
> 최초 작성: 2026-07-20

---

## 1. 환경 스펙

| 항목 | 값 |
| --- | --- |
| OS | Windows 11 Pro (10.0.26200) |
| 엔진 | **Unreal Engine 5.8.0** (`++UE5+Release-5.8`, CL 55116800) |
| 엔진 경로 | `C:\Program Files\Epic Games\UE_5.8` (약 29.6 GB) |
| 컴파일러 | Visual Studio 18 Enterprise — MSVC 14.51.36248 |
| Windows SDK | 10.0.26100.0 |
| 프로젝트 경로 | `D:\pj\motionBase` |
| VR 장비 | HTC VIVE Pro (대여) |

> ⚠️ UBT 경고: MSVC 14.51.36248은 UE 5.8의 권장 버전(14.50.35717)보다 최신입니다.
> 현재까지 빌드에는 문제 없으나, 원인 불명 컴파일 오류 발생 시 이 부분을 먼저 의심할 것.

---

## 2. 설치 순서

- [x] **Epic Games Launcher** 설치
- [x] **Unreal Engine 5.8** 설치 (런처 → 언리얼 엔진 탭)
- [x] **Steam** (기설치)
- [x] **SteamVR** 설치 (Steam → 라이브러리 → SteamVR)
- [x] **OpenXR 런타임을 SteamVR로 지정** (SteamVR → 설정 → 개발자)

### OpenXR 런타임 확인 방법

```powershell
(Get-ItemProperty "HKLM:\SOFTWARE\Khronos\OpenXR\1").ActiveRuntime
# 기대값: C:\Program Files (x86)\Steam\steamapps\common\SteamVR\steamxr_win64.json
```

---

## 3. 프로젝트 설정

### 3-1. `MotionBase.uproject`

| 항목 | 값 |
| --- | --- |
| EngineAssociation | `{1559C6AB-49AD-7E45-384E-039EE6843214}` (⚠️ 머신별 GUID) |
| 활성 플러그인 | OpenXR, XRBase, EnhancedInput, PythonScriptPlugin, EditorScriptingUtilities |

### 3-2. `Source/MotionBase/MotionBase.Build.cs`

의존 모듈: `Core`, `CoreUObject`, `Engine`, `InputCore`, `EnhancedInput`, `HeadMountedDisplay`, `HTTP`, `Json`, `JsonUtilities`

```csharp
// 카테고리별 하위 폴더를 모듈 루트 기준 경로로 #include 하므로 필수
PublicIncludePaths.Add(ModuleDirectory);
```

### 3-3. `Source/*.Target.cs`

```csharp
// 설치형 엔진과 공유 빌드환경에서 경고레벨 충돌 방지
bOverrideBuildEnvironment = true;
```

### 3-4. `Config/DefaultEngine.ini`

```ini
[/Script/EngineSettings.GameMapsSettings]
EditorStartupMap=/Game/Maps/BattingTest.BattingTest
GameDefaultMap=/Game/Maps/BattingTest.BattingTest
GlobalDefaultGameMode=/Script/MotionBase.MotionBaseGameMode
```

### 3-5. `.gitignore`

제외 대상: `Binaries/` `Build/` `Intermediate/` `Saved/` `DerivedDataCache/` `.vs/` `*.sln` `Config/Secrets.ini`

---

## 4. 트러블슈팅 기록 ★

실제로 막혔던 것들과 해결법. **같은 문제 재발 시 여기부터 확인.**

### 4-1. "Unreal Engine 누락됨 — 적합한 엔진을 찾을 수 없습니다"

- **증상**: `.uproject` 더블클릭 / VS 열기 시 에러
- **원인**: Epic 런처가 UE 5.8을 표준 위치(HKLM)에 **등록하지 않음**. 그래서 `EngineAssociation`을 버전 문자열 `"5.8"`로 두면 경로로 변환 실패
- **해결**: HKCU에 등록된 **foreign-build GUID**를 사용

```powershell
# 등록된 엔진 GUID 확인
Get-ItemProperty "HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds"
# {1559C6AB-49AD-7E45-384E-039EE6843214} : C:/Program Files/Epic Games/UE_5.8
```

> 💡 커맨드라인 빌드는 엔진 폴더에서 직접 실행되므로 이 변환이 불필요 → CLI는 되는데 VS만 안 되는 현상이 나타남
> ⚠️ 이 GUID는 **머신별 값**. 다른 PC/뉴작 환경에서는 재연결 필요.

### 4-2. "MotionBaseEditor modifies the values of properties..."

- **증상**: 프로젝트 파일 생성 중 경고레벨(`UndefinedIdentifierWarningLevel` 등) 충돌 예외
- **원인**: 설치형 엔진에서는 게임 타깃이 엔진과 공유하는 경고 레벨을 변경할 수 없음
- **해결**: 두 Target.cs에 `bOverrideBuildEnvironment = true;` 추가

### 4-3. `fatal error C1083: 'Actors/Bat.h': No such file or directory`

- **원인**: 각 `.cpp`가 모듈 루트 기준 경로로 헤더를 참조하는데, 모듈 루트가 인클루드 검색 경로에 없었음
- **해결**: `Build.cs`에 `PublicIncludePaths.Add(ModuleDirectory);`

### 4-4. "Unable to build while Live Coding is active"

- **원인**: 에디터가 실행 중이면 Live Coding이 락을 잡아 커맨드라인 빌드 불가
- **해결**: 에디터 종료 후 빌드. 또는 에디터 안에서 `Ctrl + Alt + F11` (단, **새 클래스 추가 시엔 풀 빌드 필요**)

### 4-5. 매번 수동으로 빈 레벨 만들기 번거로움

- **해결**: Python 스크립트로 맵 자동 생성 → `Content/Maps/BattingTest.umap`
- 바닥(50m) + 태양광 + SkyAtmosphere + SkyLight + PlayerStart 포함
- 시작 맵으로 지정해서 **프로젝트 열면 바로 Play 가능**

---

## 5. 빌드 & 실행

### 5-1. 커맨드라인 빌드

```powershell
# 프로젝트 파일(.sln) 생성
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  -projectfiles -project="D:\pj\motionBase\MotionBase.uproject" -game -rocket -progress

# 에디터 타깃 빌드
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  MotionBaseEditor Win64 Development -Project="D:\pj\motionBase\MotionBase.uproject" -WaitMutex
```

### 5-2. 일상 작업 3가지 방법

| 방법 | 언제 | 조작 |
| --- | --- | --- |
| 에디터 직접 | 그냥 실행/플레이 | `MotionBase.uproject` 더블클릭 |
| Live Coding | `.cpp` 수정 반영 | 에디터에서 `Ctrl + Alt + F11` |
| Visual Studio | 새 클래스·헤더 변경 | `MotionBase.sln` → `Development Editor` + `Win64` → `F5` |

### 5-3. 플레이

- **모니터**: ▶ Play (`Alt + P`) → 화면 클릭 → `Space`(스윙) / `R`(리셋)
- **VR**: SteamVR 먼저 실행 → 에디터 열기 → Play 옆 `▼` → **VR Preview**
  - ⚠️ 에디터는 **시작할 때** VR 런타임을 잡음 → SteamVR을 먼저 켤 것
  - ⚠️ PIE 실행 중에는 Play 버튼이 정지/일시정지로 바뀜. `▼`를 보려면 **먼저 정지**

---

## 6. VR 하드웨어 인식 상태

`C:\Program Files (x86)\Steam\logs\vrserver.txt` 기준

| 장치 | 시리얼 | 상태 |
| --- | --- | --- |
| 헤드셋 (HMD) | `LHR-DC45B2B4` | ✅ 인식 (펌웨어·IMU·오디오·카메라 정상) |
| 컨트롤러 1 | `LHR-8616A2DE` | ✅ 페어링됨 |
| 컨트롤러 2 | `LHR-E9534371` | ✅ 페어링됨 |
| **베이스 스테이션** | — | ❌ **미감지** |

### ⚠️ 미해결 이슈: 베이스 스테이션 미감지

로그에 `LHB-`(베이스 스테이션 시리얼) 기록이 **전무**. 세 기기 모두 아래를 출력:

```
LHR-DC45B2B4 H: No base stations seen...
LHR-8616A2DE C: No base stations seen...
LHR-E9534371 C: No base stations seen...
Preferred basestation 00000000
```

**영향**: 회전(IMU)만 되고 **위치 추적 불가**.
→ 배트 속도는 BatTip 월드 좌표 미분으로 구하므로 **스윙 측정 자체가 불가능**. 반드시 해결 필요.

**점검 항목**

- [ ] 베이스 스테이션 **AC 전원** 연결 (USB 아님)
- [ ] LED 초록 확인 (꺼짐=전원 없음, 깜빡임=시야 확보 실패)
- [ ] 높이 2m 이상, 아래로 30~45° 기울여 플레이 공간을 향하게
- [ ] 채널 설정 (2.0=자동 / 1.0=모드 A·B, c일 때 동기화 케이블)
- [ ] **대여 장비에 베이스 스테이션이 포함되었는지 확인** ← 미포함이면 주최 측 즉시 문의

---

## 7. 남은 확인 사항

- [ ] **뉴작 요구 UE 버전 확정** → 확정 시 `EngineAssociation` 교체 (현재 로컬 5.8 기준)
- [ ] SporTrack 플랫폼 연동 스펙
- [ ] LiDAR SDK / 데이터 포맷
- [ ] 멀티플레이 프레임워크 제공 여부

---

## 관련 문서

- `CLAUDE.md` — 아키텍처·설계 원칙
- `ROADMAP.md` — 개발 단계별 백로그
- `history/CLAUDE_1.md` — 정본 기획 브리프
