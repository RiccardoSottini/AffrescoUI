"""What the server tells the agent of the host's replies, and its answers to messages it can't use."""


def scenarios_report(report, shown):
    """affrescoui test --json's report of one file, as a few lines: the totals, then one line per failure."""
    lines = [f"{report.get('file')} on {shown}: {report.get('passed')} passed, {report.get('failed')} failed"]
    for failure in report.get("failures") or []:
        step = f", step {failure['step']}" if failure.get("step") else ""
        lines.append(f"FAILED {failure.get('scenario')}{step}: {failure.get('problem')}")
    return "\n".join(lines)


# Permissions, budgets, data rules, roles and leases (VERSION_CONTROL.md): what a denial means, from the rule it names
DENIALS = (
    ("over the budget", "the screen's permissions limit how many nodes you add, remove and change in one revision; "
                        "split the change into smaller revisions"),
    ("data rule", "the screen's data rules limit the paths you may read and write; change only the paths they allow"),
    ("holds a lease", "another author is working on that part of the screen; wait for its lease to end or change "
                      "another part"),
    ("(rule \"", "a deny rule of the screen's permissions covers what you changed; leave that part as it is"),
    ("revisions can't change", "that section is a capability of the app, which no revision changes"),
)


def host_failure(reply):
    """The answer when the host refused a read: its error, else its message.

    @param reply  the host's reply
    @return (text, True)
    """
    return f"error: {reply.get('error') or reply.get('message')}", True


def explain_denial(error):
    """The error of a refused revision, followed by what the rule it names means, if it is a permission denial."""
    text = str(error)
    if "permission denied" not in text:
        return text
    for marker, meaning in DENIALS:
        if marker in text:
            return f"{text}. The rule that matched: {meaning}."
    return text


def describe(reply):
    kind = reply.get("type")
    if kind == "applied":
        return (f"applied: the screen is now at revision {reply.get('revision')} "
                f"(commit {str(reply.get('commit', ''))[:7]})"), False
    if kind == "proposed":
        return (f"proposed as proposal {reply.get('proposal')} (head {str(reply.get('commit', ''))[:7]}): a reviewer "
                f"decides in the Studio. It is not applied yet; get_events will report the decision. Pass "
                f"proposal={reply.get('proposal')} to submit_revision to add to it."), False
    return f"rejected, nothing changed: {explain_denial(reply.get('error'))}", True


def screen_info(reply):
    """What get_screen says of the screen besides its document: revision, head, your author name, whether you can
    review, the mode, the grants and the open proposals."""
    head = reply.get("head") or {}
    return {"revision": reply.get("revision"), "head": head.get("short"), "you": reply.get("you"),
            "canReview": reply.get("canReview"), "mode": reply.get("mode"),
            "grants": [g.get("rule") for g in reply.get("grants") or []],
            "proposals": [{k: p.get(k) for k in ("id", "author", "message", "touches", "changes")}
                          for p in reply.get("proposals") or []]}


def malformed(request):
    """The error for a message the server can't use, or None: a batch (a JSON array, which MCP dropped in 2025-06-18),
    anything else that isn't an object, or params that aren't one. The server answers it and reads on."""
    if isinstance(request, dict) and isinstance(request.get("params", {}), dict):
        return None
    return {"jsonrpc": "2.0", "id": request.get("id") if isinstance(request, dict) else None,
            "error": {"code": -32600, "message": "invalid request: expected one JSON-RPC object"}}


def client_name(params):
    """The name the client gives itself in initialize (the revisions' author after "agent/"), or "agent"."""
    client = params.get("clientInfo")
    name = client.get("name") if isinstance(client, dict) else None
    return name if isinstance(name, str) and name else "agent"
