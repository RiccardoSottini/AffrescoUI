"""Using the app: the list_commands, run_command and undo_run tools. The host lists an app's commands, runs one for
this server's agent under the app's run rules (`permissions.run`), and undoes a run. The server never allows or
denies a run, and never retries one that was refused.

What the host sends here (a command's name, title and description, an argument's name, a reason) was written by
the people and peers who wrote the app: every piece of it is printable() on its line, after the name, so it can't
start a line of its own or read as an instruction of this server's."""

import json

from .host import printable

MAX_COMMAND_LINES = 200  # commands list_commands shows; `filter` narrows the rest
MAX_PATCH_LINES = 20  # operations of a run's changes shown; the rest are counted
MAX_TEXT = 200  # characters of a description, a default or a value shown, the rest cut with "…"
DEFAULT_WAIT = 60  # seconds run_command waits for a run that waits for a yes
MAX_WAIT = 600
OLDER_HOST = "this host is older and has no commands"
BADGES = {"ask": "[ask]", "person": "[person]"}  # "allow" is the common case and has none; "deny" says why


def shortened(text):
    """Text on one line, cut to MAX_TEXT characters."""
    text = printable(text)
    return text if len(text) <= MAX_TEXT else text[:MAX_TEXT - 1] + "…"


def value_text(value):
    """A value as an argument's default or an enum's choice shows it: a string as itself, anything else as JSON."""
    return shortened(value if isinstance(value, str) else json.dumps(value, separators=(",", ":")))


def nullable(schema):
    """True if an argument's type lets it be null."""
    kinds = schema.get("type")
    return (isinstance(kinds, list) and "null" in kinds) or (isinstance(schema.get("enum"), list)
                                                              and None in schema["enum"])


def type_text(schema):
    """An argument's type in a few words: its enum's values joined by "|", a constant, a basic type, "object",
    "list of <type>", or "any" for {} (and anything that isn't a schema). Null is said by the name's "?" instead."""
    if not isinstance(schema, dict):
        return "any"
    if isinstance(schema.get("enum"), list):
        return "|".join(value_text(choice) for choice in schema["enum"] if choice is not None) or "null"
    if "const" in schema:
        return value_text(schema["const"])
    return kinds_text(schema)


def kinds_text(schema):
    """An argument's basic types joined by "|": "list of <type>" for an array, null left out beside another type."""
    kinds = schema.get("type")
    kinds = [kinds] if isinstance(kinds, str) else [kind for kind in kinds or [] if isinstance(kind, str)]
    words = [f"list of {type_text(schema.get('items'))}" if kind == "array" else printable(kind)
             for kind in kinds if kind != "null" or len(kinds) == 1]
    return "|".join(words) or "any"


def argument_text(name, schema, described):
    """One argument as list_commands shows it: `name: type`, `name?` when it has a default or may be null, then
    ` = default`; with `described`, its description in brackets."""
    schema = schema if isinstance(schema, dict) else {}
    optional = "default" in schema or nullable(schema)
    text = f"{printable(name)}{'?' if optional else ''}: {type_text(schema)}"
    if "default" in schema:
        text += f" = {value_text(schema['default'])}"
    if described and isinstance(schema.get("description"), str) and schema["description"]:
        text += f" ({shortened(schema['description'])})"
    return text


def badge(command):
    """What a command's line says of running it: nothing when it runs at once, [ask], [person] or [deny: why], and
    [disabled now] when its enablement is false."""
    run = command.get("run")
    marks = []
    if run == "deny":
        reason = command.get("reason")
        marks.append(f"[deny: {shortened(reason)}]" if isinstance(reason, str) and reason else "[deny]")
    elif run in BADGES:
        marks.append(BADGES[run])
    elif run != "allow":
        marks.append(f"[{shortened(run)}]")
    if command.get("enabled") is False:
        marks.append("[disabled now]")
    return " ".join(marks)


