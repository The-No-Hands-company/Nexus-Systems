"""
3D Math Library for NexusLang
========================

Provides Vector3, Vector2, Matrix4, Quaternion types and operations
for 3D graphics, game engines, and spatial computations.

This is a NATIVE Python implementation that will be wrapped for NexusLang FFI.
Performance-critical operations can be JIT-compiled or moved to C later.
"""

import math
from typing import Tuple, List
from ...runtime.runtime import Runtime


# ============================================================================
# Vector2 - 2D vector operations
# ============================================================================

class Vector2:
    """2D vector for texture coordinates, screen positions, etc."""
    
    def __init__(self, x: float = 0.0, y: float = 0.0):
        self.x = float(x)
        self.y = float(y)
    
    def __add__(self, other: 'Vector2') -> 'Vector2':
        return Vector2(self.x + other.x, self.y + other.y)
    
    def __sub__(self, other: 'Vector2') -> 'Vector2':
        return Vector2(self.x - other.x, self.y - other.y)
    
    def __mul__(self, scalar: float) -> 'Vector2':
        return Vector2(self.x * scalar, self.y * scalar)
    
    def __truediv__(self, scalar: float) -> 'Vector2':
        return Vector2(self.x / scalar, self.y / scalar)
    
    def length(self) -> float:
        """Calculate vector magnitude."""
        return math.sqrt(self.x * self.x + self.y * self.y)
    
    def length_squared(self) -> float:
        """Calculate squared magnitude (faster, avoid sqrt)."""
        return self.x * self.x + self.y * self.y
    
    def normalize(self) -> 'Vector2':
        """Return unit vector in same direction."""
        length = self.length()
        if length < 1e-6:
            return Vector2(0, 0)
        return Vector2(self.x / length, self.y / length)
    
    def dot(self, other: 'Vector2') -> float:
        """Dot product."""
        return self.x * other.x + self.y * other.y
    
    def distance(self, other: 'Vector2') -> float:
        """Distance to another vector."""
        dx = self.x - other.x
        dy = self.y - other.y
        return math.sqrt(dx * dx + dy * dy)
    
    def lerp(self, other: 'Vector2', t: float) -> 'Vector2':
        """Linear interpolation."""
        return Vector2(
            self.x + (other.x - self.x) * t,
            self.y + (other.y - self.y) * t
        )
    
    def to_list(self) -> List[float]:
        """Convert to list [x, y]."""
        return [self.x, self.y]
    
    def __repr__(self) -> str:
        return f"Vector2({self.x:.3f}, {self.y:.3f})"


# ============================================================================
# Vector3 - 3D vector operations
# ============================================================================

