/**
 * Core ARPG game types and interfaces
 */

export enum ItemRarity {
  Common = "common",
  Uncommon = "uncommon",
  Rare = "rare",
  Epic = "epic",
  Legendary = "legendary",
}

export enum ItemType {
  Weapon = "weapon",
  Armor = "armor",
  Accessory = "accessory",
  Potion = "potion",
  StackedGem = "gem",
}

export interface ItemStats {
  damage?: number;
  armor?: number;
  health?: number;
  mana?: number;
  dodge?: number;
  critChance?: number;
  fireResistance?: number;
  coldResistance?: number;
  lightningResistance?: number;
  [key: string]: number | undefined;
}

export interface Item {
  id: string;
  name: string;
  type: ItemType;
  rarity: ItemRarity;
  level: number;
  stats: ItemStats;
  description: string;
  icon?: string;
}

export interface CharacterStats {
  health: number;
  maxHealth: number;
  mana: number;
  maxMana: number;
  strength: number;
  dexterity: number;
  intelligence: number;
  vitality: number;
  attackSpeed: number;
  castSpeed: number;
  movementSpeed: number;
  armor: number;
  dodge: number;
  critChance: number;
  critMultiplier: number;
}

export interface Character {
  id: string;
  name: string;
  level: number;
  experience: number;
  stats: CharacterStats;
  inventory: Item[];
  equipment: {
    mainHand?: Item;
    offHand?: Item;
    chest?: Item;
    head?: Item;
    feet?: Item;
    ring1?: Item;
    ring2?: Item;
  };
  skills: Skill[];
  position: { x: number; y: number; z: number };
}

export interface Skill {
  id: string;
  name: string;
  description: string;
  cooldown: number;
  manaCost: number;
  damage?: number;
  range?: number;
  radius?: number;
  icon?: string;
}

export interface Enemy {
  id: string;
  name: string;
  level: number;
  stats: CharacterStats;
  position: { x: number; y: number; z: number };
  targetId?: string; // ID of target (usually player)
  skills: Skill[];
  lootTable: Item[];
}

export interface DungeonRoom {
  id: string;
  name: string;
  width: number;
  height: number;
  layout: number[][]; // 0 = empty, 1 = wall, 2 = door, etc.
  enemies: Enemy[];
  items: Item[];
  difficulty: number;
}

export interface Dungeon {
  id: string;
  name: string;
  depth: number;
  currentRoom: DungeonRoom;
  rooms: DungeonRoom[];
  difficulty: number;
  seed: number; // For procedural generation
}

export interface GameState {
  character: Character;
  currentDungeon: Dungeon | null;
  gameDifficulty: number; // 1 = normal, 2 = hard, 3 = nightmare, etc.
  playtime: number; // in seconds
  gold: number;
  achievements: string[];
}
