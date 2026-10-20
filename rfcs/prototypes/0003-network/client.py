#!/usr/bin/env python3
"""A reference client for RFC 0003's connections and sign-in, and the checks that pin down its behaviour.

    python3 rfcs/prototypes/0003-network/client.py

It runs against mock_identity.py (started in-process on 127.0.0.1) and checks:
- the host's allowlist and the TLS rule (https everywhere, plain http only for the loopback), including the tricks
  that make a URL look allowed when it isn't;
- operations' path templates, whose arguments can't escape their place in the path;
- sign-in with PKCE and a loopback redirect (RFC 8252): a forged callback with the wrong `state` is ignored, a code
  works once, and a wrong verifier is refused;
- the device flow (RFC 8628), with `authorization_pending` and `slow_down`;
- requests that attach the token only to their own origin, refresh it once on a 401, and refuse a redirect to
  another origin;
- refresh failure and sign-out, which leave the connection signed out with nothing kept;
- and that no token ever reaches what a document can see (the `auth` variable), the log, or anything but the secret
  store and the request headers.

Only the standard library.
"""

import base64
import hashlib
import http.client
import http.server
import json
import secrets
import sys
import threading
import time
import urllib.parse

import mock_identity

LOOPBACK = ("127.0.0.1", "::1", "localhost")


def check_url(url, allowlist):
    """None if the host allows a connection to this URL, else the reason. The allowlist holds origins
    ("https://api.example.com") and wildcard origins ("https://*.example.com", any subdomain, not the bare domain)."""
    parts = urllib.parse.urlsplit(url)
    if parts.username is not None or parts.password is not None:
        return "a URL with a user name or password in it"
    host = (parts.hostname or "").lower()
    secure = parts.scheme in ("https", "wss")
    if not secure and not (parts.scheme in ("http", "ws") and host in LOOPBACK):
        return f"'{parts.scheme}' without TLS is only allowed for the loopback"
    port = parts.port or {"https": 443, "wss": 443, "http": 80, "ws": 80}[parts.scheme]
    for entry in allowlist:
        allowed = urllib.parse.urlsplit(entry)
        allowed_port = allowed.port or {"https": 443, "wss": 443, "http": 80, "ws": 80}[allowed.scheme]
        same_kind = {allowed.scheme, parts.scheme} <= {"https", "wss"} or allowed.scheme == parts.scheme
        pattern = (allowed.hostname or "").lower()
        if pattern.startswith("*."):
            host_matches = host.endswith(pattern[1:]) and host != pattern[2:]
        else:
            host_matches = host == pattern
        if same_kind and host_matches and port == allowed_port:
            return None
    return f"'{host}' isn't a host this app may connect to"


def expand(template, arguments):
    """An operation's path with its {name} placeholders filled: each value percent-encoded as one segment, so it
    can't add a '/', a '?' or a '..' step. Raises ValueError naming a missing argument or a value of the wrong type."""
    out, rest = [], template
    while "{" in rest:
        before, _, after = rest.partition("{")
        name, closed, rest = after.partition("}")
        if not closed:
            raise ValueError(f"path '{template}': a '{{' without its '}}'")
        if name not in arguments:
            raise ValueError(f"path '{template}': no value for '{name}'")
        value = arguments[name]
        if isinstance(value, bool) or not isinstance(value, (str, int, float)):
            raise ValueError(f"path '{template}': '{name}' must be text or a number")
        out += [before, urllib.parse.quote(str(value), safe="")]
    return "".join(out) + rest


class SecretStore:
    """The stand-in for the OS keychain (Keychain, Credential Manager, libsecret): the only place a refresh token
    is kept. The real one is a core interface with one backend per OS."""

    def __init__(self):
        self.items = {}

    def get(self, key):
        """A secret, or None."""
        return self.items.get(key)

    def set(self, key, value):
        """Keeps a secret."""
        self.items[key] = value

    def remove(self, key):
        """Forgets a secret."""
        self.items.pop(key, None)


