#!/usr/bin/env python3
"""Sanity-check GPU/TensorRT runtime status for the Video Analyzer backend.

The script queries `/api/system/info` to inspect `engine_runtime`, and can
optionally perform a subscribe/unsubscribe cycle to ensure the runtime status
updates correctly while a pipeline is active.

Usage examples::

    python scripts/check_gpu_inference.py --require-gpu \
        --base http://127.0.0.1:8082

    python scripts/check_gpu_inference.py --url rtsp://127.0.0.1:8554/camera_01 \
        --profile det_720p --require-device-binding
"""

from __future__ import annotations

import argparse
import sys
import time
import uuid
from typing import Iterable, Optional

import requests


def get_system_info(base_url: str, timeout: float) -> dict:
    response = requests.get(f"{base_url}/api/system/info", timeout=timeout)
    response.raise_for_status()
    payload = response.json()
    if not isinstance(payload, dict) or not payload.get("success"):
        raise ValueError("/api/system/info returned unexpected payload")
    return payload.get("data", {})


def post_json(base_url: str, path: str, payload: dict, timeout: float) -> dict:
    response = requests.post(f"{base_url}{path}", json=payload, timeout=timeout)
    response.raise_for_status()
    body = response.json()
    if not isinstance(body, dict) or not body.get("success"):
        raise ValueError(f"{path} returned failure payload: {body!r}")
    return body.get("data", {})


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def pick_profile(base_url: str, timeout: float, preferred: Optional[str]) -> str:
    response = requests.get(f"{base_url}/api/profiles", timeout=timeout)
    response.raise_for_status()
    payload = response.json()
    if not isinstance(payload, dict) or not payload.get("success"):
        raise ValueError("/api/profiles returned unexpected payload")
    profiles = payload.get("data")
    if not isinstance(profiles, list) or not profiles:
        raise ValueError("no profiles available")
    if preferred:
        for item in profiles:
            if item.get("name") == preferred:
                return preferred
        raise ValueError(f"profile '{preferred}' not found")
    return profiles[0].get("name")


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser(description="Validate GPU runtime status")
    parser.add_argument("--base", default="http://127.0.0.1:8082",
                        help="Analysis API base URL")
    parser.add_argument("--timeout", type=float, default=5.0,
                        help="Request timeout in seconds")
    parser.add_argument("--expect-provider", default=None,
                        help="Assert that engine_runtime.provider matches this value")
    parser.add_argument("--require-gpu", action="store_true",
                        help="Fail if engine_runtime.gpu_active is false")
    parser.add_argument("--require-iobinding", action="store_true",
                        help="Fail if engine_runtime.io_binding is false")
    parser.add_argument("--require-device-binding", action="store_true",
                        help="Fail if engine_runtime.device_binding is false")
    parser.add_argument("--url", default=None,
                        help="Optional source URL to subscribe for a short sanity check")
    parser.add_argument("--profile", default=None,
                        help="Profile name to use when --url is supplied")
    parser.add_argument("--model", default=None,
                        help="Optional model override when subscribing")
    args = parser.parse_args(list(argv))

    base = args.base.rstrip("/")

    try:
        info = get_system_info(base, args.timeout)
        runtime = info.get("engine_runtime", {})
        if not isinstance(runtime, dict):
            raise ValueError("/api/system/info payload missing engine_runtime")

        provider = runtime.get("provider")
        gpu_active = bool(runtime.get("gpu_active"))
        iobinding_active = bool(runtime.get("io_binding"))
        device_binding = bool(runtime.get("device_binding"))
        cpu_fallback = bool(runtime.get("cpu_fallback"))

        print(f"[info] provider={provider} gpu_active={gpu_active} "
              f"io_binding={iobinding_active} device_binding={device_binding} "
              f"cpu_fallback={cpu_fallback}")

        if args.expect_provider is not None:
            expect(provider == args.expect_provider,
                   f"provider mismatch: expected {args.expect_provider}, got {provider}")
        defer_gpu_check = False
        if args.require_gpu and not gpu_active:
            if args.url:
                defer_gpu_check = True
                print("[warn] GPU inactive before subscribe; will re-check after pipeline warm-up")
            else:
                raise AssertionError("GPU inactive while --require-gpu was specified")
        if args.require_iobinding:
            expect(iobinding_active, "IoBinding inactive while --require-iobinding was specified")
        defer_device_check = False
        if args.require_device_binding and not device_binding:
            if args.url:
                defer_device_check = True
                print("[warn] Device binding inactive before subscribe; will re-check after pipeline warm-up")
            else:
                raise AssertionError("Device binding inactive while --require-device-binding was specified")

        if args.url:
            profile = pick_profile(base, args.timeout, args.profile)
            stream_id = f"gpucheck_{int(time.time())}_{uuid.uuid4().hex[:6]}"
            payload = {
                "stream": stream_id,
                "profile": profile,
                "url": args.url,
            }
            if args.model:
                payload["model_id"] = args.model

            print(f"[info] subscribing stream={stream_id} profile={profile}")
            post_json(base, "/api/subscribe", payload, args.timeout)

            time.sleep(1.0)
            info_after = get_system_info(base, args.timeout)
            runtime_after = info_after.get("engine_runtime", {}) if isinstance(info_after, dict) else {}
            provider_after = runtime_after.get("provider")
            gpu_after = bool(runtime_after.get("gpu_active"))
            device_binding_after = bool(runtime_after.get("device_binding"))
            print(f"[info] runtime after subscribe provider={provider_after} gpu={gpu_after} device_binding={device_binding_after}")

            if args.require_gpu:
                if defer_gpu_check:
                    expect(gpu_after, "GPU inactive after subscribe")
                else:
                    expect(gpu_active or gpu_after, "GPU inactive after subscribe")
            if args.require_device_binding:
                if defer_device_check:
                    expect(device_binding_after, "Device binding inactive after subscribe")
                else:
                    expect(device_binding or device_binding_after, "Device binding inactive after subscribe")

            post_json(base, "/api/unsubscribe", {"stream": stream_id, "profile": profile}, args.timeout)
            print("[info] unsubscribe succeeded")

        print("\nGPU runtime checks passed.")
        return 0

    except Exception as exc:  # noqa: BLE001
        print(f"[error] {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
