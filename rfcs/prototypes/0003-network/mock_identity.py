#!/usr/bin/env python3
"""A mock identity provider and API on 127.0.0.1, for RFC 0003's sign-in checks (client.py starts it in-process).

It implements just enough of OAuth 2.0 and OpenID Connect for a native app:
- discovery (`/.well-known/openid-configuration`);
- the authorization code flow with PKCE (RFC 7636, S256 only) and a loopback redirect on any port (RFC 8252,
  section 7.3); codes are single use and expire;
- the device authorization grant (RFC 8628), with `authorization_pending` and `slow_down`;
- refresh tokens that rotate, and revocation (RFC 7009);
- `/userinfo` and a protected `/api/projects`, which answer 401 to a missing, expired or revoked token.

The real 8.6c also validates the ID token's signature against the issuer's keys; the standard library has no RSA, so
this mock gives the user's claims through `/userinfo` only. A test hook, `/device/approve`, plays the user who
approves the device code in a browser.
"""

import base64
import hashlib
import json
import secrets
import threading
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

CLIENT_ID = "demo-desktop"
ACCESS_SECONDS = 1.0  # short, so a check can watch it expire


class Identity:
    """The provider's state, shared by the request handlers (guarded by a lock: the server is threaded)."""

    def __init__(self):
        self.lock = threading.Lock()
        self.codes = {}  # code -> {challenge, redirect, expires}
        self.access = {}  # access token -> expiry time
        self.refresh = set()  # live refresh tokens
        self.devices = {}  # device code -> {user_code, approved, last_poll, interval}
        self.issued = []  # every token ever issued, so a check can look for them where they mustn't be

    def issue(self):
        """A fresh access token and refresh token."""
        access, refresh = secrets.token_urlsafe(24), secrets.token_urlsafe(24)
        self.access[access] = time.monotonic() + ACCESS_SECONDS
        self.refresh.add(refresh)
        self.issued += [access, refresh]
        return {"access_token": access, "token_type": "Bearer", "expires_in": ACCESS_SECONDS,
                "refresh_token": refresh, "scope": "openid profile projects"}

    def valid(self, header):
        """Whether an Authorization header carries a live access token."""
        token = header[7:] if header and header.startswith("Bearer ") else None
        return token in self.access and self.access[token] > time.monotonic()


def challenge_of(verifier):
    """PKCE's S256: the base64url SHA-256 of the verifier, without padding."""
    return base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()