class Connection:
    """One declared connection with OAuth sign-in. What a document sees is `auth()`; tokens stay private here."""

    def __init__(self, name, url, issuer, client, allowlist, secrets_store):
        self.name, self.url, self.issuer, self.client = name, url, issuer, client
        self.allowlist, self.secrets = allowlist, secrets_store
        self.state, self.user, self.device, self.error = "signedOut", None, None, None
        self._access = None  # in memory only, never in a document, state, history or log
        self.log = []
        self.refreshes = 0
        self.override_verifier = None  # a check sets it to send a wrong PKCE verifier
        self.last_code = None  # the last code exchanged, so a check can see it was single use
        for target in (url, issuer):
            reason = check_url(target, allowlist)
            if reason:
                raise ValueError(f"connections.{name}: {reason}")
        self.endpoints = self.fetch(issuer + "/.well-known/openid-configuration")[1]

    def auth(self):
        """The `auth.<name>` variable: what documents, state, peers and screenshots may see."""
        return {"state": self.state, "signedIn": self.state == "signedIn", "user": self.user,
                "device": self.device, "error": self.error}

    def fetch(self, url, method="GET", form=None, token=None):
        """One HTTP exchange, without following redirects: (status, JSON or None, headers)."""
        reason = check_url(url, self.allowlist)
        if reason:
            raise ValueError(reason)
        parts = urllib.parse.urlsplit(url)
        connection = http.client.HTTPConnection(parts.hostname, parts.port, timeout=5)
        headers = {"Authorization": "Bearer " + token} if token else {}
        body = urllib.parse.urlencode(form).encode() if form is not None else None
        if body is not None:
            headers["Content-Type"] = "application/x-www-form-urlencoded"
        connection.request(method, parts.path + ("?" + parts.query if parts.query else ""), body, headers)
        response = connection.getresponse()
        data = response.read(8 * 1024 * 1024 + 1)  # a body larger than the limit is refused
        if len(data) > 8 * 1024 * 1024:
            raise ValueError("a response larger than 8 MiB")
        self.log.append(f"{method} {parts.path} -> {response.status}")
        return response.status, (json.loads(data) if data else None), dict(response.getheaders())

    def keep(self, tokens):
        """Takes a token answer: the access token in memory, the refresh token in the secret store."""
        self._access = tokens["access_token"]
        self.secrets.set((self.name, "refresh"), tokens["refresh_token"])
        status, user, _ = self.fetch(self.endpoints["userinfo_endpoint"], token=self._access)
        self.user = {"name": user["name"], "email": user["email"]} if status == 200 else None
        self.state, self.device, self.error = "signedIn", None, None

    def sign_in_with_browser(self, browser, verifier=None):
        """PKCE with a loopback redirect. `browser(url)` plays the system browser; returns True when signed in."""
        verifier = verifier or secrets.token_urlsafe(48)
        state = secrets.token_urlsafe(16)
        challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()
        callback = Loopback(state)
        redirect = f"http://127.0.0.1:{callback.port}/callback"
        self.state = "signingIn"
        browser(self.endpoints["authorization_endpoint"] + "?" + urllib.parse.urlencode({
            "response_type": "code", "client_id": self.client, "redirect_uri": redirect, "scope": "openid profile",
            "state": state, "code_challenge": challenge, "code_challenge_method": "S256"}), callback)
        code = callback.wait(5)
        if not code:
            self.state, self.error = "signedOut", "the browser didn't come back"
            return False
        status, tokens, _ = self.fetch(self.endpoints["token_endpoint"], "POST", {
            "grant_type": "authorization_code", "code": code, "redirect_uri": redirect, "client_id": self.client,
            "code_verifier": self.override_verifier or verifier})
        if status != 200:
            self.state, self.error = "signedOut", tokens.get("error")
            return False
        self.last_code = code
        self.keep(tokens)
        return True

    def sign_in_with_device(self, approve):
        """The device flow: shows a code (in `auth().device`), polls at the interval, slows down when told."""
        status, grant, _ = self.fetch(self.endpoints["device_authorization_endpoint"], "POST",
                                      {"client_id": self.client})
        self.state = "signingIn"
        self.device = {"code": grant["user_code"], "url": grant["verification_uri"]}
        interval, polls, slowed = grant["interval"], 0, 0
        while True:
            if polls == 2:
                approve(grant["user_code"])  # the user approves in a browser, somewhere else
            status, tokens, _ = self.fetch(self.endpoints["token_endpoint"], "POST", {
                "grant_type": "urn:ietf:params:oauth:grant-type:device_code", "device_code": grant["device_code"],
                "client_id": self.client})
            polls += 1
            if status == 200:
                self.keep(tokens)
                return slowed
            if tokens["error"] == "slow_down":
                interval += 0.1
                slowed += 1
            elif tokens["error"] != "authorization_pending":
                self.state, self.error, self.device = "signedOut", tokens["error"], None
                return slowed
            time.sleep(interval if polls > 1 else 0.0)  # the first poll comes too fast on purpose: slow_down

    def refresh(self):
        """Trades the refresh token for new tokens; on failure the connection is signed out (expired)."""
        self.refreshes += 1
        token = self.secrets.get((self.name, "refresh"))
        status, tokens, _ = self.fetch(self.endpoints["token_endpoint"], "POST", {
            "grant_type": "refresh_token", "refresh_token": token or "", "client_id": self.client})
        if status == 200:
            self.keep(tokens)
            return True
        self._access = None
        self.secrets.remove((self.name, "refresh"))
        self.state, self.error = "expired", "signed out: sign in again"
        return False

    def request(self, path, arguments=None):
        """A read operation: (result, error). Refreshes once on a 401; refuses a redirect to another origin."""
        if self.state != "signedIn":
            return None, {"code": "signedOut"}
        url = self.url + expand(path, arguments or {})
        for attempt in range(2):
            status, body, headers = self.fetch(url, token=self._access)
            if status == 401 and attempt == 0 and self.refresh():
                continue
            if status == 401:
                return None, {"code": "signedOut"}
            if status in (301, 302, 303, 307, 308):
                target = urllib.parse.urljoin(url, headers.get("Location", ""))
                if origin(target) != origin(url):
                    return None, {"code": "redirect", "message": f"a redirect to another origin: {origin(target)}"}
            return (body, None) if status < 400 else (None, {"code": "status", "status": status})
        return None, {"code": "signedOut"}

    def sign_out(self):
        """Revokes the refresh token and forgets everything."""
        token = self.secrets.get((self.name, "refresh"))
        if token:
            self.fetch(self.endpoints["revocation_endpoint"], "POST", {"token": token, "client_id": self.client})
        self.secrets.remove((self.name, "refresh"))
        self._access, self.user, self.state = None, None, "signedOut"


