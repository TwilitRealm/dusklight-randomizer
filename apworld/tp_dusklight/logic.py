"""Port of the official randomizer's requirement language (generator/logic/requirement.cpp).

Requirements are parsed exactly like the C++ parser does, then compiled into AP rule
closures. Setting comparisons fold to constants at compile time, and form/time predicates
(Human_Link, Wolf_Link, Day, Night, Twilight) fold to constants for a fixed form-time
(exits) or become region-reachability checks for an area's union of form-times
(locations and events), mirroring how the C++ search evaluates each.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING, Callable, Union

from . import data

if TYPE_CHECKING:
    from BaseClasses import CollectionState

# FormTime bits (requirement.hpp)
HUMAN_DAY = 0b0001
HUMAN_NIGHT = 0b0010
WOLF_DAY = 0b0100
WOLF_NIGHT = 0b1000
HUMAN = HUMAN_DAY | HUMAN_NIGHT
WOLF = WOLF_DAY | WOLF_NIGHT
DAY = HUMAN_DAY | WOLF_DAY
NIGHT = HUMAN_NIGHT | WOLF_NIGHT
ALL = 0b1111
TWILIGHT = 0b10000
FORM_TIMES = (HUMAN_DAY, HUMAN_NIGHT, WOLF_DAY, WOLF_NIGHT)
FORM_NAMES = {HUMAN_DAY: "Human Day", HUMAN_NIGHT: "Human Night", WOLF_DAY: "Wolf Day",
              WOLF_NIGHT: "Wolf Night", TWILIGHT: "Twilight"}

DUNGEON_COMPLETION_EVENTS = tuple(f"Can Complete {d}" for d in data.DUNGEONS if d != "Hyrule Castle")

# --- AST ---------------------------------------------------------------------------------

Node = tuple  # ("and", [..]) | ("or", [..]) | ("item", name) | ("count", n, name) | ...


class LogicError(RuntimeError):
    pass


class Parser:
    """Faithful port of ParseRequirementString. Needs the world's settings for comparisons."""

    def __init__(self, setting_lookup: Callable[[str], str]):
        self.setting = setting_lookup
        self.macros = data.macros()
        # C++ LoadLogicMacros registers each macro name only after parsing its body, so a
        # macro body can only see macros defined above it ("Slingshot: Slingshot and ..."
        # refers to the item). The world graph, parsed later, sees every macro.
        self.macro_order = {name: i for i, name in enumerate(self.macros)}
        self._limit = len(self.macros)
        self.items = data.items()
        self.settings_info = data.settings()
        self.referenced_events: set[str] = set()
        self._cache: dict[str, Node] = {}

    def parse(self, req: str, limit: int | None = None) -> Node:
        limit = len(self.macros) if limit is None else limit
        key = (req, limit)
        node = self._cache.get(key)
        if node is None:
            saved, self._limit = self._limit, limit
            try:
                node = self._parse(req)
            finally:
                self._limit = saved
            self._cache[key] = node
        return node

    def parse_macro(self, name: str) -> Node:
        return self.parse(self.macros[name], self.macro_order[name])

    def _compare(self, setting_name: str, op: str, option: str) -> bool:
        info = self.settings_info.get(setting_name)
        if info is None:
            raise LogicError(f"unknown setting '{setting_name}'")
        cur = info.index_of(self.setting(setting_name))
        want = info.index_of(option)
        if op == "==":
            return cur == want
        if op == "!=":
            return cur != want
        if op == ">=":
            return cur >= want
        return cur <= want

    def _setting_number(self, s: str) -> int:
        if s in self.settings_info:
            s = self.setting(s)
        return int(s)

    def _parse(self, req_str: str) -> Node:
        logic = list(req_str)
        nesting = 1
        delim = "+"
        for i, ch in enumerate(logic):
            if ch == "(":
                nesting += 1
            elif ch == ")":
                nesting -= 1
            if nesting == 1 and ch == " ":
                logic[i] = delim
        if nesting != 1:
            raise LogicError(f"Extra or missing parenthesis within expression: \"{req_str}\"")
        s = "".join(logic)

        parts: list[str] = []
        while True:
            pos = s.find(delim)
            if pos == -1:
                break
            before = s[pos - 1]
            after = s[pos + 1]
            if before not in "!=><" and after not in "!=><":
                parts.append(s[:pos])
                s = s[pos + 1:]
            else:
                s = s[:pos] + s[pos + 1:]
        parts.append(s)

        if len(parts) == 1:
            arg = parts[0].replace("_", " ")
            if arg == "Nothing":
                return ("true",)
            if arg == "Human Link":
                return ("human",)
            if arg == "Wolf Link":
                return ("wolf",)
            if arg == "Twilight":
                return ("twilight",)
            if arg.startswith("'"):
                name = arg[1:-1]
                self.referenced_events.add(name)
                return ("event", name)
            if self.macro_order.get(arg, self._limit) < self._limit:
                return ("macro", arg)
            if arg in self.items:
                return ("item", arg)
            for op in ("!=", "==", ">=", "<="):
                if op in arg:
                    comp = arg.rfind("=")
                    option = arg[comp + 1:]
                    setting = arg[:comp - 1]
                    return ("true",) if self._compare(setting, op, option) else ("false",)
            if "count" in arg:
                inner = arg[arg.find("(") + 1:-1]
                item_name, count_str = inner.split(", ")
                if item_name not in self.items:
                    raise LogicError(f"unknown item '{item_name}' in {req_str}")
                return ("count", self._setting_number(count_str), item_name)
            if arg == "Day":
                return ("day",)
            if arg == "Night":
                return ("night",)
            if "hearts" in arg:
                return ("hearts", self._setting_number(arg[arg.find("(") + 1:-1]))
            if arg == "Impossible":
                return ("false",)
            if "golden bugs" in arg:
                return ("bugs", int(arg[arg.find("(") + 1:-1]))
            if "dungeons completed" in arg:
                return ("dungeons", self._setting_number(arg[arg.find("(") + 1:-1]))
            raise LogicError(f"Unrecognized logic symbol: \"{req_str}\"")

        if len(parts) == 2:
            raise LogicError(f"Unrecognized 2 part expression: {req_str}")

        is_and = "and" in parts
        is_or = "or" in parts
        if is_and and is_or:
            raise LogicError(f"\"and\" & \"or\" in same nesting level when parsing \"{req_str}\"")
        if not (is_and or is_or):
            raise LogicError(f"Could not determine logical operator type from expression: \"{req_str}\"")
        args = []
        for p in parts:
            if p in ("and", "or"):
                continue
            if p.startswith("("):
                p = p[1:-1]
            args.append(self.parse(p, self._limit))
        return ("and" if is_and else "or", tuple(args))