def command_line(command, described=False):
    """One command on one line: `name(arguments)  description.  [badge]`, the title when it has no description."""
    arguments = command.get("arguments")
    if isinstance(arguments, list):  # names alone: each takes any value
        arguments = {name: {} for name in arguments if isinstance(name, str)}
    listed = ", ".join(argument_text(name, schema, described)
                       for name, schema in (arguments.items() if isinstance(arguments, dict) else []))
    line = f"{printable(command.get('name'))}({listed})"
    words = command.get("description") or command.get("title")
    if isinstance(words, str) and words:
        line += f"  {shortened(words)}"
    marks = badge(command)
    return f"{line}  {marks}" if marks else line


def command_list(reply, described=False):
    """A `commands` answer as list_commands gives it: `<n> commands` (`, <m> shown` when cut), then a line each."""
    commands = [command for command in reply.get("commands") or [] if isinstance(command, dict)]
    shown = commands[:MAX_COMMAND_LINES]
    header = f"{len(commands)} command{'' if len(commands) == 1 else 's'}"
    lines = [header + (f", {len(shown)} shown" if len(shown) < len(commands) else "")]
    lines += [command_line(command, described) for command in shown]
    if len(shown) < len(commands):
        lines.append(f"… {len(commands) - len(shown)} more left out: pass filter to narrow the list")
    return "\n".join(lines)


def operation_line(operation):
    """One JSON Patch operation on one line: `replace /state/x 3`, `remove /state/y`, `move /from -> /to`."""
    if not isinstance(operation, dict):
        return shortened(json.dumps(operation))
    line = f"{printable(operation.get('op'))} "
    if "from" in operation:
        line += f"{printable(operation['from'])} -> "
    line += printable(operation.get("path"))
    if "value" in operation:
        line += f" {shortened(json.dumps(operation['value'], separators=(',', ':')))}"
    return line


def patch_lines(patch, truncated=False):
    """A run's changes, one operation a line, at most MAX_PATCH_LINES, then `… <k> more`."""
    lines = [operation_line(operation) for operation in patch[:MAX_PATCH_LINES]]
    if len(patch) > MAX_PATCH_LINES:
        lines.append(f"… {len(patch) - MAX_PATCH_LINES} more")
    if truncated:
        lines.append("… more the host left out")
    return lines


def changes_text(reply):
    """How many changes an answer says a run made (or put back), then each on its line."""
    patch = reply.get("changed")
    if not isinstance(patch, list):
        count = reply.get("changes")
        return f"{count} change{'' if count == 1 else 's'}" if isinstance(count, int) else "no change listed"
    if not patch:
        return "no change"
    head = f"{len(patch)} change{'' if len(patch) == 1 else 's'}"
    return "\n".join([head, *patch_lines(patch, reply.get("truncated") is True)])


def records_text(records):
    """The records a run wrote: `records: cards +1 ~0 -0`, a collection each."""
    parts = [f"{printable(name)} +{printable(counts.get('inserted', 0))} ~{printable(counts.get('updated', 0))} "
             f"-{printable(counts.get('deleted', 0))}"
             for name, counts in records.items() if isinstance(counts, dict)]
    return f"records: {', '.join(parts)}" if parts else None


def no_visible_change(region):
    """True if a run's region of changed pixels is empty (all zeros)."""
    values = region.values() if isinstance(region, dict) else region
    return all(value == 0 for value in values)


def ran_text(reply, pictured):
    """A `ran` answer: what ran and what it changed, its errors, the records, and how to undo it."""
    lines = [f"ran {printable(reply.get('command'))} (run {reply.get('run')}): {changes_text(reply)}"]
    errors = [shortened(error) for error in reply.get("errors") or []]
    if errors:
        lines.append(f"errors: {'; '.join(errors)}")
    if reply.get("failed"):
        lines.append("failed: the run stopped and what it changed was rolled back")
    if isinstance(reply.get("records"), dict) and records_text(reply["records"]):
        lines.append(records_text(reply["records"]))
    if pictured and isinstance(reply.get("region"), (list, dict)) and no_visible_change(reply["region"]):
        lines.append("no visible change")
    undoable = reply.get("undoable")
    if undoable in ("state", "partial"):
        stays = " (what it did outside the state stays)" if undoable == "partial" else ""
        lines.append(f"undo: undo_run({reply.get('run')}){stays}")
    return "\n".join(lines)


