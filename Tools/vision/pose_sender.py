#!/usr/bin/env python3
"""
MotionBase — MediaPipe 자세 송신 PoC (2단계).

웹캠 → OpenCV 캡처 → MediaPipe Pose → 13개 관절(EBodyLandmark 순서)을
UE 좌표(cm, z-up)로 변환해 UDP/JSON 으로 UE 수신부(ABodyPoseReceiver)에 보낸다.

계약(UE 쪽):
  Source/MotionBase/Data/CameraPose.h  — FCameraPoseFrame / EBodyLandmark
  Source/MotionBase/Analysis/BodyMechanicsAnalyzer.h — 이 데이터를 소비

의존성:  pip install -r requirements.txt   (opencv-python, mediapipe)
실행 예:  python pose_sender.py --camera 0 --host 127.0.0.1 --port 6789 --preview

────────────────────────────────────────────────────────────────────
좌표계 결정 (중요 — 실측 캘리브레이션 대상)
────────────────────────────────────────────────────────────────────
MediaPipe `pose_world_landmarks`: 미터 단위, 원점 = 양 엉덩이 중점.
  축: x=오른쪽(+), y=아래(+), z=카메라 쪽(-). 각도/회전 지표에 이상적.
UE: cm, z-up. x=전방, y=오른쪽, z=위.
  → UE.Y = mp.x,  UE.Z = -mp.y,  UE.X = -mp.z   (×100 로 m→cm)
축 부호(AXIS_SIGN_*)는 실제 카메라 설치 방향에 맞춰 캘리브레이션할 것.

⚠️ 한계: world_landmarks 는 '엉덩이 중점 원점'이라 몸 전체의 방(room) 이동은
   담기지 않는다. 회전 지표(X-factor·kinetic chain·척추각·머리고정)는 정확하지만,
   WeightShiftCm(체중 이동, 몸통 병진)은 이 소스로는 ≈0 이다.
   → 체중 이동은 정규화 랜드마크(image space)나 깊이카메라 단계에서 별도 보강. (TODO)
"""

import argparse
import json
import socket
import time

import cv2
import mediapipe as mp

# EBodyLandmark(UE) 순서 → MediaPipe Pose 인덱스. 순서를 절대 바꾸지 말 것
# (수신부가 위치 인덱스로 Landmarks[i] 를 채운다).
MP = mp.solutions.pose.PoseLandmark
LANDMARK_MAP = [
    MP.NOSE,            # 0  Nose
    MP.LEFT_SHOULDER,   # 1  LeftShoulder
    MP.RIGHT_SHOULDER,  # 2  RightShoulder
    MP.LEFT_ELBOW,      # 3  LeftElbow
    MP.RIGHT_ELBOW,     # 4  RightElbow
    MP.LEFT_WRIST,      # 5  LeftWrist
    MP.RIGHT_WRIST,     # 6  RightWrist
    MP.LEFT_HIP,        # 7  LeftHip
    MP.RIGHT_HIP,       # 8  RightHip
    MP.LEFT_KNEE,       # 9  LeftKnee
    MP.RIGHT_KNEE,      # 10 RightKnee
    MP.LEFT_ANKLE,      # 11 LeftAnkle
    MP.RIGHT_ANKLE,     # 12 RightAnkle
]

# m → cm, 그리고 축 매핑 부호 (설치에 맞춰 조정).
M_TO_CM = 100.0
AXIS_SIGN_FORWARD = -1.0  # UE.X = AXIS_SIGN_FORWARD * mp.z
AXIS_SIGN_RIGHT = 1.0     # UE.Y = AXIS_SIGN_RIGHT   * mp.x
AXIS_SIGN_UP = -1.0       # UE.Z = AXIS_SIGN_UP      * mp.y


def to_ue_cm(lm):
    """MediaPipe world landmark → UE 좌표(cm) 튜플 (x_fwd, y_right, z_up)."""
    return (
        AXIS_SIGN_FORWARD * lm.z * M_TO_CM,
        AXIS_SIGN_RIGHT * lm.x * M_TO_CM,
        AXIS_SIGN_UP * lm.y * M_TO_CM,
    )


