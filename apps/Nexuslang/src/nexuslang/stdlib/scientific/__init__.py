"""
NLPL Standard Library - Scientific Computing Module

Provides functions for numerical computing, physics simulations,
and scientific calculations.

This module demonstrates NLPL's capability for scientific computing
alongside all other programming domains.
"""

from typing import Tuple
import math
from nexuslang.runtime.runtime import Runtime


# Physical constants
GRAVITY = 9.81  # m/s^2
SPEED_OF_LIGHT = 299792458  # m/s
AVOGADRO_NUMBER = 6.02214076e23  # mol^-1


def calculate_projectile_range(velocity: float, angle: float) -> float:
    """Calculate projectile range given initial velocity and angle."""
    angle_rad = math.radians(angle)
    return (velocity ** 2 * math.sin(2 * angle_rad)) / GRAVITY


def calculate_trajectory(velocity: float, angle: float, time: float) -> Tuple[float, float]:
    """Calculate projectile position at given time."""
    angle_rad = math.radians(angle)
    vx = velocity * math.cos(angle_rad)
    vy = velocity * math.sin(angle_rad)
    
    x = vx * time
    y = vy * time - 0.5 * GRAVITY * time ** 2
    
    return (x, y)


def calculate_kinetic_energy(mass: float, velocity: float) -> float:
    """Calculate kinetic energy: KE = 0.5 * m * v^2"""
    return 0.5 * mass * velocity ** 2


def calculate_potential_energy(mass: float, height: float) -> float:
    """Calculate gravitational potential energy: PE = m * g * h"""
    return mass * GRAVITY * height


def solve_quadratic(a: float, b: float, c: float) -> Tuple[float, float]:
    """Solve quadratic equation ax^2 + bx + c = 0"""
    discriminant = b ** 2 - 4 * a * c
    
    if discriminant < 0:
        raise ValueError("No real solutions")
    
    sqrt_disc = math.sqrt(discriminant)
    x1 = (-b + sqrt_disc) / (2 * a)
    x2 = (-b - sqrt_disc) / (2 * a)
    
    return (x1, x2)


def celsius_to_fahrenheit(celsius: float) -> float:
    """Convert Celsius to Fahrenheit."""
    return (celsius * 9/5) + 32


def fahrenheit_to_celsius(fahrenheit: float) -> float:
    """Convert Fahrenheit to Celsius."""
    return (fahrenheit - 32) * 5/9


def calculate_distance(x1: float, y1: float, x2: float, y2: float) -> float:
    """Calculate Euclidean distance between two points."""
    return math.sqrt((x2 - x1) ** 2 + (y2 - y1) ** 2)


def register_scientific_functions(runtime: Runtime) -> None:
    """Register scientific computing functions with the NexusLang runtime."""
    
    # Physics calculations
    runtime.register_function("calculate_projectile_range", calculate_projectile_range)
    runtime.register_function("calculate_trajectory", calculate_trajectory)
    runtime.register_function("calculate_kinetic_energy", calculate_kinetic_energy)
    runtime.register_function("calculate_potential_energy", calculate_potential_energy)
    
    # Mathematical functions
    runtime.register_function("solve_quadratic", solve_quadratic)
    runtime.register_function("calculate_distance", calculate_distance)
    
    # Unit conversions
    runtime.register_function("celsius_to_fahrenheit", celsius_to_fahrenheit)
    runtime.register_function("fahrenheit_to_celsius", fahrenheit_to_celsius)
    
    # Physical constants
    runtime.register_constant("GRAVITY", GRAVITY)
    runtime.register_constant("SPEED_OF_LIGHT", SPEED_OF_LIGHT)
    runtime.register_constant("AVOGADRO_NUMBER", AVOGADRO_NUMBER)


__all__ = [
    'GRAVITY',
    'SPEED_OF_LIGHT',
    'AVOGADRO_NUMBER',
    'calculate_projectile_range',
    'calculate_trajectory',
    'calculate_kinetic_energy',
    'calculate_potential_energy',
    'solve_quadratic',
    'celsius_to_fahrenheit',
    'fahrenheit_to_celsius',
    'calculate_distance',
    'register_scientific_functions'
]
