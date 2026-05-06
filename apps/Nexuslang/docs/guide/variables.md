# Variables and Objects

## Variables in NexusLang

Variables in NexusLang are created and used in a natural way, without traditional programming declarations.

### Creating Variables
```
Create score counter with value 0
Create player health set to 100
Create game speed with value 1.5
```

### Using Variables
```
Add 10 to score counter
Reduce player health by 25
Multiply game speed by 2
```

### Variable Types

#### Numbers
```
Create player speed set to 5
Create gravity strength set to 9.81
Create enemy count set to 3
```

#### Text
```
Create player name set to "Hero"
Create game title as "My Adventure"
Create message saying "Game Over"
```

#### True/False Values
```
Create can jump set to yes
Create is game over set to no
Create player is moving initially false
```

## Objects in NexusLang

### Creating Game Objects
```
Create a player:
    Set position to screen center
    Set size to 64 pixels
    Set speed to 5

Create an enemy called "Boss":
    Set health to 100
    Set damage to 25
    Set behavior to "aggressive"
```

### Object Properties
```
Create a button:
    # Visual properties
    Set color to blue
    Set size to 200 by 50 pixels
    Set text to "Start Game"

    # Behavior properties
    Set clickable to yes
    Set visible to yes
```

### Object Collections
```
Create three enemies:
    Set health to 50 each
    Set speed to 3 each

Create coin collection:
    Add 5 coins at random positions
    Make each coin spin
```

## Working with Properties

### Reading Properties
```
When player health is below 20:
    Show warning message

If enemy distance to player is less than 100:
    Make enemy chase player
```

### Modifying Properties
```
When player collects powerup:
    Double player speed
    Make player glow
    Set invincible to yes for 5 seconds
```

## Object Relationships

### Parent-Child Relationships
```
Create a weapon for player:
    Attach to player right hand
    Move with player
    When player attacks:
        Swing this weapon
```

### Object References
```
Create enemy AI:
    Remember closest player
    When closest player is nearby:
        Move toward that player
```

## Best Practices

### 1. Clear Naming
- Use descriptive names
- Be specific about purpose
- Use natural language conventions

### 2. Organization
```
Create game manager:
    # Game state variables
    Create score counter with value 0
    Create time remaining set to 60

    # Player references
    Remember current player
    Remember player spawn point
```

### 3. Scope Management
```
Create level:
    # Level-specific variables
    Create enemy count set to 0
    Create collectibles remaining set to 10

    # Level objects
    Create player spawn point here
    Create exit portal at end
```