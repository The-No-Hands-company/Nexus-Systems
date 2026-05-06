"""
Camera system for NexusLang 3D engine
Provides FPS camera and orbital camera implementations
"""

import math
from ..math3d import Vector3, Matrix4


class Camera:
    """Base camera class"""
    
    def __init__(self, position: Vector3, target: Vector3, up: Vector3):
        self.position = position
        self.target = target
        self.up = up
    
    def get_view_matrix(self) -> Matrix4:
        """Get view matrix (camera transformation)"""
        return Matrix4.look_at(self.position, self.target, self.up)
    
    def get_view_matrix_list(self):
        """Get view matrix as flat list for shader uniforms"""
        mat = self.get_view_matrix()
        return [mat.data[i] for i in range(16)]


class FPSCamera(Camera):
    """First-person camera with WASD movement and mouse look"""
    
    def __init__(self, position: Vector3 = None):
        if position is None:
            position = Vector3(0, 0, 3)
        
        self.position = position
        self.yaw = -90.0  # Looking down -Z
        self.pitch = 0.0
        self.speed = 2.5
        self.sensitivity = 0.1
        
        # Calculate initial vectors
        self.front = Vector3(0, 0, -1)
        self.up = Vector3(0, 1, 0)
        self.right = Vector3(1, 0, 0)
        self.world_up = Vector3(0, 1, 0)
        
        self.update_vectors()
    
    def update_vectors(self):
        """Update camera vectors from yaw and pitch"""
        # Calculate new front vector
        front_x = math.cos(math.radians(self.yaw)) * math.cos(math.radians(self.pitch))
        front_y = math.sin(math.radians(self.pitch))
        front_z = math.sin(math.radians(self.yaw)) * math.cos(math.radians(self.pitch))
        
        self.front = Vector3(front_x, front_y, front_z).normalize()
        self.right = self.front.cross(self.world_up).normalize()
        self.up = self.right.cross(self.front).normalize()
    
    def move_forward(self, delta_time: float):
        """Move camera forward"""
        self.position = self.position + self.front * (self.speed * delta_time)
    
    def move_backward(self, delta_time: float):
        """Move camera backward"""
        self.position = self.position - self.front * (self.speed * delta_time)
    
    def move_left(self, delta_time: float):
        """Strafe left"""
        self.position = self.position - self.right * (self.speed * delta_time)
    
    def move_right(self, delta_time: float):
        """Strafe right"""
        self.position = self.position + self.right * (self.speed * delta_time)
    
    def move_up(self, delta_time: float):
        """Move up"""
        self.position = self.position + self.world_up * (self.speed * delta_time)
    
    def move_down(self, delta_time: float):
        """Move down"""
        self.position = self.position - self.world_up * (self.speed * delta_time)
    
    def rotate(self, yaw_offset: float, pitch_offset: float):
        """Rotate camera with mouse"""
        self.yaw += yaw_offset * self.sensitivity
        self.pitch += pitch_offset * self.sensitivity
        
        # Constrain pitch
        if self.pitch > 89.0:
            self.pitch = 89.0
        if self.pitch < -89.0:
            self.pitch = -89.0
        
        self.update_vectors()
    
    def get_view_matrix(self) -> Matrix4:
        """Get view matrix"""
        target = self.position + self.front
        return Matrix4.look_at(self.position, target, self.up)
    
    def get_position(self):
        """Get position as tuple"""
        return (self.position.x, self.position.y, self.position.z)
    
    def get_front(self):
        """Get front vector as tuple"""
        return (self.front.x, self.front.y, self.front.z)


class OrbitalCamera(Camera):
    """Orbital camera that orbits around a target point"""
    
    def __init__(self, target: Vector3 = None, distance: float = 5.0):
        if target is None:
            target = Vector3(0, 0, 0)
        
        self.target = target
        self.distance = distance
        self.azimuth = 0.0  # Horizontal angle
        self.elevation = 30.0  # Vertical angle
        self.min_distance = 1.0
        self.max_distance = 100.0
        self.sensitivity = 0.5
        
        self.update_position()
    
    def update_position(self):
        """Calculate camera position from spherical coordinates"""
        azimuth_rad = math.radians(self.azimuth)
        elevation_rad = math.radians(self.elevation)
        
        x = self.distance * math.cos(elevation_rad) * math.sin(azimuth_rad)
        y = self.distance * math.sin(elevation_rad)
        z = self.distance * math.cos(elevation_rad) * math.cos(azimuth_rad)
        
        self.position = Vector3(
            self.target.x + x,
            self.target.y + y,
            self.target.z + z
        )
    
    def rotate(self, azimuth_delta: float, elevation_delta: float):
        """Rotate camera around target"""
        self.azimuth += azimuth_delta * self.sensitivity
        self.elevation += elevation_delta * self.sensitivity
        
        # Constrain elevation
        if self.elevation > 89.0:
            self.elevation = 89.0
        if self.elevation < -89.0:
            self.elevation = -89.0
        
        self.update_position()
    
    def zoom(self, amount: float):
        """Zoom in/out"""
        self.distance -= amount
        
        if self.distance < self.min_distance:
            self.distance = self.min_distance
        if self.distance > self.max_distance:
            self.distance = self.max_distance
        
        self.update_position()
    
    def pan(self, x_offset: float, y_offset: float):
        """Pan target point"""
        # Calculate camera right and up vectors
        view_dir = (self.target - self.position).normalize()
        right = view_dir.cross(Vector3(0, 1, 0)).normalize()
        up = right.cross(view_dir).normalize()
        
        # Move target
        self.target = self.target + right * x_offset * 0.01
        self.target = self.target + up * y_offset * 0.01
        
        self.update_position()
    
    def get_view_matrix(self) -> Matrix4:
        """Get view matrix"""
        return Matrix4.look_at(self.position, self.target, Vector3(0, 1, 0))


