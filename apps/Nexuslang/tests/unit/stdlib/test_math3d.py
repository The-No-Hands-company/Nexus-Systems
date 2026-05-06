#!/usr/bin/env python3
"""
Test script for 3D Math Library
Runs all Vector3, Matrix4, and Quaternion operations to verify correctness
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from nexuslang.stdlib.math3d import Vector2, Vector3, Matrix4, Quaternion
import math

def test_vector2():
    print("Testing Vector2...")
    v1 = Vector2(3, 4)
    assert v1.length() == 5.0, "Vector2 length failed"
    
    v2 = Vector2(1, 0)
    v3 = v1 + v2
    assert v3.x == 4 and v3.y == 4, "Vector2 addition failed"
    
    normalized = v1.normalize()
    assert abs(normalized.length() - 1.0) < 0.001, "Vector2 normalize failed"
    
    dot = v1.dot(v2)
    assert dot == 3.0, "Vector2 dot product failed"
    
    print("  ✓ All Vector2 tests passed")

def test_vector3():
    print("\nTesting Vector3...")
    
    # Length test
    v1 = Vector3(1, 2, 2)
    expected_length = 3.0
    assert abs(v1.length() - expected_length) < 0.001, f"Length: expected {expected_length}, got {v1.length()}"
    
    # Addition
    v2 = Vector3(1, 1, 1)
    v3 = v1 + v2
    assert v3.x == 2 and v3.y == 3 and v3.z == 3, "Addition failed"
    
    # Subtraction
    v4 = v3 - v2
    assert v4.x == 1 and v4.y == 2 and v4.z == 2, "Subtraction failed"
    
    # Scalar multiplication
    v5 = v1 * 2
    assert v5.x == 2 and v5.y == 4 and v5.z == 4, "Scalar multiplication failed"
    
    # Dot product
    dot = v1.dot(v2)
    assert dot == 5.0, f"Dot product: expected 5.0, got {dot}"
    
    # Cross product (critical for graphics!)
    right = Vector3(1, 0, 0)
    up = Vector3(0, 1, 0)
    forward = right.cross(up)
    assert abs(forward.x) < 0.001 and abs(forward.y) < 0.001 and abs(forward.z - 1.0) < 0.001, \
        f"Cross product failed: {forward}"
    
    # Normalize
    v6 = Vector3(3, 4, 0)
    normalized = v6.normalize()
    assert abs(normalized.length() - 1.0) < 0.001, "Normalize failed"
    
    # Distance - v1=(1,2,2) to v2=(1,1,1) = sqrt((1-1)² + (2-1)² + (2-1)²) = sqrt(2)
    dist = v1.distance(v2)
    dx, dy, dz = v1.x - v2.x, v1.y - v2.y, v1.z - v2.z
    expected_dist = math.sqrt(dx**2 + dy**2 + dz**2)
    assert abs(dist - expected_dist) < 0.001, f"Distance: expected {expected_dist}, got {dist}"
    
    # Constants
    assert Vector3.zero().length() == 0, "zero() constant failed"
    assert Vector3.up().y == 1, "up() constant failed"
    assert Vector3.forward().z == 1, "forward() constant failed"
    
    print("  ✓ All Vector3 tests passed")

def test_matrix4():
    print("\nTesting Matrix4...")
    
    # Identity matrix
    identity = Matrix4.identity()
    v = Vector3(1, 2, 3)
    transformed = identity.transform_point(v)
    assert abs(transformed.x - 1) < 0.001 and abs(transformed.y - 2) < 0.001 and abs(transformed.z - 3) < 0.001, \
        "Identity transform failed"
    
    # Translation
    translation = Matrix4.translation(10, 20, 30)
    translated = translation.transform_point(v)
    assert abs(translated.x - 11) < 0.001 and abs(translated.y - 22) < 0.001 and abs(translated.z - 33) < 0.001, \
        f"Translation failed: {translated}"
    
    # Scale
    scale = Matrix4.scale(2, 2, 2)
    scaled = scale.transform_point(v)
    assert abs(scaled.x - 2) < 0.001 and abs(scaled.y - 4) < 0.001 and abs(scaled.z - 6) < 0.001, \
        "Scale failed"
    
    # Rotation (90 degrees around Y axis: X becomes -Z, Z becomes X)
    rotation_y = Matrix4.rotation_y(math.pi / 2)
    point = Vector3(1, 0, 0)
    rotated = rotation_y.transform_point(point)
    assert abs(rotated.x) < 0.001 and abs(rotated.y) < 0.001 and abs(rotated.z + 1) < 0.001, \
        f"Rotation Y failed: {rotated} (expected ~0, 0, -1)"
    
    # Matrix multiplication (critical!)
    m1 = Matrix4.translation(5, 0, 0)
    m2 = Matrix4.scale(2, 1, 1)
    combined = m1 * m2
    result = combined.transform_point(Vector3(1, 0, 0))
    # First scale (1*2 = 2), then translate (2+5 = 7)
    assert abs(result.x - 7) < 0.001, f"Matrix multiplication failed: {result.x} (expected 7)"
    
    # Transpose
    m = Matrix4([
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    ])
    mt = m.transpose()
    assert mt[0, 1] == 5 and mt[1, 0] == 2, "Transpose failed"
    
    # Look-at matrix
    eye = Vector3(0, 0, 5)
    target = Vector3(0, 0, 0)
    up = Vector3(0, 1, 0)
    view = Matrix4.look_at(eye, target, up)
    # View matrix should move world opposite to camera
    world_point = Vector3(0, 0, 0)
    view_space = view.transform_point(world_point)
    # Camera at (0,0,5) looking at origin: origin should be at (0,0,-5) in view space
    assert abs(view_space.z + 5) < 0.1, f"Look-at failed: {view_space}"
    
    print("  ✓ All Matrix4 tests passed")

def test_quaternion():
    print("\nTesting Quaternion...")
    
    # Identity
    identity = Quaternion.identity()
    v = Vector3(1, 0, 0)
    rotated = identity.rotate_vector(v)
    assert abs(rotated.x - 1) < 0.001 and abs(rotated.y) < 0.001 and abs(rotated.z) < 0.001, \
        "Identity quaternion failed"
    
    # 90 degree rotation around Y axis
    axis = Vector3(0, 1, 0)
    angle = math.pi / 2
    quat = Quaternion.from_axis_angle(axis, angle)
    
    point = Vector3(1, 0, 0)
    rotated = quat.rotate_vector(point)
    # (1, 0, 0) rotated 90° around Y should give (0, 0, -1)
    assert abs(rotated.x) < 0.001 and abs(rotated.y) < 0.001 and abs(rotated.z + 1) < 0.001, \
        f"Axis-angle rotation failed: {rotated} (expected ~0, 0, -1)"
    
    # Quaternion multiplication
    q1 = Quaternion.from_axis_angle(Vector3(0, 1, 0), math.pi / 4)  # 45° around Y
    q2 = Quaternion.from_axis_angle(Vector3(0, 1, 0), math.pi / 4)  # 45° around Y
    q_combined = q1 * q2  # Should be 90° around Y
    
    rotated2 = q_combined.rotate_vector(Vector3(1, 0, 0))
    assert abs(rotated2.x) < 0.001 and abs(rotated2.z + 1) < 0.001, \
        f"Quaternion multiplication failed: {rotated2}"
    
    # SLERP (critical for animation!)
    q_start = Quaternion.identity()
    q_end = Quaternion.from_axis_angle(Vector3(0, 1, 0), math.pi)  # 180° rotation
    
    q_half = Quaternion.slerp(q_start, q_end, 0.5)  # Should be 90° rotation
    rotated3 = q_half.rotate_vector(Vector3(1, 0, 0))
    assert abs(rotated3.x) < 0.001 and abs(rotated3.z + 1) < 0.001, \
        f"SLERP failed: {rotated3} (expected ~0, 0, -1 for 90° rotation)"
    
    # Normalize
    q = Quaternion(1, 1, 1, 1)
    normalized = q.normalize()
    assert abs(normalized.length() - 1.0) < 0.001, "Quaternion normalize failed"
    
    # Conjugate
    q = Quaternion(0.7071, 0, 0.7071, 0)  # 90° around Y
    conj = q.conjugate()
    # Conjugate should reverse rotation
    forward = Vector3(0, 0, 1)
    rotated_forward = q.rotate_vector(forward)
    rotated_back = conj.rotate_vector(rotated_forward)
    assert abs(rotated_back.x) < 0.01 and abs(rotated_back.z - 1) < 0.01, \
        f"Conjugate failed: {rotated_back} (expected ~0, 0, 1)"
    
    # Euler conversion
    pitch, yaw, roll = 0.1, 0.2, 0.3
    q = Quaternion.from_euler(pitch, yaw, roll)
    p2, y2, r2 = q.to_euler()
    assert abs(pitch - p2) < 0.01 and abs(yaw - y2) < 0.01 and abs(roll - r2) < 0.01, \
        "Euler conversion failed"
    
    print("  ✓ All Quaternion tests passed")

def run_all_tests():
    print("=" * 60)
    print("3D Math Library Test Suite")
    print("=" * 60)
    
    try:
        test_vector2()
        test_vector3()
        test_matrix4()
        test_quaternion()
        
        print("\n" + "=" * 60)
        print("✓ ALL TESTS PASSED")
        print("=" * 60)
        return 0
    
    except AssertionError as e:
        print(f"\n✗ TEST FAILED: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    except Exception as e:
        print(f"\n✗ UNEXPECTED ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == "__main__":
    sys.exit(run_all_tests())
