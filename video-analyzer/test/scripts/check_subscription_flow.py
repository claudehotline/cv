#!/usr/bin/env python3
"""Exercise the subscribe/unsubscribe REST endpoints.

This helper assumes the Video Analyzer backend is already running. It
performs the following steps against the analysis API:

1. Fetch available profiles (or use the requested profile).
2. Issue `/api/subscribe` for a temporary stream ID.
3. Verify the created pipeline is present via `/api/pipelines`.
4. Issue `/api/unsubscribe` and ensure the pipeline is removed.

Usage::

    python scripts/check_subscription_flow.py \
        --base http://127.0.0.1:8082 \
        --url rtsp://127.0.0.1:8554/camera_01

The command exits with status 0 when all steps succeed, otherwise 1.
"""

from __future__ import annotations

import argparse
import sys
import time
import uuid
from typing import Iterable, List, Optional

import requests


def get_json(base_url: str, path: str, timeout: float) -> dict:
    url = f"{base_url.rstrip('/')}{path}"
    response = requests.get(url, timeout=timeout)
    response.raise_for_status()
    payload = response.json()
    if not isinstance(payload, dict) or not payload.get("success"):
        raise ValueError(f"endpoint {url} returned unexpected payload: {payload!r}")
    return payload


def post_json(base_url: str, path: str, payload: dict, timeout: float) -> dict:
    url = f"{base_url.rstrip('/')}{path}"
    response = requests.post(url, json=payload, timeout=timeout)
    response.raise_for_status()
    data = response.json()
    if not isinstance(data, dict) or not data.get("success"):
        raise ValueError(f"endpoint {url} reported failure: {data!r}")
    return data


def pick_profile(base_url: str, timeout: float, preferred: Optional[str]) -> str:
    resp = get_json(base_url, "/api/profiles", timeout)
    profiles = resp.get("data")
    if not isinstance(profiles, list) or not profiles:
        raise ValueError("no profiles available from /api/profiles")
    if preferred:
        for entry in profiles:
            if entry.get("name") == preferred:
                return preferred
        raise ValueError(f"profile '{preferred}' not found")
    return profiles[0].get("name")


def find_pipeline_by_key(pipelines: List[dict], key: str) -> Optional[dict]:
    for item in pipelines:
        if item.get("key") == key:
            return item
    return None


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser(description="Check subscribe/unsubscribe flow")
    parser.add_argument("--base", default="http://127.0.0.1:8082", help="Analysis API base URL")
    parser.add_argument("--profile", default=None, help="Profile name to use (default: first profile)")
    parser.add_argument("--url", required=True, help="RTSP/WebRTC source URL used for subscribe")
    parser.add_argument("--model", default=None, help="Optional model id override")
    parser.add_argument("--timeout", type=float, default=5.0, help="HTTP timeout in seconds")
    args = parser.parse_args(list(argv))

    base_url = args.base.rstrip('/')
    stream_id = f"sanity_{int(time.time())}_{uuid.uuid4().hex[:6]}"

    try:
        profile = pick_profile(base_url, args.timeout, args.profile)
        print(f"[info] using profile: {profile}")

        subscribe_payload = {
            "stream": stream_id,
            "profile": profile,
            "url": args.url,
        }
        if args.model:
            subscribe_payload["model_id"] = args.model

        subscribe_resp = post_json(base_url, "/api/subscribe", subscribe_payload, args.timeout)
        data = subscribe_resp.get("data", {})
        pipeline_key = data.get("pipeline_key")
        if not pipeline_key:
            raise ValueError("subscribe response missing pipeline_key")
        print(f"[info] subscribe succeeded, pipeline_key={pipeline_key}")

        pipelines_resp = get_json(base_url, "/api/pipelines", args.timeout)
        pipelines = pipelines_resp.get("data", [])
        if not isinstance(pipelines, list):
            raise ValueError("/api/pipelines did not return a list")
        if not find_pipeline_by_key(pipelines, pipeline_key):
            raise ValueError("pipeline not visible in /api/pipelines after subscribe")
        print("[info] pipeline visible in /api/pipelines")

        unsubscribe_payload = {"stream": stream_id, "profile": profile}
        post_json(base_url, "/api/unsubscribe", unsubscribe_payload, args.timeout)
        print("[info] unsubscribe succeeded")

        pipelines_after = get_json(base_url, "/api/pipelines", args.timeout).get("data", [])
        if find_pipeline_by_key(pipelines_after, pipeline_key):
            raise ValueError("pipeline still present after unsubscribe")
        print("[info] pipeline removed after unsubscribe")

        print("\nSubscribe/unsubscribe flow passed.")
        return 0

    except Exception as exc:  # noqa: BLE001 - convert any failures into non-zero exit code
        print(f"[error] {exc}")
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
