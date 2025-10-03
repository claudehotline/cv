#!/usr/bin/env python3
"""Simple regression helper for the Video Analyzer REST API.

The script expects the VideoAnalyzer backend to be running locally
(`VideoAnalyzer.exe config/app.yaml`). It exercises a handful of
endpoints and validates that they return HTTP 200 together with a JSON
payload containing `{ "success": true, ... }`.

Usage::

    python scripts/check_analysis_api.py --base http://127.0.0.1:8082

The command exits 0 when all checks pass, and 1 otherwise.
"""

from __future__ import annotations

import argparse
import sys
from typing import Dict, Iterable, Tuple

import requests


Endpoint = Tuple[str, str]


ENDPOINTS: Iterable[Endpoint] = (
    ("/api/system/info", "system_info"),
    ("/api/system/stats", "system_stats"),
    ("/api/models", "models"),
    ("/api/profiles", "profiles"),
    ("/api/pipelines", "pipelines"),
)


def request_json(base_url: str, path: str, timeout: float) -> Dict:
    url = f"{base_url.rstrip('/')}{path}"
    response = requests.get(url, timeout=timeout)
    response.raise_for_status()
    payload = response.json()
    if not isinstance(payload, dict):
        raise ValueError(f"unexpected payload type from {url}: {type(payload).__name__}")
    if not payload.get("success"):
        raise ValueError(f"endpoint {url} reported success=false")
    return payload


def main(argv: Iterable[str]) -> int:
    parser = argparse.ArgumentParser(description="Check VideoAnalyzer analysis API endpoints")
    parser.add_argument(
        "--base",
        default="http://127.0.0.1:8082",
        help="Base URL of the analysis API (default: http://127.0.0.1:8082)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=5.0,
        help="Request timeout in seconds (default: 5.0)",
    )
    args = parser.parse_args(list(argv))

    base_url = args.base.rstrip("/")
    problems = []

    for path, label in ENDPOINTS:
        try:
            payload = request_json(base_url, path, timeout=args.timeout)
            data = payload.get("data")
            summary = "n/a"
            if isinstance(data, list):
                summary = f"{len(data)} items"
            elif isinstance(data, dict):
                summary = f"keys: {', '.join(sorted(data.keys()))}" if data else "empty"
            print(f"[OK] {label:<16} -> {summary}")
        except Exception as exc:  # noqa: BLE001 - we want to report all failures uniformly
            msg = f"[FAIL] {label:<16} -> {exc}"
            print(msg)
            problems.append(msg)

    if problems:
        print("\nEncountered failures. Ensure the backend is running and retry.")
        return 1

    print("\nAll analysis API checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
