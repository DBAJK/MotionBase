# MotionBase — 자세 인식 (MediaPipe) PoC

Vive 컨트롤러가 못 보는 **전신 신체역학**(X-factor·kinetic chain·척추각·머리 고정)을
웹캠 + OpenCV + MediaPipe 로 측정해 UE 로 보낸다. UE 빌드는 건드리지 않는 별도 프로세스.

```
[웹캠] → OpenCV → MediaPipe Pose → 좌표변환(cm,z-up) → UDP/JSON → UE(ABodyPoseReceiver)
                                                                      → UBodyMechanicsAnalyzer
```

## 설치 · 실행

```bash
cd Tools/vision
python -m venv .venv && source .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements.txt

# 송신 (프리뷰 창 포함)
python pose_sender.py --camera 0 --host 127.0.0.1 --port 6789 --preview

# UE 없이 검증: 다른 터미널에서 수신 프로브
python udp_probe.py --port 6789 --verbose
```

## UDP 프로토콜 (UE 수신부 계약)

localhost UDP, 프레임당 JSON 1개. `lm` 배열은 **EBodyLandmark 순서 고정**
(`Source/MotionBase/Data/CameraPose.h`). 단위 cm, UE 좌표(x=전방, y=우, z=상).

```jsonc
{
  "t": 1733472000.123,   // 초, 송신측 Unix epoch. 수신부가 UE 클럭에 오프셋 정렬
  "frame": 42,
  "tracked": true,       // false면 사람 미검출, lm=[]
  "lm": [                // 13개: [x, y, z, visibility]
    [12.3, -4.5, 60.1, 0.98],  // 0 Nose
    ...                        // 1..12: L/R Shoulder,Elbow,Wrist,Hip,Knee,Ankle
  ]
}
```

## 알려진 한계 / TODO (다음 단계에서 보강)

- **시간 동기화**: `t`는 송신 PC의 Unix epoch. UE 스윙 클럭과 정확히 맞추려면
  수신부에서 **오프셋 보정**이 필요 (3단계 `ABodyPoseReceiver`). 핸드셰이크/PTP는 후속.
- **체중 이동(WeightShiftCm)**: `pose_world_landmarks`는 엉덩이 중점이 원점이라
  몸 전체 병진이 담기지 않음 → 이 소스로는 ≈0. 정규화 랜드마크나 깊이카메라로 보강 필요.
- **축 부호**(`AXIS_SIGN_*` in `pose_sender.py`): 실제 카메라 설치 방향에 맞춰
  실측 캘리브레이션. 좌우/전후가 뒤집히면 여기서 조정.
- **다인 검출**: MediaPipe Pose 는 1인 기준. 타자 외 인물이 프레임에 들어오면 오탐 가능.
