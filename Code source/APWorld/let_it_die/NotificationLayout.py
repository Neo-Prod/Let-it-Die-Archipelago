"""Presentation-only wrapping for the native HUD's one/two-line popup."""
import textwrap

LINE_WIDTH = 35
LINES_PER_POPUP = 2


def display_pages(text):
    """Keep words/punctuation in order, normalize presentation whitespace only.

    Long indivisible names are split at Unicode codepoint boundaries to avoid
    overflowing the screen. Existing line breaks remain paragraph boundaries.
    Each page fits the existing native two-line layout; no game UI is changed.
    """
    wrapper = textwrap.TextWrapper(width=LINE_WIDTH, break_long_words=True,
                                   break_on_hyphens=False)
    lines = []
    for paragraph in text.splitlines():
        lines.extend(wrapper.wrap(paragraph) or [''])
    while lines and not lines[0]:
        lines.pop(0)
    while lines and not lines[-1]:
        lines.pop()
    return tuple('\n'.join(lines[index:index + LINES_PER_POPUP])
                 for index in range(0, len(lines), LINES_PER_POPUP)
                 if any(lines[index:index + LINES_PER_POPUP]))