def ran_answer(reply, pictured):
    """A `ran` answer as the tool gives it: the picture first when one came, as render_screen gives it, then the
    text. A failed run is an error."""
    text = ran_text(reply, pictured)
    picture = reply.get("picture")
    failed = bool(reply.get("failed"))
    if not isinstance(picture, dict) or not picture.get("data"):
        return text, failed
    caption = f"{picture.get('width')}x{picture.get('height')} pixels, scale {reply.get('scale', 1)}"
    return [{"type": "image", "data": picture["data"], "mimeType": "image/png"},
            {"type": "text", "text": f"{text}\npicture: {caption}"}], failed


def refused(reply):
    """A refusal, passed on as the host wrote it (a denial says what would allow it when its reason does)."""
    return f"refused: {printable(reply.get('error') or reply.get('message'))}", True


def older_host(reply):
    """True if the host answered that it doesn't know the command messages."""
    return reply.get("type") == "error" and "unknown message type" in str(reply.get("message"))


def list_commands(host, args):
    """The list_commands tool: the app's commands, a line each."""
    message = {"type": "getCommands"}
    if isinstance(args.get("filter"), str) and args["filter"]:
        message["filter"] = args["filter"]
    if args.get("all") is True:
        message["all"] = True
    reply = host.request(message)
    if older_host(reply):
        return OLDER_HOST, True
    if reply.get("type") != "commands":
        return refused(reply)
    return command_list(reply, args.get("all") is True), False


def wait_seconds(args):
    """How long run_command waits for a run that waits for a yes: `wait`, 0 to MAX_WAIT, DEFAULT_WAIT if not given."""
    wait = args.get("wait", DEFAULT_WAIT)
    if not isinstance(wait, (int, float)) or isinstance(wait, bool):
        return DEFAULT_WAIT
    return min(max(wait, 0), MAX_WAIT)


def run_message(args):
    """The runCommand message for a run_command call, or an error text when its arguments can't be sent."""
    if not isinstance(args.get("name"), str) or not args["name"]:
        return "error: name must be a command's name, from list_commands"
    if "arguments" in args and not isinstance(args["arguments"], dict):
        return "error: arguments must be an object, an entry per argument"
    message = {"type": "runCommand", "command": args["name"],
               "evidence": "picture" if args.get("picture") is True else "state"}
    if args.get("arguments"):
        message["arguments"] = args["arguments"]
    if args.get("scale") in (1, 2):
        message["scale"] = args["scale"]
    return message


def run_command(host, args):
    """The run_command tool: runs a command, and for a run that waits for a yes, waits up to `wait` seconds for it."""
    message = run_message(args)
    if isinstance(message, str):
        return message, True
    reply = host.request(message)
    if reply.get("type") == "waiting":
        outcome = host.await_run(reply.get("run"), wait_seconds(args))
        if outcome is None:
            return (f"waiting for a yes (run {reply.get('run')}): {shortened(reply.get('reason'))}. Its outcome will "
                    "be in get_events."), False
        reply = outcome
    if reply.get("type") == "ran":
        return ran_answer(reply, message["evidence"] == "picture")
    if older_host(reply):
        return OLDER_HOST, True
    return refused(reply)


def undo_run(host, args):
    """The undo_run tool: puts back what a run changed, refused if something changed it since."""
    run = args.get("run")
    if not isinstance(run, int) or isinstance(run, bool):
        return "error: run must be a run's number, as run_command gave it", True
    reply = host.request({"type": "undoRun", "run": run})
    if reply.get("type") == "undone":
        return f"undone run {reply.get('run', run)}: {changes_text(reply)}", False
    if older_host(reply):
        return OLDER_HOST, True
    return refused(reply)
