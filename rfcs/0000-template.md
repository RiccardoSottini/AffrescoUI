# RFC NNNN: Title

<!-- Copy this file to rfcs/0000-short-name.md; the maintainer gives it its number before merging. Fill in every
     section. Where one doesn't apply, write "None" and say why. Keep the guidance comments out of the final text. -->

- **Issue:** #
- **Author:** @
- **Changes:** the screen format / the live protocol or the MCP tools / the security model / the modules

## The problem

<!-- What can't be expressed or done today, and who needs it. Start from a real screen, request or app, not from the
     solution. Say what is out of scope. -->

## The design

<!-- The change, precise enough to implement and to review. For a format change: every new key, its type, its
     default and its meaning, how it is validated, and the error each mistake gives (errors name the place). For a
     protocol change: every message and its fields.

     Show examples: a screen before and after, a revision an agent would send, the line a reviewer would see.
     Check the design against the design rules in SCREEN_FORMAT.md: words, not codes; familiar notation; one key,
     one meaning; named arguments beyond two; locality first; reviewable changes; notes; errors that name the
     place. -->

```json
{
}
```

## The alternatives

<!-- Other designs you considered, what other software does, and why this one is better. Include doing nothing:
     what can be done today without a change, and what that costs. -->

## The cost in tokens

<!-- What typical agent edits cost with this change, compared with today: a first read, a narrow read, and the
     revisions an agent would send to use the feature. Give the token counts and how they were measured (the
     tokenizer and the tool). Changes should get smaller through structure (ids, components, design tokens), never
     through abbreviations. -->

| Agent edit | Today | With this RFC |
|---|---|---|
| | | |

## Performance

<!-- What it costs at run time: on a load, on an update, on a frame, and in memory. Cost should follow what changed
     or what is visible, not the size of the app. Give measured numbers for anything on a hot path, before and after,
     on the same machine, or say what will be measured and how. -->

## Security review

<!-- Treat documents, revisions and peers as untrusted. Answer each question:
     - What new input does it accept, and what are its limits (size, count, recursion depth)? What fuzzes it?
     - Does it add a capability (a channel, a file, a process, a network connection, a secret)? Capabilities are
       granted by the host and never widened by a revision: where is that checked?
     - Can a revision use it to do more than its author's permissions allow, or to approve its own change?
     - Can a secret reach a document, state, history, a log or a screenshot?
     - What callbacks or timers does it add, and what cancels them when their owner goes away? -->

## Compatibility

<!-- Is it additive within format version 1? What happens to existing screens, saved state, history (which stays
     RFC 6902) and change sets? Do the schema, SCREEN_FORMAT.md and the catalog change together? Is the public C++
     API source-compatible? If anything breaks, how do people migrate? -->

## Open questions

<!-- What is still undecided, and what the review should settle before the merge. Things deliberately left for a
     later RFC go here too. -->
