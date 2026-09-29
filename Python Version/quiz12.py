import http.server
import socketserver
import webbrowser
import os
import threading
import sqlite3
import json
import urllib.parse
from datetime import datetime

PORT = 8000
DIRECTORY = os.path.dirname(os.path.abspath(__file__))
DB_PATH = os.path.join(DIRECTORY, "kbc_history.db")


# ─────────────────────────────── Database Setup ───────────────────────────────
def init_db():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("""
        CREATE TABLE IF NOT EXISTS game_history (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            player_name TEXT    NOT NULL,
            location    TEXT,
            occupation  TEXT,
            prize_won   INTEGER NOT NULL DEFAULT 0,
            questions_answered INTEGER NOT NULL DEFAULT 0,
            outcome     TEXT    NOT NULL DEFAULT 'quit',   -- 'won', 'wrong', 'timeout', 'quit'
            played_at   TEXT    NOT NULL
        )
    """)
    conn.commit()
    conn.close()
    print("[DB] Database initialized at:", DB_PATH)


def save_game(data: dict):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute("""
        INSERT INTO game_history (player_name, location, occupation, prize_won, questions_answered, outcome, played_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    """, (
        data.get("player_name", "Unknown"),
        data.get("location", ""),
        data.get("occupation", ""),
        int(data.get("prize_won", 0)),
        int(data.get("questions_answered", 0)),
        data.get("outcome", "quit"),
        datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    ))
    conn.commit()
    conn.close()


def get_history(limit: int = 20):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    c = conn.cursor()
    rows = c.execute("""
        SELECT * FROM game_history ORDER BY played_at DESC LIMIT ?
    """, (limit,)).fetchall()
    conn.close()
    return [dict(r) for r in rows]


def get_stats():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    row = c.execute("""
        SELECT
            COUNT(*)                AS total_games,
            COALESCE(MAX(prize_won), 0)  AS highest_prize,
            COALESCE(SUM(prize_won), 0)  AS total_prize,
            COALESCE(AVG(questions_answered), 0) AS avg_questions,
            SUM(CASE WHEN outcome='won' THEN 1 ELSE 0 END) AS wins
        FROM game_history
    """).fetchone()
    conn.close()
    return {
        "total_games":    row[0],
        "highest_prize":  row[1],
        "total_prize":    row[2],
        "avg_questions":  round(row[3], 1),
        "wins":           row[4],
    }


# ─────────────────────────────── HTTP Handler ─────────────────────────────────
class KBCHandler(http.server.SimpleHTTPRequestHandler):

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=DIRECTORY, **kwargs)

    def log_message(self, format, *args):
        pass  # suppress access log noise

    def _send_json(self, data, status=200):
        body = json.dumps(data).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = parsed.path

        if path == "/api/history":
            self._send_json(get_history())
        elif path == "/api/stats":
            self._send_json(get_stats())
        else:
            super().do_GET()

    def do_POST(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/api/save":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length)
            try:
                data = json.loads(body)
                save_game(data)
                self._send_json({"status": "ok"})
            except Exception as e:
                self._send_json({"status": "error", "message": str(e)}, 400)
        else:
            self.send_error(404)


# ─────────────────────────────── Main ─────────────────────────────────────────
def run_server():
    with socketserver.TCPServer(("", PORT), KBCHandler) as httpd:
        httpd.allow_reuse_address = True
        print(f"[KBC] Server running at  http://localhost:{PORT}")
        httpd.serve_forever()


if __name__ == "__main__":
    init_db()
    t = threading.Thread(target=run_server, daemon=True)
    t.start()
    webbrowser.open(f"http://localhost:{PORT}/index.html")
    print("[KBC] Game launched in your browser. Press Ctrl+C or Enter to stop.")
    try:
        input()
    except KeyboardInterrupt:
        pass