class Vector3:
    """3D vector for positions, directions, normals, etc."""
    
    def __init__(self, x: float = 0.0, y: float = 0.0, z: float = 0.0):
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)
    
    def __add__(self, other: 'Vector3') -> 'Vector3':
        return Vector3(self.x + other.x, self.y + other.y, self.z + other.z)
    
    def __sub__(self, other: 'Vector3') -> 'Vector3':
        return Vector3(self.x - other.x, self.y - other.y, self.z - other.z)
    
    def __mul__(self, scalar: float) -> 'Vector3':
        return Vector3(self.x * scalar, self.y * scalar, self.z * scalar)
    
    def __truediv__(self, scalar: float) -> 'Vector3':
        return Vector3(self.x / scalar, self.y / scalar, self.z / scalar)
    
    def __neg__(self) -> 'Vector3':
        return Vector3(-self.x, -self.y, -self.z)
    
    def length(self) -> float:
        """Calculate vector magnitude."""
        return math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)
    
    def length_squared(self) -> float:
        """Calculate squared magnitude (faster, avoid sqrt)."""
        return self.x * self.x + self.y * self.y + self.z * self.z
    
    def normalize(self) -> 'Vector3':
        """Return unit vector in same direction."""
        length = self.length()
        if length < 1e-6:
            return Vector3(0, 0, 0)
        return Vector3(self.x / length, self.y / length, self.z / length)
    
    def dot(self, other: 'Vector3') -> float:
        """Dot product."""
        return self.x * other.x + self.y * other.y + self.z * other.z
    
    def cross(self, other: 'Vector3') -> 'Vector3':
        """Cross product (perpendicular vector)."""
        return Vector3(
            self.y * other.z - self.z * other.y,
            self.z * other.x - self.x * other.z,
            self.x * other.y - self.y * other.x
        )
    
    def distance(self, other: 'Vector3') -> float:
        """Distance to another vector."""
        dx = self.x - other.x
        dy = self.y - other.y
        dz = self.z - other.z
        return math.sqrt(dx * dx + dy * dy + dz * dz)
    
    def lerp(self, other: 'Vector3', t: float) -> 'Vector3':
        """Linear interpolation."""
        return Vector3(
            self.x + (other.x - self.x) * t,
            self.y + (other.y - self.y) * t,
            self.z + (other.z - self.z) * t
        )
    
    def reflect(self, normal: 'Vector3') -> 'Vector3':
        """Reflect vector across a normal."""
        return self - normal * (2.0 * self.dot(normal))
    
    def project(self, onto: 'Vector3') -> 'Vector3':
        """Project this vector onto another."""
        return onto * (self.dot(onto) / onto.dot(onto))
    
    def angle_to(self, other: 'Vector3') -> float:
        """Angle in radians between two vectors."""
        dot = self.normalize().dot(other.normalize())
        dot = max(-1.0, min(1.0, dot))  # Clamp for numerical stability
        return math.acos(dot)
    
    def to_list(self) -> List[float]:
        """Convert to list [x, y, z]."""
        return [self.x, self.y, self.z]
    
    def __repr__(self) -> str:
        return f"Vector3({self.x:.3f}, {self.y:.3f}, {self.z:.3f})"
    
    # Common vector constants
    @staticmethod
    def zero() -> 'Vector3':
        return Vector3(0, 0, 0)
    
    @staticmethod
    def one() -> 'Vector3':
        return Vector3(1, 1, 1)
    
    @staticmethod
    def up() -> 'Vector3':
        return Vector3(0, 1, 0)
    
    @staticmethod
    def down() -> 'Vector3':
        return Vector3(0, -1, 0)
    
    @staticmethod
    def forward() -> 'Vector3':
        return Vector3(0, 0, 1)
    
    @staticmethod
    def back() -> 'Vector3':
        return Vector3(0, 0, -1)
    
    @staticmethod
    def right() -> 'Vector3':
        return Vector3(1, 0, 0)
    
    @staticmethod
    def left() -> 'Vector3':
        return Vector3(-1, 0, 0)


# ============================================================================
# Matrix4 - 4x4 matrix for 3D transformations
# ============================================================================

