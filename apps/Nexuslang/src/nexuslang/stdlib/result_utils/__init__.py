"""result_utils — Result/Option types and structured error chaining for NexusLang.

Provides two algebraic data types:

  Result  — represents either success (Ok) or failure (Err).
  Option  — represents either a present value (Some) or absence (None).

And a structured error-chain API for wrapping and inspecting errors.

All values are plain Python dicts so they are transparently serialisable
and require no special runtime support.

Result representation:
  {'tag': 'ok',  'value': <any>}
  {'tag': 'err', 'error': <error_dict_or_str>}

Option representation:
  {'tag': 'some', 'value': <any>}
  {'tag': 'none'}

Error dict representation:
  {
    'message': str,
    'code':    str | None,
    'context': dict | None,
    'cause':   error_dict | None,
  }
"""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from ...runtime.runtime import Runtime


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------


def _ok(value) -> dict:
    return {"tag": "ok", "value": value}


def _err(error) -> dict:
    return {"tag": "err", "error": error}


def _some(value) -> dict:
    return {"tag": "some", "value": value}


def _none() -> dict:
    return {"tag": "none"}


def _require_result(r, fname: str) -> None:
    if not isinstance(r, dict) or r.get("tag") not in ("ok", "err"):
        raise RuntimeError(
            f"{fname}: expected a Result dict (tag='ok'/'err'), got {type(r).__name__}"
        )


def _require_option(o, fname: str) -> None:
    if not isinstance(o, dict) or o.get("tag") not in ("some", "none"):
        raise RuntimeError(
            f"{fname}: expected an Option dict (tag='some'/'none'), got {type(o).__name__}"
        )


def _call(fn, *args):
    """Invoke fn which may be a Python callable or an NexusLang runtime value."""
    if callable(fn):
        return fn(*args)
    raise RuntimeError(f"result_utils: expected a callable, got {type(fn).__name__}")


# ===========================================================================
# Result constructors
# ===========================================================================


def result_ok(value):
    """Create an Ok Result wrapping *value*."""
    return _ok(value)


def result_err(error):
    """Create an Err Result wrapping *error* (string or error dict)."""
    return _err(error)


# ===========================================================================
# Result predicates
# ===========================================================================


def result_is_ok(r) -> bool:
    """Return True if *r* is an Ok Result."""
    _require_result(r, "result_is_ok")
    return r["tag"] == "ok"


def result_is_err(r) -> bool:
    """Return True if *r* is an Err Result."""
    _require_result(r, "result_is_err")
    return r["tag"] == "err"


# ===========================================================================
# Result extraction
# ===========================================================================


def result_unwrap(r):
    """Return the Ok value, or raise RuntimeError if Err."""
    _require_result(r, "result_unwrap")
    if r["tag"] == "err":
        msg = r["error"]
        if isinstance(msg, dict):
            msg = msg.get("message", repr(msg))
        raise RuntimeError(f"result_unwrap: called on Err — {msg}")
    return r["value"]


def result_unwrap_err(r):
    """Return the Err value, or raise RuntimeError if Ok."""
    _require_result(r, "result_unwrap_err")
    if r["tag"] == "ok":
        raise RuntimeError(
            f"result_unwrap_err: called on Ok({r['value']!r})"
        )
    return r["error"]


def result_unwrap_or(r, default):
    """Return the Ok value, or *default* if Err."""
    _require_result(r, "result_unwrap_or")
    return r["value"] if r["tag"] == "ok" else default


def result_unwrap_or_else(r, fn):
    """Return the Ok value, or the result of calling *fn*() if Err."""
    _require_result(r, "result_unwrap_or_else")
    if r["tag"] == "ok":
        return r["value"]
    return _call(fn)


def result_expect(r, message: str):
    """Return the Ok value, or raise RuntimeError with *message* if Err."""
    _require_result(r, "result_expect")
    if r["tag"] == "err":
        raise RuntimeError(message)
    return r["value"]


# ===========================================================================
# Result transformations
# ===========================================================================


def result_map(r, fn):
    """Apply *fn* to the Ok value; pass Err through unchanged."""
    _require_result(r, "result_map")
    if r["tag"] == "err":
        return r
    return _ok(_call(fn, r["value"]))


def result_map_err(r, fn):
    """Apply *fn* to the Err value; pass Ok through unchanged."""
    _require_result(r, "result_map_err")
    if r["tag"] == "ok":
        return r
    return _err(_call(fn, r["error"]))


def result_and_then(r, fn):
    """Flatmap: call *fn*(value) → Result if Ok; pass Err through."""
    _require_result(r, "result_and_then")
    if r["tag"] == "err":
        return r
    new_r = _call(fn, r["value"])
    _require_result(new_r, "result_and_then (fn return value)")
    return new_r


