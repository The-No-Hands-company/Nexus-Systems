"""
Scene Graph module for NexusLang - Hierarchical object management
Provides parent-child relationships, transform inheritance, and multi-object rendering
"""

from typing import Dict, List, Optional
import sys
import os

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from nexuslang.stdlib.math3d import Matrix4, Vector3, Quaternion


class SceneNode:
    """A node in the scene graph with local and world transforms"""
    
    def __init__(self, name: str = "Node"):
        self.name = name
        self.parent: Optional['SceneNode'] = None
        self.children: List['SceneNode'] = []
        
        # Local transform (relative to parent)
        self.position = Vector3(0.0, 0.0, 0.0)
        self.rotation = Quaternion(0.0, 0.0, 0.0, 1.0)
        self.scale = Vector3(1.0, 1.0, 1.0)
        
        # Cached world transform (updated when dirty)
        self._world_matrix: Optional[Matrix4] = None
        self._dirty = True
        
        # Optional rendering data
        self.vao_id: Optional[int] = None
        self.vertex_count: Optional[int] = None
        self.index_count: Optional[int] = None
        self.texture_id: Optional[int] = None
        self.shader_id: Optional[int] = None
    
    def set_parent(self, parent: Optional['SceneNode']):
        """Set parent node and mark transform as dirty"""
        # Remove from old parent
        if self.parent is not None:
            self.parent.children.remove(self)
        
        # Set new parent
        self.parent = parent
        if parent is not None:
            parent.children.append(self)
        
        self.mark_dirty()
    
    def mark_dirty(self):
        """Mark this node and all children as needing transform update"""
        self._dirty = True
        for child in self.children:
            child.mark_dirty()
    
    def get_local_matrix(self) -> Matrix4:
        """Calculate local transform matrix from position/rotation/scale"""
        translation = Matrix4.translation(self.position.x, self.position.y, self.position.z)
        rotation_mat = self.rotation.to_matrix4()
        scale_mat = Matrix4.scale(self.scale.x, self.scale.y, self.scale.z)
        
        # TRS order: Scale, then Rotate, then Translate
        return translation * (rotation_mat * scale_mat)
    
    def get_world_matrix(self) -> Matrix4:
        """Get world transform matrix (includes parent transforms)"""
        if not self._dirty and self._world_matrix is not None:
            return self._world_matrix
        
        local = self.get_local_matrix()
        
        if self.parent is None:
            self._world_matrix = local
        else:
            parent_world = self.parent.get_world_matrix()
            self._world_matrix = parent_world * local
        
        self._dirty = False
        return self._world_matrix
    
    def set_position(self, x: float, y: float, z: float):
        """Set local position"""
        self.position = Vector3(x, y, z)
        self.mark_dirty()
    
    def set_rotation_euler(self, x: float, y: float, z: float):
        """Set local rotation from Euler angles (radians)"""
        # Create quaternion from Euler angles (ZYX order)
        qx = Quaternion.from_axis_angle(Vector3(1, 0, 0), x)
        qy = Quaternion.from_axis_angle(Vector3(0, 1, 0), y)
        qz = Quaternion.from_axis_angle(Vector3(0, 0, 1), z)
        self.rotation = qz * (qy * qx)
        self.mark_dirty()
    
    def set_scale(self, x: float, y: float, z: float):
        """Set local scale"""
        self.scale = Vector3(x, y, z)
        self.mark_dirty()
    
    def set_scale_uniform(self, scale: float):
        """Set uniform scale"""
        self.scale = Vector3(scale, scale, scale)
        self.mark_dirty()
    
    def translate(self, x: float, y: float, z: float):
        """Translate by offset"""
        self.position = Vector3(
            self.position.x + x,
            self.position.y + y,
            self.position.z + z
        )
        self.mark_dirty()
    
    def rotate_euler(self, x: float, y: float, z: float):
        """Rotate by Euler angles (radians)"""
        qx = Quaternion.from_axis_angle(Vector3(1, 0, 0), x)
        qy = Quaternion.from_axis_angle(Vector3(0, 1, 0), y)
        qz = Quaternion.from_axis_angle(Vector3(0, 0, 1), z)
        delta = qz * (qy * qx)
        self.rotation = self.rotation * delta
        self.mark_dirty()
    
    def set_rendering_data(self, vao_id: int, vertex_count: int = 0, index_count: int = 0,
                           texture_id: Optional[int] = None, shader_id: Optional[int] = None):
        """Attach rendering data to this node"""
        self.vao_id = vao_id
        self.vertex_count = vertex_count
        self.index_count = index_count
        self.texture_id = texture_id
        self.shader_id = shader_id