def build_payload(world_landmarks, timestamp, frame_index):
    """FCameraPoseFrame 계약에 맞는 JSON 직렬화용 dict.

    lm[i] = [x, y, z, visibility]  (i = EBodyLandmark 순서, cm, UE 좌표)
    """
    lm_out = []
    for src in LANDMARK_MAP:
        p = world_landmarks.landmark[src.value]
        x, y, z = to_ue_cm(p)
        # 소수 3자리로 잘라 패킷 크기 축소 (µm 정밀도 불필요).
        lm_out.append([round(x, 3), round(y, 3), round(z, 3), round(p.visibility, 3)])
    return {
        "t": timestamp,      # 초, 송신측 시계(Unix epoch). 수신부가 UE 클럭에 오프셋 정렬.
        "frame": frame_index,
        "tracked": True,
        "lm": lm_out,
    }


def main():
    ap = argparse.ArgumentParser(description="MotionBase MediaPipe 자세 UDP 송신 PoC")
    ap.add_argument("--camera", type=int, default=0, help="OpenCV 카메라 인덱스")
    ap.add_argument("--host", default="127.0.0.1", help="UE 수신부 호스트")
    ap.add_argument("--port", type=int, default=6789, help="UE 수신부 UDP 포트")
    ap.add_argument("--width", type=int, default=1280, help="캡처 가로")
    ap.add_argument("--height", type=int, default=720, help="캡처 세로")
    ap.add_argument("--complexity", type=int, default=1, choices=[0, 1, 2],
                    help="Pose 모델 복잡도 (0=빠름, 2=정확). 실시간은 1 권장")
    ap.add_argument("--preview", action="store_true", help="캡처 창 표시(디버그)")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    dest = (args.host, args.port)

    cap = cv2.VideoCapture(args.camera)
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
    if not cap.isOpened():
        raise SystemExit(f"카메라 {args.camera} 를 열 수 없습니다.")

    pose = mp.solutions.pose.Pose(
        model_complexity=args.complexity,
        smooth_landmarks=True,
        min_detection_confidence=0.5,
        min_tracking_confidence=0.5,
    )

    print(f"[pose_sender] {dest} 로 송신 시작. 종료: Ctrl+C" + (" / q(프리뷰)" if args.preview else ""))
    frame_index = 0
    sent = 0
    last_log = time.time()

    try:
        while True:
            ok, frame = cap.read()
            if not ok:
                continue
            frame_index += 1
            timestamp = time.time()

            # MediaPipe 는 RGB 입력. 미러링은 하지 않는다(좌우 관절 라벨 뒤집힘 방지).
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            rgb.flags.writeable = False
            result = pose.process(rgb)

            if result.pose_world_landmarks:
                payload = build_payload(result.pose_world_landmarks, timestamp, frame_index)
                sock.sendto(json.dumps(payload, separators=(",", ":")).encode("utf-8"), dest)
                sent += 1
            else:
                # 사람 미검출 — tracked=false 만 알려 수신부가 게이트 처리.
                sock.sendto(json.dumps(
                    {"t": timestamp, "frame": frame_index, "tracked": False, "lm": []},
                    separators=(",", ":")).encode("utf-8"), dest)

            # 1초마다 처리량 로그.
            now = time.time()
            if now - last_log >= 1.0:
                print(f"[pose_sender] {sent} fps 송신중 (frame {frame_index})")
                sent = 0
                last_log = now

            if args.preview:
                if result.pose_landmarks:
                    mp.solutions.drawing_utils.draw_landmarks(
                        frame, result.pose_landmarks, mp.solutions.pose.POSE_CONNECTIONS)
                cv2.imshow("MotionBase pose (q: quit)", frame)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break
    except KeyboardInterrupt:
        print("\n[pose_sender] 종료.")
    finally:
        pose.close()
        cap.release()
        if args.preview:
            cv2.destroyAllWindows()
        sock.close()


if __name__ == "__main__":
    main()