def result_or_else(r, fn):
    """If Ok, return r unchanged. If Err, call *fn*(error) → Result."""
    _require_result(r, "result_or_else")
    if r["tag"] == "ok":
        return r
    new_r = _call(fn, r["error"])
    _require_result(new_r, "result_or_else (fn return value)")
    return new_r


# ===========================================================================
# Result ↔ Option conversions
# ===========================================================================


def result_to_option(r):
    """Convert Result to Option: Ok(v) → Some(v), Err → None."""
    _require_result(r, "result_to_option")
    if r["tag"] == "ok":
        return _some(r["value"])
    return _none()


def result_ok_or(opt, error):
    """Convert Option to Result: Some(v) → Ok(v), None → Err(error)."""
    _require_option(opt, "result_ok_or")
    if opt["tag"] == "some":
        return _ok(opt["value"])
    return _err(error)


# ===========================================================================
# Option constructors
# ===========================================================================


def option_some(value):
    """Create a Some Option wrapping *value*."""
    return _some(value)


def option_none():
    """Create a None Option (no value present)."""
    return _none()


# ===========================================================================
# Option predicates
# ===========================================================================


def option_is_some(o) -> bool:
    """Return True if *o* is a Some Option."""
    _require_option(o, "option_is_some")
    return o["tag"] == "some"


def option_is_none(o) -> bool:
    """Return True if *o* is a None Option."""
    _require_option(o, "option_is_none")
    return o["tag"] == "none"


# ===========================================================================
# Option extraction
# ===========================================================================


def option_unwrap(o):
    """Return the Some value, or raise RuntimeError if None."""
    _require_option(o, "option_unwrap")
    if o["tag"] == "none":
        raise RuntimeError("option_unwrap: called on None Option")
    return o["value"]


def option_unwrap_or(o, default):
    """Return the Some value, or *default* if None."""
    _require_option(o, "option_unwrap_or")
    return o["value"] if o["tag"] == "some" else default


def option_unwrap_or_else(o, fn):
    """Return the Some value, or the result of calling *fn*() if None."""
    _require_option(o, "option_unwrap_or_else")
    if o["tag"] == "some":
        return o["value"]
    return _call(fn)


def option_expect(o, message: str):
    """Return the Some value, or raise RuntimeError with *message* if None."""
    _require_option(o, "option_expect")
    if o["tag"] == "none":
        raise RuntimeError(message)
    return o["value"]


# ===========================================================================
# Option transformations
# ===========================================================================


def option_map(o, fn):
    """Apply *fn* to the Some value; pass None through unchanged."""
    _require_option(o, "option_map")
    if o["tag"] == "none":
        return o
    return _some(_call(fn, o["value"]))


def option_and_then(o, fn):
    """Flatmap: call *fn*(value) → Option if Some; pass None through."""
    _require_option(o, "option_and_then")
    if o["tag"] == "none":
        return o
    new_o = _call(fn, o["value"])
    _require_option(new_o, "option_and_then (fn return value)")
    return new_o


def option_or(o, other):
    """Return *o* if Some; otherwise return *other* Option."""
    _require_option(o, "option_or")
    _require_option(other, "option_or (other)")
    return o if o["tag"] == "some" else other


def option_or_else(o, fn):
    """Return *o* if Some; otherwise call *fn*() → Option."""
    _require_option(o, "option_or_else")
    if o["tag"] == "some":
        return o
    new_o = _call(fn)
    _require_option(new_o, "option_or_else (fn return value)")
    return new_o


def option_filter(o, pred):
    """Return Some(v) if Some and pred(v) is truthy; otherwise None."""
    _require_option(o, "option_filter")
    if o["tag"] == "none":
        return o
    return o if _call(pred, o["value"]) else _none()


def option_flatten(o):
    """Unwrap one level of Option<Option<T>>.

    Some(Some(v)) → Some(v), Some(None) → None, None → None.
    """
    _require_option(o, "option_flatten")
    if o["tag"] == "none":
        return o
    inner = o["value"]
    _require_option(inner, "option_flatten (inner value)")
    return inner


# ===========================================================================
# Option ↔ Result conversion
# ===========================================================================


def option_to_result(o, error):
    """Convert Option to Result: Some(v) → Ok(v), None → Err(error)."""
    _require_option(o, "option_to_result")
    if o["tag"] == "some":
        return _ok(o["value"])
    return _err(error)


# ===========================================================================
# Error chain API
# ===========================================================================


def error_new(message: str, code=None, context=None) -> dict:
    """Create a structured error dict.

    Parameters
    ----------
    message : str
        Human-readable description of the error.
    code : str or None
        Optional machine-readable error code (e.g. "E_NOT_FOUND").
    context : dict or None
        Optional key/value metadata (e.g. file path, line number).
    """
    return {
        "message": str(message),
        "code": code,
        "context": context if context is not None else {},
        "cause": None,
    }


