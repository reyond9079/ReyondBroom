"""Inspect or moderate a local Mapping Jam with the operator's key file."""

import argparse
import json
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import build_opener, ProxyHandler, Request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8770)
    parser.add_argument("--key-file", type=Path, default=Path("out/jam-v2/moderator.key"))
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("reports")
    commands.add_parser("audit")
    for category, actions in (("report", ("resolve", "dismiss")),
                              ("revision", ("hide", "restore")),
                              ("account", ("restrict", "restore"))):
        operation = commands.add_parser(category)
        operation.add_argument("id")
        operation.add_argument("action", choices=actions)
        operation.add_argument("--reason", required=True)
    args = parser.parse_args()
    if not 1024 <= args.port <= 65535:
        parser.error("Invalid local port.")
    body = None
    path = args.command
    if hasattr(args, "id"):
        if len(args.id) != 32 or any(c not in "0123456789abcdef" for c in args.id):
            parser.error("Expected the 32-character target ID.")
        path = args.command + "s/" + args.id
        body = json.dumps({"action": args.action, "reason": args.reason}).encode()
    request = Request(f"http://127.0.0.1:{args.port}/v1/admin/{path}", data=body,
                      method="PATCH" if body else "GET",
                      headers={"Authorization": "Bearer " + args.key_file.read_text(encoding="ascii").strip(),
                               "Content-Type": "application/json"})
    try:
        with build_opener(ProxyHandler({})).open(request, timeout=10) as response:
            print(json.dumps(json.load(response), ensure_ascii=False, indent=2))
    except HTTPError as error:
        parser.exit(1, f"Moderator API returned {error.code}: {json.load(error).get('title', '')}\n")


if __name__ == "__main__":
    main()
