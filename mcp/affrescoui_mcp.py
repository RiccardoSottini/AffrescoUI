#!/usr/bin/env python3
"""AffrescoUI MCP server, run from a checkout: `python3 mcp/affrescoui_mcp.py [--socket DIR] [--launch PROGRAM]`.

The server is the package next to this file (affrescoui_mcp/, README.md says what each module does). This file keeps
the path that MCP clients, scripts and documents name: run, it starts the server; imported, or loaded by path as
tests/perf/tokens.py does, it offers every name the package does.
"""

import os
import sys

# Loaded by path, this folder isn't on sys.path; run, it is already. The package folder wins over this file there.
HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

from affrescoui_mcp import *  # noqa: E402,F401,F403 (the package's names, as this file had them)
from affrescoui_mcp import main  # noqa: E402

if __name__ == "__main__":
    main()