def register_camera_functions(runtime):
    """Register camera functions with NexusLang runtime"""
    
    cameras = {}
    next_camera_id = [1]  # Use list for mutable closure
    
    def create_fps_camera(x: float = 0.0, y: float = 0.0, z: float = 3.0) -> int:
        """Create FPS camera"""
        camera = FPSCamera(Vector3(x, y, z))
        camera_id = next_camera_id[0]
        cameras[camera_id] = camera
        next_camera_id[0] += 1
        return camera_id
    
    def create_orbital_camera(target_x: float = 0.0, target_y: float = 0.0, 
                            target_z: float = 0.0, distance: float = 5.0) -> int:
        """Create orbital camera"""
        camera = OrbitalCamera(Vector3(target_x, target_y, target_z), distance)
        camera_id = next_camera_id[0]
        cameras[camera_id] = camera
        next_camera_id[0] += 1
        return camera_id
    
    def camera_get_view_matrix(camera_id: int):
        """Get view matrix as list"""
        if camera_id not in cameras:
            raise ValueError(f"Invalid camera ID: {camera_id}")
        return cameras[camera_id].get_view_matrix_list()
    
    def fps_move_forward(camera_id: int, delta_time: float):
        if camera_id not in cameras or not isinstance(cameras[camera_id], FPSCamera):
            raise ValueError(f"Invalid FPS camera ID: {camera_id}")
        cameras[camera_id].move_forward(delta_time)
    
    def fps_move_backward(camera_id: int, delta_time: float):
        if camera_id not in cameras or not isinstance(cameras[camera_id], FPSCamera):
            raise ValueError(f"Invalid FPS camera ID: {camera_id}")
        cameras[camera_id].move_backward(delta_time)
    
    def fps_move_left(camera_id: int, delta_time: float):
        if camera_id not in cameras or not isinstance(cameras[camera_id], FPSCamera):
            raise ValueError(f"Invalid FPS camera ID: {camera_id}")
        cameras[camera_id].move_left(delta_time)
    
    def fps_move_right(camera_id: int, delta_time: float):
        if camera_id not in cameras or not isinstance(cameras[camera_id], FPSCamera):
            raise ValueError(f"Invalid FPS camera ID: {camera_id}")
        cameras[camera_id].move_right(delta_time)
    
    def fps_move_up(camera_id: int, delta_time: float):
        if camera_id not in cameras or not isinstance(cameras[camera_id], FPSCamera):
            raise ValueError(f"Invalid FPS camera ID: {camera_id}")
        cameras[camera_id].move_up(delta_time)
    
    def fps_move_down(camera_id: int, delta_time: float):
        if camera_id not in cameras or not isinstance(cameras[camera_id], FPSCamera):
            raise ValueError(f"Invalid FPS camera ID: {camera_id}")
        cameras[camera_id].move_down(delta_time)
    
    def fps_rotate(camera_id: int, yaw: float, pitch: float):
        if camera_id not in cameras or not isinstance(cameras[camera_id], FPSCamera):
            raise ValueError(f"Invalid FPS camera ID: {camera_id}")
        cameras[camera_id].rotate(yaw, pitch)
    
    def orbital_rotate(camera_id: int, azimuth: float, elevation: float):
        if camera_id not in cameras or not isinstance(cameras[camera_id], OrbitalCamera):
            raise ValueError(f"Invalid orbital camera ID: {camera_id}")
        cameras[camera_id].rotate(azimuth, elevation)
    
    def orbital_zoom(camera_id: int, amount: float):
        if camera_id not in cameras or not isinstance(cameras[camera_id], OrbitalCamera):
            raise ValueError(f"Invalid orbital camera ID: {camera_id}")
        cameras[camera_id].zoom(amount)
    
    def orbital_pan(camera_id: int, x: float, y: float):
        if camera_id not in cameras or not isinstance(cameras[camera_id], OrbitalCamera):
            raise ValueError(f"Invalid orbital camera ID: {camera_id}")
        cameras[camera_id].pan(x, y)
    
    # Register functions
    runtime.register_function("create_fps_camera", create_fps_camera)
    runtime.register_function("create_orbital_camera", create_orbital_camera)
    runtime.register_function("camera_get_view_matrix", camera_get_view_matrix)
    
    runtime.register_function("fps_move_forward", fps_move_forward)
    runtime.register_function("fps_move_backward", fps_move_backward)
    runtime.register_function("fps_move_left", fps_move_left)
    runtime.register_function("fps_move_right", fps_move_right)
    runtime.register_function("fps_move_up", fps_move_up)
    runtime.register_function("fps_move_down", fps_move_down)
    runtime.register_function("fps_rotate", fps_rotate)
    
    runtime.register_function("orbital_rotate", orbital_rotate)
    runtime.register_function("orbital_zoom", orbital_zoom)
    runtime.register_function("orbital_pan", orbital_pan)