class SceneGraph:
    """Manages a collection of scene nodes"""
    
    def __init__(self):
        self.nodes: Dict[int, SceneNode] = {}
        self.next_node_id = 1
        self.root: SceneNode = SceneNode("Root")
        self.nodes[0] = self.root
    
    def create_node(self, name: str = "Node") -> int:
        """Create a new scene node and return its ID"""
        node = SceneNode(name)
        node_id = self.next_node_id
        self.nodes[node_id] = node
        self.next_node_id += 1
        return node_id
    
    def get_node(self, node_id: int) -> SceneNode:
        """Get scene node by ID"""
        if node_id not in self.nodes:
            raise ValueError(f"Invalid node ID: {node_id}")
        return self.nodes[node_id]
    
    def delete_node(self, node_id: int):
        """Delete a node and all its children"""
        if node_id not in self.nodes:
            return
        
        node = self.nodes[node_id]
        
        # Delete children first
        for child in list(node.children):
            child_id = self._find_node_id(child)
            if child_id is not None:
                self.delete_node(child_id)
        
        # Remove from parent
        if node.parent is not None:
            node.parent.children.remove(node)
        
        # Remove from graph
        del self.nodes[node_id]
    
    def _find_node_id(self, node: SceneNode) -> Optional[int]:
        """Find node ID by node reference"""
        for nid, n in self.nodes.items():
            if n is node:
                return nid
        return None
    
    def traverse(self, callback, node: Optional[SceneNode] = None):
        """Depth-first traversal of scene graph"""
        if node is None:
            node = self.root
        
        callback(node)
        for child in node.children:
            self.traverse(callback, child)
    
    def update_all(self):
        """Force update of all world matrices"""
        def update_node(node: SceneNode):
            node.get_world_matrix()
        
        self.traverse(update_node)


# Global scene graph instance
scene_graph: Optional[SceneGraph] = None


def register_scene_functions(runtime):
    """Register scene graph functions with NexusLang runtime"""
    global scene_graph
    scene_graph = SceneGraph()
    
    # Node creation and management
    def create_scene_node(name: str = "Node") -> int:
        """Create a new scene node"""
        return scene_graph.create_node(name)
    
    def delete_scene_node(node_id: int):
        """Delete a scene node"""
        scene_graph.delete_node(node_id)
    
    def set_node_parent(node_id: int, parent_id: int):
        """Set node's parent"""
        node = scene_graph.get_node(node_id)
        parent = scene_graph.get_node(parent_id) if parent_id >= 0 else None
        node.set_parent(parent)
    
    # Transform functions
    def set_node_position(node_id: int, x: float, y: float, z: float):
        """Set node local position"""
        node = scene_graph.get_node(node_id)
        node.set_position(x, y, z)
    
    def set_node_rotation(node_id: int, x: float, y: float, z: float):
        """Set node local rotation (Euler angles in radians)"""
        node = scene_graph.get_node(node_id)
        node.set_rotation_euler(x, y, z)
    
    def set_node_scale(node_id: int, x: float, y: float, z: float):
        """Set node local scale"""
        node = scene_graph.get_node(node_id)
        node.set_scale(x, y, z)
    
    def set_node_scale_uniform(node_id: int, scale: float):
        """Set node uniform scale"""
        node = scene_graph.get_node(node_id)
        node.set_scale_uniform(scale)
    
    def node_translate(node_id: int, x: float, y: float, z: float):
        """Translate node by offset"""
        node = scene_graph.get_node(node_id)
        node.translate(x, y, z)
    
    def node_rotate(node_id: int, x: float, y: float, z: float):
        """Rotate node by Euler angles"""
        node = scene_graph.get_node(node_id)
        node.rotate_euler(x, y, z)
    
    def get_node_world_matrix(node_id: int) -> List[float]:
        """Get node's world transform matrix as list"""
        node = scene_graph.get_node(node_id)
        matrix = node.get_world_matrix()
        return matrix.to_list()
    
    # Rendering data
    def set_node_rendering(node_id: int, vao_id: int, vertex_count: int, 
                          index_count: int = 0, texture_id: int = -1, shader_id: int = -1):
        """Attach rendering data to node"""
        node = scene_graph.get_node(node_id)
        tex = texture_id if texture_id >= 0 else None
        shd = shader_id if shader_id >= 0 else None
        node.set_rendering_data(vao_id, vertex_count, index_count, tex, shd)
    
    def update_scene_graph():
        """Update all node world matrices"""
        scene_graph.update_all()
    
    # Register functions
    runtime.register_function("create_scene_node", create_scene_node)
    runtime.register_function("delete_scene_node", delete_scene_node)
    runtime.register_function("set_node_parent", set_node_parent)
    
    runtime.register_function("set_node_position", set_node_position)
    runtime.register_function("set_node_rotation", set_node_rotation)
    runtime.register_function("set_node_scale", set_node_scale)
    runtime.register_function("set_node_scale_uniform", set_node_scale_uniform)
    runtime.register_function("node_translate", node_translate)
    runtime.register_function("node_rotate", node_rotate)
    runtime.register_function("get_node_world_matrix", get_node_world_matrix)
    
    runtime.register_function("set_node_rendering", set_node_rendering)
    runtime.register_function("update_scene_graph", update_scene_graph)
    
    return scene_graph