def origin(url):
    """A URL's scheme, host and port."""
    parts = urllib.parse.urlsplit(url)
    return f"{parts.scheme}://{parts.hostname}:{parts.port}"


class Loopback:
    """The one-shot loopback listener of RFC 8252: 127.0.0.1 only, an ephemeral port, and a callback whose `state`
    doesn't match is answered 400 and ignored (a local program can't slip in its own code)."""

    def __init__(self, state):
        self.code, self.ignored = None, 0
        done = threading.Event()
        owner = self

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, format, *args):
                """Quiet."""

            def do_GET(self):
                """The browser's redirect, or something pretending to be it."""
                query = dict(urllib.parse.parse_qsl(urllib.parse.urlsplit(self.path).query))
                good = query.get("state") == state and "code" in query
                self.send_response(200 if good else 400)
                self.end_headers()
                self.wfile.write(b"You can close this window." if good else b"Not for this sign-in.")
                if good:
                    owner.code = query["code"]
                    done.set()
                else:
                    owner.ignored += 1

        self.server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
        self.port = self.server.server_address[1]
        self.done = done
        threading.Thread(target=self.serve, daemon=True).start()

    def serve(self):
        """Answers until the right callback arrives."""
        while not self.done.is_set():
            self.server.handle_request()

    def wait(self, seconds):
        """The code, or None after the timeout; the listener is closed either way."""
        self.done.wait(seconds)
        self.done.set()
        self.server.server_close()
        return self.code


def system_browser(attacker_first=False):
    """Plays the system browser: follows the authorization redirect to the loopback. With `attacker_first`, a local
    program hits the loopback with its own code before the browser does."""

    def open_url(url, callback):
        if attacker_first:
            get(f"http://127.0.0.1:{callback.port}/callback?code=evil&state=guess")
        status, headers = get(url)
        get(headers["Location"])

    return open_url


def get(url):
    """A GET that doesn't follow redirects: (status, headers)."""
    parts = urllib.parse.urlsplit(url)
    connection = http.client.HTTPConnection(parts.hostname, parts.port, timeout=5)
    connection.request("GET", parts.path + "?" + parts.query)
    response = connection.getresponse()
    response.read()
    return response.status, dict(response.getheaders())


def check(name, condition):
    """Prints one check's result; stops at the first failure."""
    print(("ok    " if condition else "FAIL  ") + name)
    if not condition:
        sys.exit(1)