# --- Compilation -------------------------------------------------------------------------

Rule = Union[bool, Callable[["CollectionState"], bool]]


@dataclass(frozen=True)
class AreaForms:
    """Union-of-form-times evaluation context for an area: predicates check copy reachability."""
    human: Rule
    wolf: Rule
    day: Rule
    night: Rule
    twilight: Rule


class Compiler:
    def __init__(self, parser: Parser, player: int, event_item: Callable[[str], str],
                 golden_bugs: tuple[str, ...]):
        self.parser = parser
        self.player = player
        self.event_item = event_item
        self.golden_bugs = golden_bugs
        self._macro_cache: dict[tuple[str, object], Rule] = {}
        self.referenced_items: set[str] = set()

    # Combinators -----------------------------------------------------------------------
    @staticmethod
    def all_of(rules: list[Rule]) -> Rule:
        fns = []
        for r in rules:
            if r is False:
                return False
            if r is not True:
                fns.append(r)
        if not fns:
            return True
        if len(fns) == 1:
            return fns[0]
        if len(fns) == 2:
            a, b = fns
            return lambda state: a(state) and b(state)
        fns_t = tuple(fns)
        return lambda state: all(f(state) for f in fns_t)

    @staticmethod
    def any_of(rules: list[Rule]) -> Rule:
        fns = []
        for r in rules:
            if r is True:
                return True
            if r is not False:
                fns.append(r)
        if not fns:
            return False
        if len(fns) == 1:
            return fns[0]
        if len(fns) == 2:
            a, b = fns
            return lambda state: a(state) or b(state)
        fns_t = tuple(fns)
        return lambda state: any(f(state) for f in fns_t)

    # Entry points ----------------------------------------------------------------------
    def compile_str(self, req: str, forms: Union[int, AreaForms]) -> Rule:
        return self.compile(self.parser.parse(req), forms)

    def compile(self, node: Node, forms: Union[int, AreaForms]) -> Rule:
        kind = node[0]
        p = self.player
        if kind == "true":
            return True
        if kind == "false":
            return False
        if kind == "and":
            return self.all_of([self.compile(n, forms) for n in node[1]])
        if kind == "or":
            return self.any_of([self.compile(n, forms) for n in node[1]])
        if kind == "item":
            name = node[1]
            self.referenced_items.add(name)
            return lambda state: state.has(name, p)
        if kind == "count":
            n, name = node[1], node[2]
            self.referenced_items.add(name)
            if n <= 0:
                return True
            return lambda state: state.has(name, p, n)
        if kind == "event":
            ev = self.event_item(node[1])
            return lambda state: state.has(ev, p)
        if kind == "macro":
            key = (node[1], forms if isinstance(forms, int) else id(forms))
            rule = self._macro_cache.get(key)
            if rule is None:
                # Macros are parsed with forceLogic and may reference settings: parse lazily.
                rule = self.compile(self.parser.parse_macro(node[1]), forms)
                self._macro_cache[key] = rule
            return rule
        if kind in ("human", "wolf", "day", "night", "twilight"):
            if isinstance(forms, int):
                mask = {"human": HUMAN, "wolf": WOLF, "day": DAY, "night": NIGHT, "twilight": TWILIGHT}[kind]
                return bool(forms & mask)
            return getattr(forms, kind)
        if kind == "hearts":
            n = node[1]
            self.referenced_items.update(("Piece of Heart", "Heart Container"))
            return lambda state: (state.count("Piece of Heart", p)
                                  + (state.count("Heart Container", p) + 3) * 5) >= n * 5
        if kind == "bugs":
            n = node[1]
            bugs = self.golden_bugs
            self.referenced_items.update(bugs)
            return lambda state: state.has_from_list_unique(bugs, p, n)
        if kind == "dungeons":
            n = node[1]
            evs = tuple(self.event_item(e) for e in DUNGEON_COMPLETION_EVENTS)
            for e in DUNGEON_COMPLETION_EVENTS:
                self.parser.referenced_events.add(e)
            return lambda state: sum(1 for e in evs if state.has(e, p)) >= n
        raise LogicError(f"unknown node {node}")


def as_callable(rule: Rule) -> Callable[["CollectionState"], bool]:
    if rule is True:
        return lambda state: True
    if rule is False:
        return lambda state: False
    return rule
