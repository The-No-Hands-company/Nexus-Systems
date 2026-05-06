"""
CSV and data parsing utilities for NexusLang.
"""

import csv
import io
from typing import List, Dict, Optional
from ...runtime.runtime import Runtime


def csv_read(filepath: str, has_header: bool = True, delimiter: str = ',') -> List[Dict]:
    """
    Read CSV file and return list of dictionaries.
    If has_header=True, first row is used as keys.
    """
    try:
        with open(filepath, 'r', newline='', encoding='utf-8') as f:
            if has_header:
                reader = csv.DictReader(f, delimiter=delimiter)
                return list(reader)
            else:
                reader = csv.reader(f, delimiter=delimiter)
                return [list(row) for row in reader]
    except Exception as e:
        print(f"CSV read failed: {e}")
        return []


def csv_write(filepath: str, data: List[Dict], fieldnames: Optional[List[str]] = None,
              delimiter: str = ',') -> bool:
    """
    Write list of dictionaries to CSV file.
    """
    try:
        if not data:
            return False
        
        if fieldnames is None and isinstance(data[0], dict):
            fieldnames = list(data[0].keys())
        
        with open(filepath, 'w', newline='', encoding='utf-8') as f:
            if isinstance(data[0], dict):
                writer = csv.DictWriter(f, fieldnames=fieldnames, delimiter=delimiter)
                writer.writeheader()
                writer.writerows(data)
            else:
                writer = csv.writer(f, delimiter=delimiter)
                writer.writerows(data)
        return True
    except Exception as e:
        print(f"CSV write failed: {e}")
        return False


def csv_parse_string(csv_string: str, has_header: bool = True, delimiter: str = ',') -> List[Dict]:
    """Parse CSV from string."""
    try:
        f = io.StringIO(csv_string)
        if has_header:
            reader = csv.DictReader(f, delimiter=delimiter)
            return list(reader)
        else:
            reader = csv.reader(f, delimiter=delimiter)
            return [list(row) for row in reader]
    except Exception as e:
        print(f"CSV parse failed: {e}")
        return []


def csv_to_string(data: List[Dict], fieldnames: Optional[List[str]] = None,
                  delimiter: str = ',') -> str:
    """Convert list of dictionaries to CSV string."""
    try:
        if not data:
            return ""
        
        if fieldnames is None and isinstance(data[0], dict):
            fieldnames = list(data[0].keys())
        
        output = io.StringIO()
        if isinstance(data[0], dict):
            writer = csv.DictWriter(output, fieldnames=fieldnames, delimiter=delimiter)
            writer.writeheader()
            writer.writerows(data)
        else:
            writer = csv.writer(output, delimiter=delimiter)
            writer.writerows(data)
        return output.getvalue()
    except Exception as e:
        print(f"CSV to string failed: {e}")
        return ""


def csv_read_rows(filepath: str, delimiter: str = ',') -> List[List[str]]:
    """Read CSV file as list of rows (no header processing)."""
    try:
        with open(filepath, 'r', newline='', encoding='utf-8') as f:
            reader = csv.reader(f, delimiter=delimiter)
            return [list(row) for row in reader]
    except Exception as e:
        print(f"CSV read rows failed: {e}")
        return []


def csv_write_rows(filepath: str, rows: List[List], delimiter: str = ',') -> bool:
    """Write list of rows to CSV file."""
    try:
        with open(filepath, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f, delimiter=delimiter)
            writer.writerows(rows)
        return True
    except Exception as e:
        print(f"CSV write rows failed: {e}")
        return False


def tsv_read(filepath: str, has_header: bool = True) -> List[Dict]:
    """Read TSV (tab-separated) file."""
    return csv_read(filepath, has_header=has_header, delimiter='\t')


def tsv_write(filepath: str, data: List[Dict], fieldnames: Optional[List[str]] = None) -> bool:
    """Write TSV (tab-separated) file."""
    return csv_write(filepath, data, fieldnames=fieldnames, delimiter='\t')


def register_csv_functions(runtime: Runtime) -> None:
    """Register CSV functions with the runtime."""
    
    # CSV operations
    runtime.register_function("csv_read", csv_read)
    runtime.register_function("csv_write", csv_write)
    runtime.register_function("csv_parse_string", csv_parse_string)
    runtime.register_function("csv_to_string", csv_to_string)
    runtime.register_function("csv_read_rows", csv_read_rows)
    runtime.register_function("csv_write_rows", csv_write_rows)
    
    # TSV operations
    runtime.register_function("tsv_read", tsv_read)
    runtime.register_function("tsv_write", tsv_write)
