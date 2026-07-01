import { create } from "zustand";

export interface AlbumData {
  id: string;
  name: string;
  description: string;
  cover_url: string;
  photo_count: number;
  created_at: string;
}

export interface PhotoData {
  id: string;
  album_id: string | null;
  title: string;
  description: string;
  url: string;
  thumbnail_url: string;
  width: number;
  height: number;
  file_size: number;
  content_type: string;
  tags: string;
  is_favorite: boolean;
  exif_data: Record<string, unknown>;
  created_at: string;
}

export interface FilterState {
  brightness: number;
  contrast: number;
  saturation: number;
  blur: number;
  sharpen: number;
}

export interface CropTool {
  active: boolean;
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface Transform {
  rotation: number;
  flipH: boolean;
  flipV: boolean;
}

type ViewMode = "grid" | "detail" | "edit";

interface EditorStore {
  albums: AlbumData[];
  selectedAlbumId: string | null;
  setAlbums: (albums: AlbumData[]) => void;
  setSelectedAlbumId: (id: string | null) => void;

  photos: PhotoData[];
  selectedPhotoIds: Set<string>;
  viewingPhoto: PhotoData | null;
  setPhotos: (photos: PhotoData[]) => void;
  selectPhoto: (id: string, multi?: boolean) => void;
  setViewingPhoto: (photo: PhotoData | null) => void;
  toggleFavorite: (id: string) => void;
  deletePhotos: (ids: string[]) => void;

  viewMode: ViewMode;
  showSidebar: boolean;
  setViewMode: (mode: ViewMode) => void;
  setShowSidebar: (show: boolean) => void;

  filters: FilterState;
  setFilter: (key: keyof FilterState, value: number) => void;
  resetFilters: () => void;
  applyFilter: () => void;

  cropTool: CropTool;
  setCropTool: (crop: Partial<CropTool>) => void;
  transform: Transform;
  setTransform: (t: Partial<Transform>) => void;
}

const defaultFilters: FilterState = {
  brightness: 0,
  contrast: 0,
  saturation: 0,
  blur: 0,
  sharpen: 0,
};

export const useEditorStore = create<EditorStore>((set, get) => ({
  albums: [],
  selectedAlbumId: null,
  setAlbums: (albums) => set({ albums }),
  setSelectedAlbumId: (id) => set({ selectedAlbumId: id }),

  photos: [],
  selectedPhotoIds: new Set(),
  viewingPhoto: null,
  setPhotos: (photos) => set({ photos }),
  selectPhoto: (id, multi = false) =>
    set((s) => {
      const next = multi ? new Set(s.selectedPhotoIds) : new Set<string>();
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return { selectedPhotoIds: next };
    }),
  setViewingPhoto: (photo) => set({ viewingPhoto: photo }),
  toggleFavorite: (id) =>
    set((s) => ({
      photos: s.photos.map((p) => (p.id === id ? { ...p, is_favorite: !p.is_favorite } : p)),
    })),
  deletePhotos: (ids) =>
    set((s) => ({
      photos: s.photos.filter((p) => !ids.includes(p.id)),
      selectedPhotoIds: new Set([...s.selectedPhotoIds].filter((id) => !ids.includes(id))),
    })),

  viewMode: "grid",
  showSidebar: true,
  setViewMode: (mode) => set({ viewMode: mode }),
  setShowSidebar: (show) => set({ showSidebar: show }),

  filters: { ...defaultFilters },
  setFilter: (key, value) =>
    set((s) => ({
      filters: { ...s.filters, [key]: value },
    })),
  resetFilters: () => set({ filters: { ...defaultFilters } }),
  applyFilter: () => {},

  cropTool: { active: false, x: 0, y: 0, width: 0, height: 0 },
  setCropTool: (crop) =>
    set((s) => ({
      cropTool: { ...s.cropTool, ...crop },
    })),
  transform: { rotation: 0, flipH: false, flipV: false },
  setTransform: (t) =>
    set((s) => ({
      transform: { ...s.transform, ...t },
    })),
}));
