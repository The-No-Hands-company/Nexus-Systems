# Behaviors

## Understanding Behaviors

Behaviors in NexusLang are reusable sets of actions that define how objects act and react in your game.

### Basic Behavior Structure
```
Create behavior "Walk":
    When right arrow pressed:
        Move right by 5 pixels
        Play walking animation
    When left arrow pressed:
        Move left by 5 pixels
        Play walking animation
```

## Types of Behaviors

### 1. Movement Behaviors
```
Create behavior "Platform Movement":
    When right arrow is pressed:
        Face right
        Move right at walk speed
        Play walk animation
    When space is pressed:
        If touching ground:
            Apply upward force
            Play jump sound
```

### 2. Combat Behaviors
```
Create behavior "Enemy Combat":
    When player is within attack range:
        Face player
        Play attack animation
        Deal damage to player
        Wait for 2 seconds
```

### 3. AI Behaviors
```
Create behavior "Guard Patrol":
    Loop forever:
        Move to point A
        Wait 2 seconds
        Move to point B
        Wait 2 seconds
        
    When sees player:
        Stop patrolling
        Chase player
```

## Applying Behaviors

### Adding to Objects
```
Create player:
    Add behavior "Platform Movement"
    Add behavior "Combat"
    
Create enemy:
    Add behavior "Guard Patrol"
    Add behavior "Enemy Combat"
```

### Customizing Behaviors
```
Create behavior "Bounce":
    Set bounce height to 5
    Set bounce speed to 2
    
    Every frame:
        Move up and down by bounce height
        at bounce speed

Add Bounce to coin:
    Set bounce height to 3
    Set bounce speed to 1
```

## Behavior Communication

### Events and Messages
```
Create behavior "Health System":
    When damaged:
        Reduce health
        Play hurt animation
        If health reaches 0:
            Trigger "death" event

Create behavior "Game Manager":
    When any object triggers "death":
        Play death effects
        Wait 2 seconds
        Restart level
```

### State Management
```
Create behavior "Power Up":
    When collected:
        Remember collection time
        Make player invincible
        Start glowing effect
        
    Every frame:
        If time since collection > 10 seconds:
            Remove invincibility
            Stop glowing effect
```

## Best Practices

### 1. Modular Design
- Keep behaviors focused on single responsibilities
- Make behaviors reusable
- Allow for customization

### 2. Performance Considerations
```
Create behavior "Enemy AI":
    Every 0.5 seconds:  # Not every frame
        Check for player
        Update path
    
    Every frame:
        Move along current path
```

### 3. Behavior Combinations
```
Create behavior "Complete Character":
    Combine:
        Movement behavior
        Combat behavior
        Animation behavior
        Sound behavior
```