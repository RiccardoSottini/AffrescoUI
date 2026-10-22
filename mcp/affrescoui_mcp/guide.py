"""The format guide (SCREEN_FORMAT.md) one topic at a time, so an agent reads only the part it needs."""

GUIDE_TOPIC = 5000  # characters: about 1,500 tokens, the most a topic of the format guide gives


def guide_sections(text):
    """SCREEN_FORMAT.md as its sections: (title, text) for the introduction ("introduction") and each "##" and "###"
    heading; a "##" section includes its "###" ones, or names them if they would make it longer than GUIDE_TOPIC."""
    lines = text.split("\n")
    sections, starts = [], [(0, 1, "introduction")]
    fence = False
    for i, line in enumerate(lines):
        fence = fence != line.startswith("```")
        if not fence and line.startswith("## "):
            starts.append((i, 2, line[3:].strip()))
        elif not fence and line.startswith("### "):
            starts.append((i, 3, line[4:].strip()))
    for n, (start, level, title) in enumerate(starts):
        end = next((s for s, other, _ in starts[n + 1:] if other <= level), len(lines))
        if level == 1:  # the introduction ends where the first section starts
            end = starts[1][0] if len(starts) > 1 else len(lines)
        part = "\n".join(lines[start:end]).strip()
        inner = [(s, t) for s, other, t in starts[n + 1:] if start < s < end and other == level + 1]
        if level == 2 and inner and len(part) > GUIDE_TOPIC:
            part = ("\n".join(lines[start:inner[0][0]]).strip() + "\n\nMore in the topics: " +
                    ", ".join(t.replace("`", "") for _, t in inner) + ".")
        sections.append((title.replace("`", ""), part))
    return sections


def format_guide(text, topic=None):
    """The part of the format guide an agent asks for: by default the introduction and the list of topics; a topic
    (a heading, any case, or a word in it) gives that section; "all" gives everything."""
    sections = guide_sections(text)
    if topic and str(topic).strip().lower() == "all":
        return text
    if topic:
        wanted = str(topic).strip().lower().replace("`", "")
        matches = [s for s in sections if s[0].lower() == wanted] or [s for s in sections if wanted in s[0].lower()]
        if not matches:
            return None
        also = [title for title, _ in matches[1:]]
        return matches[0][1] + (f"\n\n(Also matching: {', '.join(also)}.)" if also else "")
    titles = ", ".join(title for title, _ in sections[1:])
    return (sections[0][1] + "\n\nTopics (pass one as `topic`, or \"all\" for the whole guide): " + titles + ".")
