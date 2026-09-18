import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE_FILES = (
    "src/autohidedockwidget.cpp",
    "src/canvas.cpp",
    "src/drawingtoolsettings.cpp",
    "src/layermodel.cpp",
    "src/layerpanel.cpp",
    "src/main.cpp",
    "src/mainwindow.cpp",
    "src/project.cpp",
    "src/rolloutsection.cpp",
    "src/toolpropertiespanel.cpp",
)
CYRILLIC = re.compile(r"[\u0400-\u04ff]")
USER_TEXT_CALLS = {
    "QAction",
    "QLabel",
    "addAction",
    "addMenu",
    "addRow",
    "addTab",
    "critical",
    "drawText",
    "getColor",
    "getOpenFileName",
    "getSaveFileName",
    "information",
    "question",
    "setPlaceholderText",
    "setPrefix",
    "setSuffix",
    "setText",
    "setToolTip",
    "setWindowTitle",
    "showMessage",
    "warning",
}


def tokens(text):
    """Yield the C++ tokens needed to track calls and user-facing string literals."""
    index = 0
    line = 1
    while index < len(text):
        if text.startswith("//", index):
            end = text.find("\n", index)
            index = len(text) if end < 0 else end
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = len(text) - 2 if end < 0 else end
            line += text.count("\n", index, end + 2)
            index = end + 2
            continue
        character = text[index]
        if character.isspace():
            if character == "\n":
                line += 1
            index += 1
            continue
        if character.isalpha() or character == "_":
            end = index + 1
            while end < len(text) and (text[end].isalnum() or text[end] == "_"):
                end += 1
            yield "identifier", text[index:end], line
            index = end
            continue
        prefix_length = 0
        for prefix in ("u8", "u", "U", "L"):
            if text.startswith(prefix + '"', index):
                prefix_length = len(prefix)
                break
        if character == '"' or prefix_length:
            start_line = line
            start = index + prefix_length + 1
            end = start
            while end < len(text):
                if text[end] == "\n":
                    line += 1
                if text[end] == '"' and (end == start or text[end - 1] != "\\"):
                    break
                end += 1
            yield "string", text[start:end], start_line
            index = min(end + 1, len(text))
            continue
        yield "symbol", character, line
        index += 1


def audit(path):
    """Return string literals that bypass the supported Qt translation calls."""
    stack = []
    previous = None
    violations = []
    for kind, value, line in tokens(path.read_text(encoding="utf-8-sig")):
        if kind == "symbol" and value == "(":
            callee = previous[1] if previous and previous[0] == "identifier" else ""
            stack.append(callee)
        elif kind == "symbol" and value == ")":
            if stack:
                stack.pop()
        elif kind == "string":
            translated = any(callee in {"tr", "translate", "QT_TRANSLATE_NOOP"} for callee in stack)
            visible_call = bool(stack) and stack[-1] in USER_TEXT_CALLS
            if not translated and (CYRILLIC.search(value) or visible_call):
                violations.append((line, value.replace("\n", "\\n")[:100]))
        previous = (kind, value)
    return violations


def main():
    """Audit every production source file and return a process-friendly status code."""
    failures = []
    for relative in SOURCE_FILES:
        path = ROOT / relative
        failures.extend((relative, line, value) for line, value in audit(path))
    if failures:
        for relative, line, value in failures:
            print(f"{relative}:{line}: user-facing Cyrillic string is not marked for translation: {value}")
        return 1
    print(f"Translation audit passed for {len(SOURCE_FILES)} production source files.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
