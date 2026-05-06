# Game Objects

## Understanding Game Objects

Game objects are the basic building blocks in NexusLang. They represent anything that exists in your game world.

## Creating Objects

### Basic Objects
```
Create a sprite called "player":
    Set position to center
    Set size to 64 pixels
    Use image "player.png"

Create a text label:
    Set text to "Score: 0"
    Position at top right
    Set color to white
```

### Complex Objects
```
Create an enemy:
    # Visual setup
    Use model "enemy.fbx"
    Set scale to 1.5
    
    # Properties
    Set health to 100
    Set speed to 3
    
    # Behaviors
    Add patrol behavior
    Add combat behavior
```

## Object Properties

### Common Properties
```
Create game object:
    # Transform properties
    Set position to (100, 200)
    Set rotation to 45 degrees
    Set scale to 2

    # Visual properties
    Set color to red
    Set opacity to 0.8
    Set visible to yes

    # Physics properties
    Set solid to yes
    Enable gravity
    Set bounce to 0.5
```

### Custom Properties
```
Create player character:
    # Game-specific properties
    Set health to 100
    Set max speed to 5
    Set jump power to 10
    Set is invincible to no
```

## Object Hierarchy

### Parent-Child Relationships
```
Create character:
    Add weapon as child:
        Attach to right hand
        Move with parent
    
    Add shield as child:
        Attach to left hand
        Rotate with parent
```

### Groups and Collections
```
Create enemy group:
    Add 5 enemies:
        Space evenly
        Share behavior "group movement"
    
    When any enemy dies:
        Adjust group formation
```

## Object Interactions

### Collision Detection
```
Create player hitbox:
    Set size to player size
    When collides with enemy:
        Take damage
        Play hit effect
```

### Object Communication
```
Create power up:
    When collected by player:
        Tell player "power up"
        Tell score manager "add points"
        Remove self from game
```

## Object Lifecycle

### Creation and Destruction
```
When game starts:
    Create initial objects
    
When object health reaches 0:
    Play death animation
    Drop loot
    Remove from game
```

### Object Pooling
```
Create bullet pool:
    Prepare 20 bullets
    When bullet needed:
        Get from pool
    When bullet finished:
        Return to pool
```

## Best Practices

### 1. Organization
- Group related objects
- Use clear naming conventions
- Keep object hierarchies simple

### 2. Performance
```
# Good: Object pooling for frequent creation/destruction
Create particle system:
    Use pooling for particles
    Limit max particles to 100

# Good: Efficient updates
Every frame:
    Update only visible objects
    Update only active behaviors
```

### 3. Maintainability
```
Create enemy template:
    Define common properties
    Define basic behaviors
    
Create boss from enemy template:
    Add special abilities
    Increase stats
```