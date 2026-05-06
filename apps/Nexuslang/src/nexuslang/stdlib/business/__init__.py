"""
NLPL Standard Library - Business Module

Provides functions for business logic, financial calculations,
and enterprise application development.

This module demonstrates NLPL's capability for business applications
alongside all other programming domains.
"""

from typing import Any, Dict, List
from nexuslang.runtime.runtime import Runtime


def calculate_tax(amount: float, rate: float) -> float:
    """Calculate tax on an amount."""
    return amount * (rate / 100.0)


def calculate_discount(price: float, discount_percent: float) -> float:
    """Calculate discounted price."""
    discount = price * (discount_percent / 100.0)
    return price - discount


def format_currency(amount: float, currency: str = "USD") -> str:
    """Format amount as currency string."""
    symbol_map = {
        "USD": "$",
        "EUR": "€",
        "GBP": "£",
        "JPY": "¥"
    }
    symbol = symbol_map.get(currency, currency)
    return f"{symbol}{amount:,.2f}"


def calculate_profit_margin(revenue: float, cost: float) -> float:
    """Calculate profit margin percentage."""
    if revenue == 0:
        return 0.0
    profit = revenue - cost
    return (profit / revenue) * 100.0


def calculate_roi(gain: float, cost: float) -> float:
    """Calculate return on investment percentage."""
    if cost == 0:
        return 0.0
    return ((gain - cost) / cost) * 100.0


def register_business_functions(runtime: Runtime) -> None:
    """Register business functions with the NexusLang runtime."""
    
    # Tax calculations
    runtime.register_function("calculate_tax", calculate_tax)
    runtime.register_function("calculate_discount", calculate_discount)
    
    # Currency formatting
    runtime.register_function("format_currency", format_currency)
    
    # Business metrics
    runtime.register_function("calculate_profit_margin", calculate_profit_margin)
    runtime.register_function("calculate_roi", calculate_roi)


__all__ = [
    'calculate_tax',
    'calculate_discount',
    'format_currency',
    'calculate_profit_margin',
    'calculate_roi',
    'register_business_functions'
]
