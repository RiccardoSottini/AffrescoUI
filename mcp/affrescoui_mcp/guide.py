"""The format guide (SCREEN_FORMAT.md) one topic at a time, so an agent reads only the part it needs."""

GUIDE_TOPIC = 6000  # characters: about 1,800 tokens, the most a topic of the format guide gives


def headings(lines):
    """Where the guide's topics start, as (line, level, title): the introduction (level 1) at line 0, then each "##"
    and "###" heading outside a code block."""
    starts = [(0, 1, "introduction")]
    fence = False
    for i, line in enumerate(lines):
        fence = fence != line.startswith("```")
        if not fence and line.startswith("## "):
            starts.append((i, 2, line[3:].strip()))
        elif not fence and line.startswith("### "):
            starts.append((i, 3, line[4:].strip()))
    return starts


def section(lines, starts, n):
    """The text of the topic starts[n]: down to the next heading of its level or above, its "###" ones only named
    when they would make a "##" topic longer than GUIDE_TOPIC."""
    start, level, _ = starts[n]
    end = next((s for s, other, _ in starts[n + 1:] if other <= level), len(lines))
    if level == 1:  # the introduction ends where the first section starts
        end = starts[1][0] if len(starts) > 1 else len(lines)
    part = "\n".join(lines[start:end]).strip()
    inner = [(s, t) for s, other, t in starts[n + 1:] if start < s < end and other == level + 1]
    if level == 2 and inner and len(part) > GUIDE_TOPIC:
        part = ("\n".join(lines[start:inner[0][0]]).strip() + "\n\nMore in the topics: " +
                ", ".join(t.replace("`", "") for _, t in inner) + ".")
    return part


def pages(title, part):
    """A topic as (title, text) pages of at most GUIDE_TOPIC characters, split between lines: the first keeps the
    title, the next ones are "title (2)" and on, each but the last naming the next; a page that starts inside a table
    repeats its header, and one inside a code block opens it again."""
    if len(part) <= GUIDE_TOPIC:
        return [(title, part)]
    result, page, header, fence = [], [], [], False
    room = GUIDE_TOPIC - 80  # for the line naming the next page, and a code block closed
    for line in part.split("\n"):
        if page and len("\n".join(page + [line])) > room:
            result.append(page + ["```"] if fence else page)
            page = ["```"] if fence else list(header) if line.startswith("|") else []
        page.append(line)
        fence = fence != line.startswith("```")
        if line.startswith("|---"):
            header = page[-2:]
        elif not line.startswith("|"):
            header = []
    result.append(page)
    names = [title] + [f"{title} ({n})" for n in range(2, len(result) + 1)]
    return [(names[n], "\n".join(lines) + (f"\n\nContinued in the topic \"{names[n + 1]}\"."
                                            if n + 1 < len(result) else "")) for n, lines in enumerate(result)]


def guide_sections(text):
    """SCREEN_FORMAT.md as its sections: (title, text) for the introduction ("introduction") and each "##" and "###"
    heading; a "##" section includes its "###" ones, or names them if they would make it longer than GUIDE_TOPIC, and
    a section still longer comes in pages (pages())."""
    lines = text.split("\n")
    starts = headings(lines)
    return [page for n, (_, _, title) in enumerate(starts)
            for page in pages(title.replace("`", ""), section(lines, starts, n))]


def format_guide(text, topic=None):
    """The part of the format guide an agent asks for: by default the introduction and the list of topics; a topic
    (a heading, any case, or a word in it) gives that section; "all" gives everything."""
    sections = guide_sections(text)
    if topic and str(topic).strip().lower() == "all":
        return text
    if topic:
        return guide_topic(sections, topic)
    titles = ", ".join(title for title, _ in sections[1:])
    return (sections[0][1] + "\n\nTopics (pass one as `topic`, or \"all\" for the whole guide): " + titles + ".")


def guide_topic(sections, topic):
    """The section a topic names (its heading, any case, else a word in it), saying which others match too; None if
    no heading does."""
    wanted = str(topic).strip().lower().replace("`", "")
    matches = [s for s in sections if s[0].lower() == wanted] or [s for s in sections if wanted in s[0].lower()]
    if not matches:
        return None
    also = [title for title, _ in matches[1:]]
    return matches[0][1] + (f"\n\n(Also matching: {', '.join(also)}.)" if also else "")