class Matrix4:
    """4x4 matrix for 3D transformations (translation, rotation, scale, projection)."""
    
    def __init__(self, data: List[float] = None):
        """Initialize matrix. Data is row-major: [m00, m01, m02, m03, m10, ...]"""
        if data is None:
            # Identity matrix
            self.data = [
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                0.0, 0.0, 0.0, 1.0
            ]
        else:
            assert len(data) == 16, "Matrix4 requires 16 elements"
            self.data = list(data)
    
    def __getitem__(self, index: Tuple[int, int]) -> float:
        """Access element at [row, col]."""
        row, col = index
        return self.data[row * 4 + col]
    
    def __setitem__(self, index: Tuple[int, int], value: float):
        """Set element at [row, col]."""
        row, col = index
        self.data[row * 4 + col] = float(value)
    
    def __mul__(self, other: 'Matrix4') -> 'Matrix4':
        """Matrix multiplication."""
        result = Matrix4()
        for i in range(4):
            for j in range(4):
                sum_val = 0.0
                for k in range(4):
                    sum_val += self[i, k] * other[k, j]
                result[i, j] = sum_val
        return result
    
    def transform_point(self, vec: Vector3) -> Vector3:
        """Transform a 3D point (w=1)."""
        x = self[0, 0] * vec.x + self[0, 1] * vec.y + self[0, 2] * vec.z + self[0, 3]
        y = self[1, 0] * vec.x + self[1, 1] * vec.y + self[1, 2] * vec.z + self[1, 3]
        z = self[2, 0] * vec.x + self[2, 1] * vec.y + self[2, 2] * vec.z + self[2, 3]
        w = self[3, 0] * vec.x + self[3, 1] * vec.y + self[3, 2] * vec.z + self[3, 3]
        
        if abs(w) > 1e-6:
            return Vector3(x / w, y / w, z / w)
        return Vector3(x, y, z)
    
    def transform_direction(self, vec: Vector3) -> Vector3:
        """Transform a direction vector (w=0, no translation)."""
        x = self[0, 0] * vec.x + self[0, 1] * vec.y + self[0, 2] * vec.z
        y = self[1, 0] * vec.x + self[1, 1] * vec.y + self[1, 2] * vec.z
        z = self[2, 0] * vec.x + self[2, 1] * vec.y + self[2, 2] * vec.z
        return Vector3(x, y, z)
    
    def transpose(self) -> 'Matrix4':
        """Return transposed matrix."""
        result = Matrix4()
        for i in range(4):
            for j in range(4):
                result[i, j] = self[j, i]
        return result
    
    def determinant(self) -> float:
        """Calculate determinant (for inversion check)."""
        # Simplified 4x4 determinant calculation
        m = self.data
        return (
            m[0] * (m[5] * (m[10] * m[15] - m[11] * m[14]) -
                    m[6] * (m[9] * m[15] - m[11] * m[13]) +
                    m[7] * (m[9] * m[14] - m[10] * m[13])) -
            m[1] * (m[4] * (m[10] * m[15] - m[11] * m[14]) -
                    m[6] * (m[8] * m[15] - m[11] * m[12]) +
                    m[7] * (m[8] * m[14] - m[10] * m[12])) +
            m[2] * (m[4] * (m[9] * m[15] - m[11] * m[13]) -
                    m[5] * (m[8] * m[15] - m[11] * m[12]) +
                    m[7] * (m[8] * m[13] - m[9] * m[12])) -
            m[3] * (m[4] * (m[9] * m[14] - m[10] * m[13]) -
                    m[5] * (m[8] * m[14] - m[10] * m[12]) +
                    m[6] * (m[8] * m[13] - m[9] * m[12]))
        )
    
    @staticmethod
    def identity() -> 'Matrix4':
        """Create identity matrix."""
        return Matrix4()
    
    @staticmethod
    def translation(x: float, y: float, z: float) -> 'Matrix4':
        """Create translation matrix."""
        return Matrix4([
            1.0, 0.0, 0.0, x,
            0.0, 1.0, 0.0, y,
            0.0, 0.0, 1.0, z,
            0.0, 0.0, 0.0, 1.0
        ])
    
    @staticmethod
    def scale(x: float, y: float, z: float) -> 'Matrix4':
        """Create scale matrix."""
        return Matrix4([
            x,   0.0, 0.0, 0.0,
            0.0, y,   0.0, 0.0,
            0.0, 0.0, z,   0.0,
            0.0, 0.0, 0.0, 1.0
        ])
    
    @staticmethod
    def rotation_x(angle_radians: float) -> 'Matrix4':
        """Create rotation matrix around X axis."""
        c = math.cos(angle_radians)
        s = math.sin(angle_radians)
        return Matrix4([
            1.0, 0.0, 0.0, 0.0,
            0.0, c,   -s,  0.0,
            0.0, s,   c,   0.0,
            0.0, 0.0, 0.0, 1.0
        ])
    
    @staticmethod
    def rotation_y(angle_radians: float) -> 'Matrix4':
        """Create rotation matrix around Y axis."""
        c = math.cos(angle_radians)
        s = math.sin(angle_radians)
        return Matrix4([
            c,   0.0, s,   0.0,
            0.0, 1.0, 0.0, 0.0,
            -s,  0.0, c,   0.0,
            0.0, 0.0, 0.0, 1.0
        ])
    
    @staticmethod
    def rotation_z(angle_radians: float) -> 'Matrix4':
        """Create rotation matrix around Z axis."""
        c = math.cos(angle_radians)
        s = math.sin(angle_radians)
        return Matrix4([
            c,   -s,  0.0, 0.0,
            s,   c,   0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0
        ])
    
    @staticmethod
    def look_at(eye: Vector3, target: Vector3, up: Vector3) -> 'Matrix4':
        """Create view matrix (camera transformation)."""
        forward = (target - eye).normalize()
        right = forward.cross(up).normalize()
        up_corrected = right.cross(forward)
        
        return Matrix4([
            right.x, right.y, right.z, -right.dot(eye),
            up_corrected.x, up_corrected.y, up_corrected.z, -up_corrected.dot(eye),
            -forward.x, -forward.y, -forward.z, forward.dot(eye),
            0.0, 0.0, 0.0, 1.0
        ])
    
    @staticmethod
    def perspective(fov_radians: float, aspect: float, near: float, far: float) -> 'Matrix4':
        """Create perspective projection matrix."""
        tan_half_fov = math.tan(fov_radians / 2.0)
        
        return Matrix4([
            1.0 / (aspect * tan_half_fov), 0.0, 0.0, 0.0,
            0.0, 1.0 / tan_half_fov, 0.0, 0.0,
            0.0, 0.0, -(far + near) / (far - near), -(2.0 * far * near) / (far - near),
            0.0, 0.0, -1.0, 0.0
        ])
    
    @staticmethod
    def orthographic(left: float, right: float, bottom: float, top: float,
                     near: float, far: float) -> 'Matrix4':
        """Create orthographic projection matrix."""
        return Matrix4([
            2.0 / (right - left), 0.0, 0.0, -(right + left) / (right - left),
            0.0, 2.0 / (top - bottom), 0.0, -(top + bottom) / (top - bottom),
            0.0, 0.0, -2.0 / (far - near), -(far + near) / (far - near),
            0.0, 0.0, 0.0, 1.0
        ])
    
    def to_list(self) -> List[float]:
        """Convert to flat list (for OpenGL/Vulkan)."""
        return self.data.copy()
    
    def __repr__(self) -> str:
        return (f"Matrix4(\n"
                f"  [{self[0,0]:.2f}, {self[0,1]:.2f}, {self[0,2]:.2f}, {self[0,3]:.2f}],\n"
                f"  [{self[1,0]:.2f}, {self[1,1]:.2f}, {self[1,2]:.2f}, {self[1,3]:.2f}],\n"
                f"  [{self[2,0]:.2f}, {self[2,1]:.2f}, {self[2,2]:.2f}, {self[2,3]:.2f}],\n"
                f"  [{self[3,0]:.2f}, {self[3,1]:.2f}, {self[3,2]:.2f}, {self[3,3]:.2f}])")


