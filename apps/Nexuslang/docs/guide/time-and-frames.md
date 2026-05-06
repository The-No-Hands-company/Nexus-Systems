# Time and Frames

## Understanding Game Time

In NexusLang, time management is crucial for smooth gameplay and consistent behavior across different devices.

## Frame-Based Updates

### Every Frame
```
Every frame:
    Move player based on input
    Update animations
    Check for collisions
```

### Fixed Updates
```
Every physics frame:
    Update physics simulation
    Apply forces
    Resolve collisions
```

## Time-Based Actions

### Delays and Timers
```
When player takes damage:
    Make player invincible
    Wait for 2 seconds
    Make player vulnerable again

Create countdown timer:
    Start at 60 seconds
    Every second:
        Reduce time by 1
        Update display
    When reaches 0:
        End game
```

### Scheduled Events
```
Schedule "Spawn Enemy":
    Every 5 seconds:
        Create new enemy
        If enemy count > 10:
            Stop this schedule
```

## Time Management

### Game Speed Control
```
Create time manager:
    Set game speed to 1
    
    When slow motion activated:
        Set game speed to 0.5
        Wait for 3 seconds
        Return game speed to normal
```

### Pausing
```
When pause button pressed:
    Pause game time
    Show pause menu
    Keep UI active

When resume selected:
    Hide pause menu
    Resume game time
```

## Frame Rate Management

### Setting Frame Rate
```
Set target frame rate to 60
Enable vsync

If running on mobile:
    Set target frame rate to 30
```

### Frame Rate Independence
```
Create movement system:
    Every frame:
        Move objects based on:
            Their speed
            Multiplied by time since last frame
```

## Animation Timing

### Animation Control
```
Create animation controller:
    Play "walk" animation:
        At 30 frames per second
        Loop while moving
    
    Play "jump" animation:
        Once
        At normal speed
```

### Time-Based Effects
```
Create particle effect:
    Emit 10 particles per second
    Each particle lives for 2 seconds
    Fade out over last 0.5 seconds
```

## Best Practices

### 1. Performance Optimization
```
# Good: Time-based updates
Every 0.5 seconds:
    Update AI pathfinding
    Check for distant enemies

# Avoid: Heavy operations every frame
Every frame:
    Update only nearby objects
    Skip complex calculations for distant objects
```

### 2. Consistent Timing
```
Create movement:
    Set base speed to 5 units
    Every frame:
        Move based on:
            Base speed
            Times delta time
            Times game speed
```

### 3. Time Scale Effects
```
Create bullet time effect:
    Slow game time to 0.2
    Keep player at normal speed
    Fade vision to blue
    Wait for 5 seconds
    Return all to normal
```