def check_rules():
    """The allowlist, the TLS rule and path templates."""
    allow = ["https://api.example.com", "https://*.example.org", "http://127.0.0.1:8080"]
    check("an allowed https origin is allowed", check_url("https://api.example.com/v1/x", allow) is None)
    check("wss to an allowed https origin is allowed", check_url("wss://api.example.com/live", allow) is None)
    check("plain http to another host is refused", "TLS" in check_url("http://api.example.com/", allow))
    check("http to the loopback is allowed when listed", check_url("http://127.0.0.1:8080/x", allow) is None)
    check("a host that only ends like an allowed one is refused",
          check_url("https://api.example.com.evil.net/", allow) is not None)
    check("a user name that hides the real host is refused",
          check_url("https://api.example.com@evil.net/", allow) is not None)
    check("a wildcard covers subdomains", check_url("https://eu.example.org/", allow) is None)
    check("a wildcard doesn't cover the bare domain", check_url("https://example.org/", allow) is not None)
    check("another port is another origin", check_url("https://api.example.com:8443/", allow) is not None)
    check("a path argument can't climb out of its segment",
          expand("/projects/{id}/files", {"id": "../admin?x=1"}) == "/projects/..%2Fadmin%3Fx%3D1/files")
    check("a path argument keeps text whole", expand("/notes/{name}", {"name": "café notes"}) ==
          "/notes/caf%C3%A9%20notes")
    for arguments, problem in (({}, "no value for 'id'"), ({"id": {"a": 1}}, "must be text or a number")):
        try:
            expand("/projects/{id}", arguments)
            refused = ""
        except ValueError as error:
            refused = str(error)
        check(f"a path template refuses: {problem}", problem in refused)


def check_sign_in():
    """The flows, refresh, redirects, sign-out, and where tokens may go."""
    server, issuer, identity = mock_identity.start()
    store = SecretStore()
    allow = [issuer]
    api = Connection("api", issuer + "/api", issuer, mock_identity.CLIENT_ID, allow, store)

    check("a forged callback is ignored, and the real sign-in still works",
          api.sign_in_with_browser(system_browser(attacker_first=True)) and api.auth()["user"]["name"] ==
          "Ada Lovelace")
    with identity.lock:
        reused = api.last_code in identity.codes
    check("a code is gone once it is used", not reused)
    projects, error = api.request("/projects")
    check("a signed-in request gets its result", error is None and projects[0]["name"] == "Engine")

    time.sleep(mock_identity.ACCESS_SECONDS + 0.2)
    projects, error = api.request("/projects")
    check("an expired access token is refreshed once and the request retried", error is None and api.refreshes == 1)

    projects, error = api.request("/moved")
    check("a redirect to another origin is refused, without sending the token there",
          error and error["code"] == "redirect")

    other = Connection("other", issuer + "/api", issuer, mock_identity.CLIENT_ID, allow, SecretStore())
    other.override_verifier = "not-the-verifier"
    check("a wrong PKCE verifier is refused", not other.sign_in_with_browser(system_browser()) and
          other.auth()["state"] == "signedOut")

    device = Connection("device", issuer + "/api", issuer, mock_identity.CLIENT_ID, allow, SecretStore())
    approve = lambda code: device.fetch(issuer + "/device/approve", "POST", {"user_code": code})
    slowed = device.sign_in_with_device(approve)
    check("the device flow signs in after the user approves, slowing down when told",
          device.auth()["signedIn"] and slowed >= 1)

    with identity.lock:
        identity.refresh.clear()  # the server revokes every session
        identity.access.clear()
    projects, error = api.request("/projects")
    check("when refresh fails, the request fails and the connection is expired",
          error == {"code": "signedOut"} and api.auth()["state"] == "expired" and store.get(("api", "refresh")) is
          None)

    device_token = device.secrets.get(("device", "refresh"))
    device.sign_out()
    with identity.lock:
        revoked = device_token not in identity.refresh
    check("signing out revokes the refresh token and forgets it",
          revoked and device.secrets.get(("device", "refresh")) is None and not device.auth()["signedIn"])

    visible = json.dumps([api.auth(), device.auth(), other.auth(), api.log, device.log, other.log])
    with identity.lock:
        issued = list(identity.issued)
    check(f"none of the {len(issued)} tokens issued reaches the auth variable or the log",
          len(issued) == 6 and all(token not in visible for token in issued) and
          "Bearer" not in visible and "access_token" not in visible)
    server.shutdown()


if __name__ == "__main__":
    check_rules()
    check_sign_in()
    print("all checks passed")
