#!/usr/bin/env python3
"""
UDP 수신 프로브 — UE 없이 pose_sender.py 출력을 검증한다.

같은 host/port 로 들어오는 JSON 프레임을 받아 요약 출력한다.
UE 수신부(ABodyPoseReceiver, 3단계)가 파싱할 포맷을 사람 눈으로 확인하는 용도.

실행:  python udp_probe.py --port 6789
"""

import argparse
import json
import socket


LANDMARK_NAMES = [
    "Nose", "LShoulder", "RShoulder", "LElbow", "RElbow", "LWrist", "RWrist",
    "LHip", "RHip", "LKnee", "RKnee", "LAnkle", "RAnkle",
]


def main():
    ap = argparse.ArgumentParser(description="MotionBase 자세 UDP 수신 프로브")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=6789)
    ap.add_argument("--verbose", action="store_true", help="관절 좌표 전체 출력")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((args.host, args.port))
    print(f"[udp_probe] {args.host}:{args.port} 수신 대기...")

    count = 0
    while True:
        data, _ = sock.recvfrom(65535)
        try:
            msg = json.loads(data.decode("utf-8"))
        except json.JSONDecodeError:
            print("[udp_probe] JSON 파싱 실패")
            continue

        count += 1
        if not msg.get("tracked"):
            print(f"frame {msg.get('frame')}: 사람 미검출")
            continue

        lm = msg.get("lm", [])
        if args.verbose:
            print(f"--- frame {msg['frame']} t={msg['t']:.3f} 관절 {len(lm)} ---")
            for name, p in zip(LANDMARK_NAMES, lm):
                print(f"  {name:10s} x={p[0]:8.1f} y={p[1]:8.1f} z={p[2]:8.1f} vis={p[3]:.2f}")
        elif count % 15 == 0:  # 대략 0.5초마다 한 줄 요약
            nose = lm[0] if lm else [0, 0, 0, 0]
            hips_vis = (lm[7][3] + lm[8][3]) / 2 if len(lm) > 8 else 0
            print(f"frame {msg['frame']} t={msg['t']:.3f} | 코 z(높이)={nose[2]:6.1f}cm "
                  f"| 엉덩이 신뢰도={hips_vis:.2f}")


if __name__ == "__main__":
    main()
