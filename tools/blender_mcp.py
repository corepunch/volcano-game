#!/usr/bin/env python3
"""Send a Python file to Blender's installed MCP socket bridge.

Start an isolated server with:
  Blender --background --online-mode --command blender_mcp --host 127.0.0.1 --port 9877
This uses the Blender extension's null-delimited execute protocol.
"""
import argparse
import json
import socket
from pathlib import Path


def execute(code, port=9877, timeout=3600):
    with socket.create_connection(("127.0.0.1", port), timeout=10) as client:
        client.settimeout(timeout)
        client.sendall(json.dumps(dict(type="execute", code=code, strict_json=True)).encode() + b"\0")
        chunks = bytearray()
        while b"\0" not in chunks:
            chunk = client.recv(65536)
            if not chunk:
                raise RuntimeError("Blender closed the connection before its response")
            chunks.extend(chunk)
    response = json.loads(chunks.split(b"\0", 1)[0])
    if response.get("status") != "ok":
        raise RuntimeError(json.dumps(response, indent=2))
    return response


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("script", type=Path)
    parser.add_argument("--port", type=int, default=9877)
    args = parser.parse_args()
    code = '__file__ = ' + repr(str(args.script.resolve())) + '\n' + args.script.read_text()
    print(json.dumps(execute(code, args.port), indent=2))
