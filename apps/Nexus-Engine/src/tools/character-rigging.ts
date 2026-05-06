export interface BoneMap {
  hips?: string;
  spine?: string;
  head?: string;
  leftArm?: string;
  rightArm?: string;
  leftLeg?: string;
  rightLeg?: string;
}

export interface RigValidationResult {
  valid: boolean;
  warnings: string[];
  mappedBones: BoneMap;
}

/**
 * Helper for standardizing imported character rigs to an engine-friendly schema.
 * This is a preparation layer; full rig editing UI can sit on top of it later.
 */
export class CharacterRiggingAssistant {
  private readonly dictionary: Record<keyof BoneMap, string[]> = {
    hips: ["hips", "pelvis", "root", "mixamorig:Hips"],
    spine: ["spine", "chest", "mixamorig:Spine"],
    head: ["head", "neck", "mixamorig:Head"],
    leftArm: ["leftarm", "l_arm", "mixamorig:LeftArm"],
    rightArm: ["rightarm", "r_arm", "mixamorig:RightArm"],
    leftLeg: ["leftleg", "l_leg", "mixamorig:LeftLeg"],
    rightLeg: ["rightleg", "r_leg", "mixamorig:RightLeg"],
  };

  validateAndMap(boneNames: string[]): RigValidationResult {
    const lower = boneNames.map((name) => name.toLowerCase());
    const mapped: BoneMap = {};
    const warnings: string[] = [];

    (Object.keys(this.dictionary) as Array<keyof BoneMap>).forEach((slot) => {
      const aliases = this.dictionary[slot];
      const foundIndex = lower.findIndex((name) => aliases.some((alias) => name.includes(alias.toLowerCase())));
      if (foundIndex >= 0) {
        mapped[slot] = boneNames[foundIndex];
      } else {
        warnings.push(`Missing expected bone for slot: ${slot}`);
      }
    });

    return {
      valid: warnings.length <= 2,
      warnings,
      mappedBones: mapped,
    };
  }
}
