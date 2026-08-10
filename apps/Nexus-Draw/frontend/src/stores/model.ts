export type StyleMode = "clean" | "sketch";
export type ElementType = "rectangle"|"ellipse"|"line"|"arrow"|"freehand"|"text"|"sticky"|"image";
export interface ElementStyle {
  stroke:string; fill:string; strokeWidth:number;
  strokeStyle:"solid"|"dashed"|"dotted"; opacity:number; radius:number;
  fontFamily:string; fontSize:number; textAlign:"left"|"center"|"right"; styleMode?:StyleMode;
}
export interface ElementData {
  id:string; elementType:ElementType; data:Record<string,any>;
  style:ElementStyle; transform:{a:number;b:number;c:number;d:number;e:number;f:number}; order:number; seed:number;
}
const DEFAULT_STYLE: ElementStyle = {
  stroke:"#e4e4e7", fill:"none", strokeWidth:2, strokeStyle:"solid",
  opacity:1, radius:8, fontFamily:"ui-sans-serif, system-ui", fontSize:20, textAlign:"left",
};
export function makeElement(type:ElementType, data:Record<string,any>, style:Partial<ElementStyle>={}):ElementData {
  return {
    id: crypto.randomUUID(), elementType:type, data,
    style: { ...DEFAULT_STYLE, ...style },
    transform: { a:1,b:0,c:0,d:1,e:0,f:0 }, order: 0,
    seed: Math.floor(Math.random()*2**31),
  };
}
export function resolveStyleMode(el:ElementData, boardDefault:StyleMode):StyleMode {
  return el.style.styleMode ?? boardDefault;
}
