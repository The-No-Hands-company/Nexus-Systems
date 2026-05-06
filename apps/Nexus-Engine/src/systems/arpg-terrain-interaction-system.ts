import type { ARPGInputCommand } from "@systems/arpg-input-system";
import type { TerrainEditor } from "@tools/terrain-editor";

export interface TerrainInteractionResult {
  handled: boolean;
  statusMessage: string | null;
}

export function isTerrainInteractionCommand(command: ARPGInputCommand): boolean {
  return (
    command === "toggleTerrain" ||
    command === "setTerrainRaise" ||
    command === "setTerrainLower"
  );
}

export class ARPGTerrainInteractionSystem {
  handleCommand(
    command: ARPGInputCommand,
    terrainEditor: TerrainEditor | null
  ): TerrainInteractionResult {
    if (!terrainEditor || !isTerrainInteractionCommand(command)) {
      return { handled: false, statusMessage: null };
    }

    if (command === "toggleTerrain") {
      const enabled = terrainEditor.toggleEnabled();
      return {
        handled: true,
        statusMessage: enabled
          ? "Terrain editor enabled (drag mouse to sculpt)"
          : "Terrain editor disabled",
      };
    }

    if (command === "setTerrainRaise") {
      terrainEditor.setBrush({ mode: "raise" });
      return { handled: true, statusMessage: null };
    }

    terrainEditor.setBrush({ mode: "lower" });
    return { handled: true, statusMessage: null };
  }
}