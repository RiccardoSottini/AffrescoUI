"""Undoing an earlier revision and keeping what was built on it (SCREEN_FORMAT.md, "Undoing an earlier commit"): the
revert_revision tool's choices, checked as its schema says before the host is asked, and the plan cut to what an
agent needs."""

# The choices revert and undoPlan both take, in the order they are sent
CHOICES = ("drop", "keep", "then", "head")

# The plan's keys an agent is given as they are; `of` is cut to a few of its own, and the rest (the lines of the
# change, its impact, the open proposals) stays with the host
PLAN_KEYS = ("outcome", "blocked", "head", "later", "laterMore", "conflicts", "conflictsMore", "state", "warnings")
COMMIT_KEYS = ("number", "short", "message")


def is_paths(value):
    """Whether a drop or keep choice is as the schema says: a list of conflict paths, or "all"."""
    return value == "all" or (isinstance(value, list) and all(isinstance(path, str) for path in value))


def choice_error(args):
    """Why the tool's arguments break its schema, or None: the error names the argument, and nothing is sent.

    @param args  the call's arguments
    @return the error text, or None
    """
    revision = args.get("revision")
    if isinstance(revision, bool) or not isinstance(revision, (int, str)):
        return "error: revision: expected a revision number or a commit id"
    for key in ("drop", "keep"):
        if key in args and not is_paths(args[key]):
            return f'error: {key}: expected a list of conflict paths or "all"'
    then = args.get("then", [])
    if not isinstance(then, list) or not all(isinstance(operation, dict) for operation in then):
        return "error: then: expected a JSON Patch, a list of operations"
    for key in ("head", "message"):
        if key in args and not isinstance(args[key], str):
            return f"error: {key}: expected text"
    if "dry_run" in args and not isinstance(args["dry_run"], bool):
        return "error: dry_run: expected true or false"
    return None


def undo_message(kind, args, author):
    """The message to the host: `undoPlan` (a dry run) or `revert`, with each choice given and no other.

    @param kind  "undoPlan" or "revert"
    @param args  the call's arguments, already checked
    @param author  the connection's author name, for a revert
    @return the message
    """
    message = {"type": kind, "revision": args.get("revision")}
    if kind == "revert":
        message["author"] = author
    for key in CHOICES + (("message",) if kind == "revert" else ()):
        if key in args:
            message[key] = args[key]
    return message


def plan_summary(plan):
    """The plan as an agent is given it: what decides the next call, and of the commit to undo only its number, short
    id and message. A count of more (laterMore, conflictsMore) is kept only when there are more.

    @param plan  the host's plan
    @return the summary
    """
    if not isinstance(plan, dict):
        return plan
    summary = {key: plan[key] for key in PLAN_KEYS if key in plan and not (key.endswith("More") and not plan[key])}
    commit = plan.get("of")
    if isinstance(commit, dict):
        summary = {"of": {key: commit[key] for key in COMMIT_KEYS if key in commit}, **summary}
    return summary