# ============================================================================
# Quaternion - rotation representation (better than Euler angles)
# ============================================================================

class Quaternion:
    """Quaternion for 3D rotations (avoids gimbal lock, smoother interpolation)."""
    
    def __init__(self, w: float = 1.0, x: float = 0.0, y: float = 0.0, z: float = 0.0):
        self.w = float(w)
        self.x = float(x)
        self.y = float(y)
        self.z = float(z)
    
    def __mul__(self, other: 'Quaternion') -> 'Quaternion':
        """Quaternion multiplication (composition of rotations)."""
        return Quaternion(
            self.w * other.w - self.x * other.x - self.y * other.y - self.z * other.z,
            self.w * other.x + self.x * other.w + self.y * other.z - self.z * other.y,
            self.w * other.y - self.x * other.z + self.y * other.w + self.z * other.x,
            self.w * other.z + self.x * other.y - self.y * other.x + self.z * other.w
        )
    
    def length(self) -> float:
        """Calculate quaternion magnitude."""
        return math.sqrt(self.w * self.w + self.x * self.x + 
                        self.y * self.y + self.z * self.z)
    
    def normalize(self) -> 'Quaternion':
        """Return unit quaternion."""
        length = self.length()
        if length < 1e-6:
            return Quaternion()  # Identity
        return Quaternion(self.w / length, self.x / length, 
                         self.y / length, self.z / length)
    
    def conjugate(self) -> 'Quaternion':
        """Return conjugate (inverse for unit quaternions)."""
        return Quaternion(self.w, -self.x, -self.y, -self.z)
    
    def rotate_vector(self, vec: Vector3) -> Vector3:
        """Rotate a vector by this quaternion."""
        # q * v * q^-1
        q_vec = Quaternion(0, vec.x, vec.y, vec.z)
        result = self * q_vec * self.conjugate()
        return Vector3(result.x, result.y, result.z)
    
    def to_matrix4(self) -> Matrix4:
        """Convert to 4x4 rotation matrix."""
        xx = self.x * self.x
        yy = self.y * self.y
        zz = self.z * self.z
        xy = self.x * self.y
        xz = self.x * self.z
        yz = self.y * self.z
        wx = self.w * self.x
        wy = self.w * self.y
        wz = self.w * self.z
        
        return Matrix4([
            1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy), 0.0,
            2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx), 0.0,
            2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy), 0.0,
            0.0, 0.0, 0.0, 1.0
        ])
    
    def to_euler(self) -> Tuple[float, float, float]:
        """Convert to Euler angles (pitch, yaw, roll) in radians."""
        # Pitch (x-axis rotation)
        sinp = 2.0 * (self.w * self.y - self.z * self.x)
        if abs(sinp) >= 1:
            pitch = math.copysign(math.pi / 2, sinp)
        else:
            pitch = math.asin(sinp)
        
        # Yaw (y-axis rotation)
        siny_cosp = 2.0 * (self.w * self.z + self.x * self.y)
        cosy_cosp = 1.0 - 2.0 * (self.y * self.y + self.z * self.z)
        yaw = math.atan2(siny_cosp, cosy_cosp)
        
        # Roll (z-axis rotation)
        sinr_cosp = 2.0 * (self.w * self.x + self.y * self.z)
        cosr_cosp = 1.0 - 2.0 * (self.x * self.x + self.y * self.y)
        roll = math.atan2(sinr_cosp, cosr_cosp)
        
        return (pitch, yaw, roll)
    
    @staticmethod
    def identity() -> 'Quaternion':
        """Create identity quaternion (no rotation)."""
        return Quaternion(1, 0, 0, 0)
    
    @staticmethod
    def from_axis_angle(axis: Vector3, angle_radians: float) -> 'Quaternion':
        """Create quaternion from axis and angle."""
        half_angle = angle_radians / 2.0
        s = math.sin(half_angle)
        axis_norm = axis.normalize()
        return Quaternion(
            math.cos(half_angle),
            axis_norm.x * s,
            axis_norm.y * s,
            axis_norm.z * s
        )
    
    @staticmethod
    def from_euler(pitch: float, yaw: float, roll: float) -> 'Quaternion':
        """Create quaternion from Euler angles (radians)."""
        cy = math.cos(yaw * 0.5)
        sy = math.sin(yaw * 0.5)
        cp = math.cos(pitch * 0.5)
        sp = math.sin(pitch * 0.5)
        cr = math.cos(roll * 0.5)
        sr = math.sin(roll * 0.5)
        
        return Quaternion(
            cr * cp * cy + sr * sp * sy,
            sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy
        )
    
    @staticmethod
    def slerp(q1: 'Quaternion', q2: 'Quaternion', t: float) -> 'Quaternion':
        """Spherical linear interpolation (smooth rotation interpolation)."""
        # Normalize inputs
        q1 = q1.normalize()
        q2 = q2.normalize()
        
        # Compute dot product
        dot = q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z
        
        # If negative, negate one quaternion for shortest path
        if dot < 0.0:
            q2 = Quaternion(-q2.w, -q2.x, -q2.y, -q2.z)
            dot = -dot
        
        # Clamp dot product
        dot = max(-1.0, min(1.0, dot))
        
        # If quaternions are very close, use linear interpolation
        if dot > 0.9995:
            return Quaternion(
                q1.w + t * (q2.w - q1.w),
                q1.x + t * (q2.x - q1.x),
                q1.y + t * (q2.y - q1.y),
                q1.z + t * (q2.z - q1.z)
            ).normalize()
        
        # Calculate angle
        theta = math.acos(dot)
        sin_theta = math.sin(theta)
        
        # Calculate weights
        w1 = math.sin((1.0 - t) * theta) / sin_theta
        w2 = math.sin(t * theta) / sin_theta
        
        return Quaternion(
            q1.w * w1 + q2.w * w2,
            q1.x * w1 + q2.x * w2,
            q1.y * w1 + q2.y * w2,
            q1.z * w1 + q2.z * w2
        )
    
    def __repr__(self) -> str:
        return f"Quaternion(w={self.w:.3f}, x={self.x:.3f}, y={self.y:.3f}, z={self.z:.3f})"


