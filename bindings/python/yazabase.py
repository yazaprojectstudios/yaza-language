"""
YazaBase Python Driver — Embedded Database Engine
Usable from FastAPI, Flask, Django, Data Science, and standard Python scripts.
"""

import subprocess
import csv
import io
import os
import shutil
from typing import List, Dict, Any, Optional

class YazaBaseError(Exception):
    pass

class YazaBaseConnection:
    def __init__(self, dir_path: str, db_name: str, cli_path: Optional[str] = None):
        self.dir_path = os.path.abspath(dir_path)
        self.db_name = db_name
        self.cli_path = cli_path or self._find_cli()

        # Ensure directory exists and initialize DB if needed
        os.makedirs(self.dir_path, exist_ok=True)
        db_file = os.path.join(self.dir_path, f"{db_name}.ydb")
        if not os.path.exists(db_file):
            self._init_db()

    def _find_cli(self) -> str:
        candidates = [
            "yazabase",
            "yazabase-cli",
            os.path.expanduser("~/.yaza/bin/yazabase"),
            os.path.expanduser("~/.yazabase/bin/yazabase"),
            "/usr/local/bin/yazabase",
            os.path.join(os.path.dirname(__file__), "../../build/yazabase-cli"),
        ]
        for c in candidates:
            resolved = shutil.which(c) or (os.path.isfile(c) and os.access(c, os.X_OK) and c)
            if resolved:
                return resolved
        return "yazabase"

    def _init_db(self):
        cmd = [self.cli_path, "init", self.dir_path, self.db_name]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            raise YazaBaseError(f"Failed to initialize database: {res.stderr or res.stdout}")

    def execute(self, sql: str) -> List[Dict[str, Any]]:
        cmd = [self.cli_path, "sql", "--csv", self.dir_path, self.db_name, sql]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            raise YazaBaseError(f"SQL execution failed: {res.stderr or res.stdout}")

        output = res.stdout.strip()
        if not output:
            return []

        reader = csv.DictReader(io.StringIO(output))
        rows = []
        for r in reader:
            # Clean up null markers
            cleaned = {k: (None if v == "\\N" else v) for k, v in r.items()}
            rows.append(cleaned)
        return rows

def connect(dir_path: str, db_name: str = "main", cli_path: Optional[str] = None) -> YazaBaseConnection:
    return YazaBaseConnection(dir_path, db_name, cli_path)
