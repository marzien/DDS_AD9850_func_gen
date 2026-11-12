"""Minimal S-expression reader/writer for KiCad files."""
import re

_TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))', re.S)


class Sym(str):
    """Unquoted atom."""


def parse(text):
    stack = [[]]
    pos = 0
    n = len(text)
    while pos < n:
        m = _TOKEN.match(text, pos)
        if not m:
            if text[pos:].strip() == '':
                break
            raise ValueError('parse error at %d: %r' % (pos, text[pos:pos + 40]))
        pos = m.end()
        if m.group(1):
            stack.append([])
        elif m.group(2):
            lst = stack.pop()
            stack[-1].append(lst)
        elif m.group(3) is not None:
            stack[-1].append(m.group(3).replace('\\"', '"').replace('\\\\', '\\'))
        else:
            stack[-1].append(Sym(m.group(4)))
    return stack[0][0]


def _atom(a):
    if isinstance(a, Sym):
        return str(a)
    if isinstance(a, bool):
        return 'yes' if a else 'no'
    if isinstance(a, (int, float)):
        if isinstance(a, float):
            s = ('%.4f' % a).rstrip('0').rstrip('.')
            return s if s not in ('-0', '') else '0'
        return str(a)
    return '"' + str(a).replace('\\', '\\\\').replace('"', '\\"') + '"'


def dump(e, indent=0):
    if not isinstance(e, list):
        return _atom(e)
    simple = all(not isinstance(x, list) for x in e)
    if simple:
        return '(' + ' '.join(_atom(x) for x in e) + ')'
    pad = '\t' * (indent + 1)
    parts = []
    head = []
    for x in e:
        if isinstance(x, list):
            break
        head.append(_atom(x))
    out = '(' + ' '.join(head)
    for x in e[len(head):]:
        out += '\n' + pad + dump(x, indent + 1)
    return out + '\n' + '\t' * indent + ')'


def find(e, key):
    for x in e:
        if isinstance(x, list) and x and x[0] == key:
            return x
    return None


def find_all(e, key):
    return [x for x in e if isinstance(x, list) and x and x[0] == key]