def error_wrap(error, message: str) -> dict:
    """Wrap *error* with a higher-level *message*, building an error chain."""
    if not isinstance(error, dict) or "message" not in error:
        # Normalise plain strings into an error dict before wrapping
        error = error_new(str(error))
    return {
        "message": str(message),
        "code": None,
        "context": {},
        "cause": error,
    }


def error_chain(errors: list) -> dict:
    """Build a linked error chain from a list [outermost, ..., root].

    The first element becomes the outermost error; each subsequent element
    is stored as the `cause` of the previous one.
    """
    if not errors:
        raise RuntimeError("error_chain: list must not be empty")
    chain = None
    for raw in reversed(errors):
        if isinstance(raw, str):
            raw = error_new(raw)
        node = dict(raw)
        node["cause"] = chain
        chain = node
    return chain  # type: ignore[return-value]


def error_root(error: dict) -> dict:
    """Return the root-cause error at the end of the chain."""
    if not isinstance(error, dict) or "message" not in error:
        raise RuntimeError("error_root: expected an error dict")
    current = error
    while current.get("cause") is not None:
        current = current["cause"]
    return current


def error_message(error) -> str:
    """Return the message string from an error dict or plain string."""
    if isinstance(error, str):
        return error
    if isinstance(error, dict):
        return str(error.get("message", ""))
    return str(error)


def error_code(error) -> str | None:
    """Return the error code from an error dict, or None."""
    if not isinstance(error, dict):
        return None
    return error.get("code")


def error_context(error) -> dict:
    """Return the context dict from an error dict (or empty dict)."""
    if not isinstance(error, dict):
        return {}
    return error.get("context") or {}


def error_causes(error) -> list:
    """Return all errors in the chain as a list [outermost, ..., root]."""
    if not isinstance(error, dict) or "message" not in error:
        raise RuntimeError("error_causes: expected an error dict")
    chain: list = []
    current: dict | None = error
    while current is not None:
        chain.append(current)
        current = current.get("cause")
    return chain


# ===========================================================================
# Registration
# ===========================================================================


def register_result_utils_functions(runtime: "Runtime") -> None:
    """Register all result_utils functions with the NexusLang runtime."""
    # Result constructors
    runtime.register_function("result_ok", result_ok)
    runtime.register_function("result_err", result_err)
    # Result predicates
    runtime.register_function("result_is_ok", result_is_ok)
    runtime.register_function("result_is_err", result_is_err)
    # Result extraction
    runtime.register_function("result_unwrap", result_unwrap)
    runtime.register_function("result_unwrap_err", result_unwrap_err)
    runtime.register_function("result_unwrap_or", result_unwrap_or)
    runtime.register_function("result_unwrap_or_else", result_unwrap_or_else)
    runtime.register_function("result_expect", result_expect)
    # Result transformations
    runtime.register_function("result_map", result_map)
    runtime.register_function("result_map_err", result_map_err)
    runtime.register_function("result_and_then", result_and_then)
    runtime.register_function("result_or_else", result_or_else)
    # Result ↔ Option conversions
    runtime.register_function("result_to_option", result_to_option)
    runtime.register_function("result_ok_or", result_ok_or)
    # Option constructors
    runtime.register_function("option_some", option_some)
    runtime.register_function("option_none", option_none)
    # Option predicates
    runtime.register_function("option_is_some", option_is_some)
    runtime.register_function("option_is_none", option_is_none)
    # Option extraction
    runtime.register_function("option_unwrap", option_unwrap)
    runtime.register_function("option_unwrap_or", option_unwrap_or)
    runtime.register_function("option_unwrap_or_else", option_unwrap_or_else)
    runtime.register_function("option_expect", option_expect)
    # Option transformations
    runtime.register_function("option_map", option_map)
    runtime.register_function("option_and_then", option_and_then)
    runtime.register_function("option_or", option_or)
    runtime.register_function("option_or_else", option_or_else)
    runtime.register_function("option_filter", option_filter)
    runtime.register_function("option_flatten", option_flatten)
    # Option ↔ Result conversion
    runtime.register_function("option_to_result", option_to_result)
    # Error chain
    runtime.register_function("error_new", error_new)
    runtime.register_function("error_wrap", error_wrap)
    runtime.register_function("error_chain", error_chain)
    runtime.register_function("error_root", error_root)
    runtime.register_function("error_message", error_message)
    runtime.register_function("error_code", error_code)
    runtime.register_function("error_context", error_context)
    runtime.register_function("error_causes", error_causes)

    runtime.register_module("result_utils")
    runtime.register_module("error_handling")
