{:note "Calamaio's sample files: a small home folder kept in the state, as the stock host grants no file system. Dired lists it, C-x C-f visits it and C-x C-s writes back to it."
  :state {:home "~/"
    :files [{:path "~/letters/" :dir true :mtime 1778748120000}
      {:path "~/hello.py"
        :text "\"\"\"Counts the words of a text file and prints the most common ones.\"\"\"\n\nimport sys\nfrom collections import Counter\n\n\ndef words(text):\n    \"\"\"Splits the text into lowercase words, dropping punctuation.\"\"\"\n    cleaned = \"\".join(c if c.isalnum() else \" \" for c in text.lower())\n    return cleaned.split()\n\n\ndef main(path, top=10):\n    with open(path, encoding=\"utf-8\") as handle:\n        counts = Counter(words(handle.read()))\n    for word, n in counts.most_common(top):\n        print(f\"{n:6}  {word}\")\n    return 0\n\n\nif __name__ == \"__main__\":\n    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else \"notes.txt\"))\n"
        :mtime 1778509920000}
      {:path "~/init.el"
        :text ";;; init.el --- personal settings  -*- lexical-binding: t -*-\n\n;; Calamaio keeps its keys, menus and settings in the screen document;\n;; this file is only a sample to edit.\n\n(setq inhibit-startup-screen nil)\n(setq fill-column 72)\n(global-set-key (kbd \"C-c l\") #'display-line-numbers-mode)\n\n(defun my-insert-date ()\n  \"Insert today's date at point.\"\n  (interactive)\n  (insert (format-time-string \"%Y-%m-%d\")))\n\n(provide 'init)\n;;; init.el ends here\n"
        :mtime 1778713920000}
      {:path "~/notes.txt"
        :text "Notes from the garden club, spring meeting\n\nThe tomatoes went in late this year, after the frost on the third week of\nApril. Marta suggested we try the yellow pear variety along the south fence,\nwhere the wall keeps the heat through the evening.\n\nThings to bring next time:\n  - the seed trays from the shed\n  - string, about twenty metres\n  - the ledger with last year's yields\n\nNobody remembered who borrowed the long ladder. If you have it, please bring\nit back before the plum harvest; the top branches are out of reach again.\n\nThe next meeting is on the first Saturday of June, at the library, starting\nat ten. Coffee will be provided, but bring your own cup.\n"
        :mtime 1778744520000}
      {:path "~/plan.md"
        :text "# Moving the workshop\n\n## Before the move\n- Measure the new room, and the door\n- Sort the timber: keep, sell, burn\n- Book the van for a Saturday\n\n## Moving day\n1. Benches and the big saw first\n2. Then the boxes, labelled by wall\n3. The plants last, in the car\n\n## Afterwards\nHang the tools in the same order as before, so the hands still know where\neverything is. *Check the wiring* before plugging in the lathe.\n"
        :mtime 1778029920000}
      {:path "~/ring.c"
        :text "/* A ring buffer of integers: push at the head, pop at the tail. */\n#include <stdio.h>\n#include <stdlib.h>\n\n#define CAPACITY 8\n\nstruct ring {\n\tint items[CAPACITY];\n\tsize_t head, tail, count;\n};\n\nstatic int push(struct ring *r, int value)\n{\n\tif (r->count == CAPACITY)\n\t\treturn -1;\n\tr->items[r->head] = value;\n\tr->head = (r->head + 1) % CAPACITY;\n\tr->count++;\n\treturn 0;\n}\n\nstatic int pop(struct ring *r, int *value)\n{\n\tif (r->count == 0)\n\t\treturn -1;\n\t*value = r->items[r->tail];\n\tr->tail = (r->tail + 1) % CAPACITY;\n\tr->count--;\n\treturn 0;\n}\n\nint main(void)\n{\n\tstruct ring r = {0};\n\tint value;\n\n\tfor (int i = 1; i <= 10; i++)\n\t\tif (push(&r, i * i) != 0)\n\t\t\tprintf(\"full at %d\\n\", i);\n\twhile (pop(&r, &value) == 0)\n\t\tprintf(\"%d\\n\", value);\n\treturn EXIT_SUCCESS;\n}\n"
        :mtime 1778317920000}
      {:path "~/todo.org"
        :text "* Errands\n** TODO Return the long ladder\n** DONE Buy string for the tomatoes\n* Reading\n** TODO Finish the book on dry stone walls\n   Chapter six is about drainage; take notes.\n** TODO Lend the atlas to the neighbours\n* Workshop\n** TODO Sharpen the chisels\n** TODO Oil the bench vice\n"
        :mtime 1778731920000}
      {:path "~/letters/draft.txt"
        :text "Dear Ilaria,\n\nThank you for the seeds; they arrived in good order and the parcel smelled\nof the cellar, which made everyone in the house homesick.\n\nThe beans are already up. I will write again when the first pods come.\n\nWith love,\nG.\n"
        :mtime 1778747220000}]}}
