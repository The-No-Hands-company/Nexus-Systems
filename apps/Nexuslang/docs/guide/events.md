# Events and Triggers

## Understanding Events

Events in NexusLang are things that happen during your game that you want to respond to. They can be user actions, game states, or object interactions.

## Basic Event Structure

### Simple Events
```
When space key is pressed:
    Make player jump

When left mouse button clicked:
    Fire weapon
```

### Compound Events
```
When player health is below 20 and not healing:
    Show warning message
    Start health regeneration
```

## Types of Events

### 1. Input Events
```
When any key is pressed:
    Show pressed key

When mouse moves:
    Update cursor position

When controller button A pressed:
    Perform action
```

### 2. Collision Events
```
When player touches enemy:
    Take damage
    Play hit sound

When projectile hits wall:
    Create explosion
    Remove projectile
```

### 3. State Change Events
```
When score reaches 100:
    Unlock achievement
    Show celebration

When health becomes 0:
    Play death animation
    Show game over screen
```

## Custom Triggers

### Creating Triggers
```
Create trigger "Boss Defeated":
    When activated:
        Play victory music
        Open exit door
        Save progress
```

### Using Triggers
```
When boss health reaches 0:
    Activate trigger "Boss Defeated"
    
When player reaches checkpoint:
    Trigger "Save Game"
```

## Event Conditions

### State Checks
```
When player jumps:
    If has double jump power:
        Allow second jump
    If wearing heavy armor:
        Reduce jump height
```

### Multiple Conditions
```
When enemy spots player:
    If player is hiding:
        Ignore player
    If player is visible and within range:
        Start chase behavior
    Otherwise:
        Continue patrol
```

## Event Communication

### Broadcasting Events
```
Create event "Level Complete":
    Broadcast to all objects:
        Stop current actions
        Play victory animation
        
When last enemy defeated:
    Broadcast "Level Complete"
```

### Listening for Events
```
Listen for "Level Complete":
    When received:
        Update score
        Save progress
        Show next level button
```

## Best Practices

### 1. Event Organization
- Group related events together
- Use clear, descriptive event names
- Keep event handlers focused

### 2. Performance
```
# Good: Specific event
When player enters trigger zone:
    Activate cutscene

# Avoid: Checking every frame
Every frame:
    Check if player in trigger zone
    If true:
        Activate cutscene
```

### 3. Event Handling
```
When important event happens:
    Try:
        Handle the event
    If fails:
        Show error message
        Log the error
```