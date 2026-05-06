"""
Date and Time operations for NexusLang.
"""

import time
from datetime import datetime, timedelta, timezone, date
from typing import Optional, Any
from ...runtime.runtime import Runtime


def now() -> datetime:
    """Get current datetime."""
    return datetime.now()


def today() -> date:
    """Get current date (date object, not datetime)."""
    return date.today()


def utc_now() -> datetime:
    """Get current UTC datetime."""
    return datetime.now(timezone.utc)


def timestamp() -> float:
    """Get current Unix timestamp (seconds since epoch)."""
    return time.time()


def unix_timestamp() -> int:
    """Get current Unix timestamp as integer (seconds since epoch)."""
    return int(time.time())


def timestamp_ms() -> int:
    """Get current timestamp in milliseconds."""
    return int(time.time() * 1000)


def from_timestamp(ts: float, utc: bool = False) -> datetime:
    """Convert Unix timestamp to datetime."""
    if utc:
        return datetime.fromtimestamp(ts, tz=timezone.utc)
    return datetime.fromtimestamp(ts)


def to_timestamp(dt: datetime) -> float:
    """Convert datetime to Unix timestamp."""
    return dt.timestamp()


def parse_datetime(date_string: str, format: str = "%Y-%m-%d %H:%M:%S") -> datetime:
    """Parse datetime string with format."""
    try:
        return datetime.strptime(date_string, format)
    except ValueError as e:
        raise ValueError(f"Cannot parse datetime: {e}")


def format_datetime(dt: datetime, format: str = "%Y-%m-%d %H:%M:%S") -> str:
    """Format datetime as string."""
    return dt.strftime(format)


def parse_date(date_string: str, format: str = "%Y-%m-%d") -> datetime:
    """Parse date string."""
    try:
        return datetime.strptime(date_string, format)
    except ValueError as e:
        raise ValueError(f"Cannot parse date: {e}")


def format_date(dt: datetime, format: str = "%Y-%m-%d") -> str:
    """Format date as string."""
    return dt.strftime(format)


def parse_time(time_string: str, format: str = "%H:%M:%S") -> datetime:
    """Parse time string."""
    try:
        return datetime.strptime(time_string, format)
    except ValueError as e:
        raise ValueError(f"Cannot parse time: {e}")


def format_time(dt: datetime, format: str = "%H:%M:%S") -> str:
    """Format time as string."""
    return dt.strftime(format)


def parse_iso(iso_string: str) -> datetime:
    """Parse ISO 8601 datetime string."""
    try:
        return datetime.fromisoformat(iso_string)
    except ValueError as e:
        raise ValueError(f"Invalid ISO format: {e}")


def to_iso(dt: datetime) -> str:
    """Convert datetime to ISO 8601 string."""
    return dt.isoformat()


def add_days(dt: datetime, days: int) -> datetime:
    """Add days to datetime."""
    return dt + timedelta(days=days)


def add_hours(dt: datetime, hours: int) -> datetime:
    """Add hours to datetime."""
    return dt + timedelta(hours=hours)


def add_minutes(dt: datetime, minutes: int) -> datetime:
    """Add minutes to datetime."""
    return dt + timedelta(minutes=minutes)


def add_seconds(dt: datetime, seconds: int) -> datetime:
    """Add seconds to datetime."""
    return dt + timedelta(seconds=seconds)


def diff_days(dt1: datetime, dt2: datetime) -> int:
    """Get difference in days between two datetimes."""
    return (dt1 - dt2).days


def diff_seconds(dt1: datetime, dt2: datetime) -> float:
    """Get difference in seconds between two datetimes."""
    return (dt1 - dt2).total_seconds()


def get_year(dt: datetime) -> int:
    """Get year from datetime."""
    return dt.year


def get_month(dt: datetime) -> int:
    """Get month from datetime."""
    return dt.month


def get_day(dt: datetime) -> int:
    """Get day from datetime."""
    return dt.day


def get_hour(dt: datetime) -> int:
    """Get hour from datetime."""
    return dt.hour


def get_minute(dt: datetime) -> int:
    """Get minute from datetime."""
    return dt.minute


def get_second(dt: datetime) -> int:
    """Get second from datetime."""
    return dt.second


def get_weekday(dt: datetime) -> int:
    """Get weekday (0=Monday, 6=Sunday)."""
    return dt.weekday()


def get_weekday_name(dt: datetime) -> str:
    """Get weekday name (Monday, Tuesday, etc)."""
    return dt.strftime("%A")


def get_month_name(dt: datetime) -> str:
    """Get month name (January, February, etc)."""
    return dt.strftime("%B")


def sleep(seconds: float) -> None:
    """Sleep for specified seconds."""
    time.sleep(seconds)


def sleep_ms(milliseconds: int) -> None:
    """Sleep for specified milliseconds."""
    time.sleep(milliseconds / 1000.0)


def measure_time(func, *args, **kwargs) -> tuple:
    """Measure execution time of function. Returns (result, elapsed_seconds)."""
    start = time.time()
    result = func(*args, **kwargs)
    elapsed = time.time() - start
    return result, elapsed


def register_datetime_functions(runtime: Runtime) -> None:
    """Register datetime functions with the runtime."""
    
    # Current time
    runtime.register_function("now", now)
    runtime.register_function("today", today)
    runtime.register_function("utc_now", utc_now)
    runtime.register_function("timestamp", timestamp)
    runtime.register_function("unix_timestamp", unix_timestamp)
    runtime.register_function("timestamp_ms", timestamp_ms)
    
    # Conversion
    runtime.register_function("from_timestamp", from_timestamp)
    runtime.register_function("to_timestamp", to_timestamp)
    
    # Parsing
    runtime.register_function("parse_datetime", parse_datetime)
    runtime.register_function("format_datetime", format_datetime)
    runtime.register_function("parse_date", parse_date)
    runtime.register_function("format_date", format_date)
    runtime.register_function("parse_time", parse_time)
    runtime.register_function("format_time", format_time)
    runtime.register_function("parse_iso", parse_iso)
    runtime.register_function("to_iso", to_iso)
    
    # Arithmetic
    runtime.register_function("add_days", add_days)
    runtime.register_function("add_hours", add_hours)
    runtime.register_function("add_minutes", add_minutes)
    runtime.register_function("add_seconds", add_seconds)
    runtime.register_function("diff_days", diff_days)
    runtime.register_function("diff_seconds", diff_seconds)
    
    # Components
    runtime.register_function("get_year", get_year)
    runtime.register_function("get_month", get_month)
    runtime.register_function("get_day", get_day)
    runtime.register_function("get_hour", get_hour)
    runtime.register_function("get_minute", get_minute)
    runtime.register_function("get_second", get_second)
    runtime.register_function("get_weekday", get_weekday)
    runtime.register_function("get_weekday_name", get_weekday_name)
    runtime.register_function("get_month_name", get_month_name)
    
    # Utilities
    runtime.register_function("sleep", sleep)
    runtime.register_function("sleep_ms", sleep_ms)
    runtime.register_function("measure_time", measure_time)