def make_handler(identity, issuer):
    """The request handler class, bound to one provider."""

    class Handler(BaseHTTPRequestHandler):
        """Answers the endpoints listed in the module's docstring."""

        def log_message(self, format, *args):
            """Quiet: the checks print their own lines."""

        def reply(self, status, body=None, headers=None):
            """Writes a JSON answer (or none)."""
            data = json.dumps(body).encode() if body is not None else b""
            self.send_response(status)
            for name, value in (headers or {}).items():
                self.send_header(name, value)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            self.wfile.write(data)

        def form(self):
            """The request's form body."""
            length = int(self.headers.get("Content-Length", 0))
            return {k: v[0] for k, v in urllib.parse.parse_qs(self.rfile.read(length).decode()).items()}

        def do_GET(self):
            """Discovery, the authorization endpoint, userinfo and the API."""
            url = urllib.parse.urlsplit(self.path)
            query = {k: v[0] for k, v in urllib.parse.parse_qs(url.query).items()}
            if url.path == "/.well-known/openid-configuration":
                return self.reply(200, {"issuer": issuer, "authorization_endpoint": issuer + "/authorize",
                                        "token_endpoint": issuer + "/token",
                                        "device_authorization_endpoint": issuer + "/device",
                                        "userinfo_endpoint": issuer + "/userinfo",
                                        "revocation_endpoint": issuer + "/revoke",
                                        "code_challenge_methods_supported": ["S256"]})
            if url.path == "/authorize":
                return self.authorize(query)
            if url.path == "/api/moved":  # a redirect to another origin (localhost isn't 127.0.0.1)
                port = self.server.server_address[1]
                return self.reply(302, None, {"Location": f"http://localhost:{port}/api/projects"})
            with identity.lock:
                authorized = identity.valid(self.headers.get("Authorization"))
            if not authorized:
                return self.reply(401, {"error": "invalid_token"}, {"WWW-Authenticate": "Bearer"})
            if url.path == "/userinfo":
                return self.reply(200, {"sub": "u-42", "name": "Ada Lovelace", "email": "ada@example.com"})
            if url.path == "/api/projects":
                return self.reply(200, [{"id": "p1", "name": "Engine"}, {"id": "p2", "name": "Notes"}])
            return self.reply(404, {"error": "not_found"})

        def authorize(self, query):
            """The user approves at once: redirects to the loopback with a single-use code."""
            redirect = urllib.parse.urlsplit(query.get("redirect_uri", ""))
            loopback = redirect.scheme == "http" and redirect.hostname in ("127.0.0.1", "::1")
            if query.get("client_id") != CLIENT_ID or not loopback or query.get("code_challenge_method") != "S256":
                return self.reply(400, {"error": "invalid_request"})
            code = secrets.token_urlsafe(16)
            with identity.lock:
                identity.codes[code] = {"challenge": query["code_challenge"], "redirect": query["redirect_uri"],
                                        "expires": time.monotonic() + 60}
            location = query["redirect_uri"] + "?" + urllib.parse.urlencode({"code": code, "state": query["state"]})
            self.reply(302, None, {"Location": location})

        def do_POST(self):
            """The token endpoint, device authorization, revocation and the approval hook."""
            fields = self.form()
            with identity.lock:
                if self.path == "/token":
                    status, body = self.token(fields)
                elif self.path == "/device":
                    device, user = secrets.token_urlsafe(16), secrets.token_hex(3).upper()
                    identity.devices[device] = {"user_code": user, "approved": False, "last_poll": 0.0,
                                                "interval": 0.2}
                    status, body = 200, {"device_code": device, "user_code": user, "interval": 0.2,
                                         "verification_uri": issuer + "/activate", "expires_in": 60}
                elif self.path == "/device/approve":
                    for device in identity.devices.values():
                        device["approved"] |= device["user_code"] == fields.get("user_code")
                    status, body = 200, {}
                elif self.path == "/revoke":
                    identity.refresh.discard(fields.get("token"))
                    identity.access.pop(fields.get("token"), None)
                    status, body = 200, {}
                else:
                    status, body = 404, {"error": "not_found"}
            self.reply(status, body)

        def token(self, fields):
            """The three grants; called with the lock held."""
            grant = fields.get("grant_type")
            if grant == "authorization_code":
                code = identity.codes.pop(fields.get("code"), None)  # single use: gone even if what follows fails
                if (not code or code["expires"] < time.monotonic() or code["redirect"] != fields.get("redirect_uri")
                        or challenge_of(fields.get("code_verifier", "")) != code["challenge"]):
                    return 400, {"error": "invalid_grant"}
                return 200, identity.issue()
            if grant == "refresh_token":
                if fields.get("refresh_token") not in identity.refresh:
                    return 400, {"error": "invalid_grant"}
                identity.refresh.discard(fields["refresh_token"])  # rotation
                return 200, identity.issue()
            if grant == "urn:ietf:params:oauth:grant-type:device_code":
                device = identity.devices.get(fields.get("device_code"))
                if not device:
                    return 400, {"error": "expired_token"}
                now = time.monotonic()
                too_fast = now - device["last_poll"] < device["interval"]
                device["last_poll"] = now
                if too_fast:
                    device["interval"] += 0.1
                    return 400, {"error": "slow_down"}
                if not device["approved"]:
                    return 400, {"error": "authorization_pending"}
                del identity.devices[fields["device_code"]]
                return 200, identity.issue()
            return 400, {"error": "unsupported_grant_type"}

    return Handler


def start():
    """Starts the provider on an ephemeral port of 127.0.0.1; returns (server, issuer URL, identity)."""
    identity = Identity()
    server = ThreadingHTTPServer(("127.0.0.1", 0), None)
    issuer = f"http://127.0.0.1:{server.server_address[1]}"
    server.RequestHandlerClass = make_handler(identity, issuer)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server, issuer, identity
