"""Word wrapping and private color spans for native item notifications."""
import textwrap

LINE_WIDTH = 35
LINES_PER_POPUP = 3
SPAN = '\x1e'


def _characters(text):
    color = 4
    index = 0
    while index < len(text):
        char = text[index]
        if char == SPAN and index + 1 < len(text):
            following = text[index + 1]
            if following in '01234':
                color = int(following)
                index += 2
                continue
            if following == SPAN:
                index += 1
        yield char, color
        index += 1


def _colored_line(chars):
    result = []
    previous = 4
    for char, color in chars:
        if color != previous:
            result.append(SPAN + str(color))
            previous = color
        result.append(char.replace(SPAN, SPAN * 2))
    if previous != 4:
        result.append(SPAN + '4')
    return ''.join(result)


def display_pages(text):
    """Wrap visible characters, preserve colors across lines/pages, reset white.

    Color controls consume no visible columns. Page splitting is retained for
    unusually long messages; one popup now carries up to three 35-column lines.
    Native three-line visual fit requires an in-game test.
    """
    wrapper = textwrap.TextWrapper(width=LINE_WIDTH, break_long_words=True,
                                   break_on_hyphens=False)
    paragraphs = [[]]
    previous_cr = False
    for char, color in _characters(text):
        if char == '\n' and previous_cr:
            previous_cr = False
            continue
        previous_cr = char == '\r'
        if char in '\n\r\v\f\x85\u2028\u2029':
            paragraphs.append([])
        else:
            paragraphs[-1].append((' ' if char.isspace() else char, color))
    lines = []
    for chars in paragraphs:
        plain = ''.join(char for char, _ in chars)
        cursor = 0
        for line in wrapper.wrap(plain) or ['']:
            start = plain.find(line, cursor)
            if start < 0:
                raise ValueError('Notification visible-span alignment')
            lines.append(_colored_line(chars[start:start + len(line)]))
            cursor = start + len(line)
    while lines and not lines[0]:
        lines.pop(0)
    while lines and not lines[-1]:
        lines.pop()
    return tuple('\n'.join(lines[index:index + LINES_PER_POPUP])
                 for index in range(0, len(lines), LINES_PER_POPUP)
                 if any(lines[index:index + LINES_PER_POPUP]))