# ============================================================================
# NexusLang Runtime Registration
# ============================================================================

def register_math3d_functions(runtime: Runtime) -> None:
    """Register 3D math functions with the NexusLang runtime."""
    
    # Vector2 constructors and operations
    runtime.register_function("Vector2", lambda x=0.0, y=0.0: Vector2(x, y))
    runtime.register_function("vec2_add", lambda v1, v2: v1 + v2)
    runtime.register_function("vec2_sub", lambda v1, v2: v1 - v2)
    runtime.register_function("vec2_mul", lambda v, s: v * s)
    runtime.register_function("vec2_div", lambda v, s: v / s)
    runtime.register_function("vec2_length", lambda v: v.length())
    runtime.register_function("vec2_normalize", lambda v: v.normalize())
    runtime.register_function("vec2_dot", lambda v1, v2: v1.dot(v2))
    runtime.register_function("vec2_distance", lambda v1, v2: v1.distance(v2))
    runtime.register_function("vec2_lerp", lambda v1, v2, t: v1.lerp(v2, t))
    
    # Vector3 constructors and operations
    runtime.register_function("Vector3", lambda x=0.0, y=0.0, z=0.0: Vector3(x, y, z))
    runtime.register_function("vec3_add", lambda v1, v2: v1 + v2)
    runtime.register_function("vec3_sub", lambda v1, v2: v1 - v2)
    runtime.register_function("vec3_mul", lambda v, s: v * s)
    runtime.register_function("vec3_div", lambda v, s: v / s)
    runtime.register_function("vec3_length", lambda v: v.length())
    runtime.register_function("vec3_normalize", lambda v: v.normalize())
    runtime.register_function("vec3_dot", lambda v1, v2: v1.dot(v2))
    runtime.register_function("vec3_cross", lambda v1, v2: v1.cross(v2))
    runtime.register_function("vec3_distance", lambda v1, v2: v1.distance(v2))
    runtime.register_function("vec3_lerp", lambda v1, v2, t: v1.lerp(v2, t))
    runtime.register_function("vec3_reflect", lambda v, n: v.reflect(n))
    runtime.register_function("vec3_angle", lambda v1, v2: v1.angle_to(v2))
    
    # Vector3 constants
    runtime.register_function("vec3_zero", Vector3.zero)
    runtime.register_function("vec3_one", Vector3.one)
    runtime.register_function("vec3_up", Vector3.up)
    runtime.register_function("vec3_down", Vector3.down)
    runtime.register_function("vec3_forward", Vector3.forward)
    runtime.register_function("vec3_back", Vector3.back)
    runtime.register_function("vec3_right", Vector3.right)
    runtime.register_function("vec3_left", Vector3.left)
    
    # Matrix4 constructors and operations
    runtime.register_function("Matrix4", lambda data=None: Matrix4(data))
    runtime.register_function("mat4_identity", Matrix4.identity)
    runtime.register_function("mat4_translation", Matrix4.translation)
    runtime.register_function("mat4_scale", Matrix4.scale)
    runtime.register_function("mat4_rotation_x", Matrix4.rotation_x)
    runtime.register_function("mat4_rotation_y", Matrix4.rotation_y)
    runtime.register_function("mat4_rotation_z", Matrix4.rotation_z)
    runtime.register_function("mat4_look_at", Matrix4.look_at)
    runtime.register_function("mat4_perspective", Matrix4.perspective)
    runtime.register_function("mat4_orthographic", Matrix4.orthographic)
    runtime.register_function("mat4_multiply", lambda m1, m2: m1 * m2)
    runtime.register_function("mat4_transform_point", lambda m, v: m.transform_point(v))
    runtime.register_function("mat4_transform_direction", lambda m, v: m.transform_direction(v))
    runtime.register_function("mat4_transpose", lambda m: m.transpose())
    runtime.register_function("mat4_to_list", lambda m: m.to_list())
    
    # Quaternion constructors and operations
    runtime.register_function("Quaternion", lambda w=1.0, x=0.0, y=0.0, z=0.0: Quaternion(w, x, y, z))
    runtime.register_function("quat_identity", Quaternion.identity)
    runtime.register_function("quat_from_axis_angle", Quaternion.from_axis_angle)
    runtime.register_function("quat_from_euler", Quaternion.from_euler)
    runtime.register_function("quat_multiply", lambda q1, q2: q1 * q2)
    runtime.register_function("quat_normalize", lambda q: q.normalize())
    runtime.register_function("quat_conjugate", lambda q: q.conjugate())
    runtime.register_function("quat_rotate_vector", lambda q, v: q.rotate_vector(v))
    runtime.register_function("quat_to_matrix4", lambda q: q.to_matrix4())
    runtime.register_function("quat_to_euler", lambda q: q.to_euler())
    runtime.register_function("quat_slerp", Quaternion.slerp)
    
    # Utility functions
    runtime.register_function("radians", lambda degrees: degrees * math.pi / 180.0)
    runtime.register_function("degrees", lambda radians: radians * 180.0 / math.pi)
