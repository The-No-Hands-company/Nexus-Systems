import { create } from "zustand";

export interface AlbumData {
  id: string;
  name: string;
  description: string;
  cover_url?: string;
  photo_count: number;
  created_at?: string;
}

export interface ExifData {
  make?: string;
  model?: string;
  lens?: string;
  iso?: number;
  fNumber?: number;
  exposureTime?: string;
  focalLength?: string;
  gps?: {
    latitude: number;
    longitude: number;
    altitude?: number;
  };
  capturedAt?: string;
}

export interface PhotoData {
  id: string;
  album_id: string | null;
  title: string;
  description?: string;
  url: string;
  thumbnail_url: string;
  width: number;
  height: number;
  file_size: number;
  content_type?: string;
  tags: string;
  is_favorite: boolean;
  exif_data: ExifData;
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
  createAlbum: (name: string, description?: string) => AlbumData;
  deleteAlbum: (id: string) => void;

  photos: PhotoData[];
  selectedPhotoIds: Set<string>;
  viewingPhoto: PhotoData | null;
  setPhotos: (photos: PhotoData[]) => void;
  selectPhoto: (id: string, multi?: boolean) => void;
  selectAll: () => void;
  clearSelection: () => void;
  setViewingPhoto: (photo: PhotoData | null) => void;
  toggleFavorite: (id: string) => void;
  deletePhotos: (ids: string[]) => void;
  batchMoveToAlbum: (albumId: string) => void;
  batchDownload: () => void;
  triggerThumbnailGen: (photoId: string) => void;

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

const INITIAL_ALBUMS: AlbumData[] = [
  {
    id: "album-landscapes",
    name: "Nordic Landscapes",
    description: "Fjords, mountains, and aurora captures",
    photo_count: 2,
    created_at: new Date(Date.now() - 86400000 * 5).toISOString(),
  },
  {
    id: "album-architecture",
    name: "Modern Architecture",
    description: "Brutalist and minimalist geometric structures",
    photo_count: 2,
    created_at: new Date(Date.now() - 86400000 * 2).toISOString(),
  },
];

const INITIAL_PHOTOS: PhotoData[] = [
  {
    id: "photo-aurora",
    album_id: "album-landscapes",
    title: "Tromsø Aurora Borealis",
    description: "Vibrant green aurora curtain over frozen fjord waters",
    url: "https://images.unsplash.com/photo-1508739773434-c26b3d09e071?auto=format&fit=crop&w=1600&q=80",
    thumbnail_url: "https://images.unsplash.com/photo-1508739773434-c26b3d09e071?auto=format&fit=crop&w=400&q=80",
    width: 6000,
    height: 4000,
    file_size: 14200000,
    content_type: "image/jpeg",
    tags: "aurora, norway, fjord, night",
    is_favorite: true,
    exif_data: {
      make: "Sony",
      model: "ILCE-7M4 (Alpha 7 IV)",
      lens: "FE 14mm F1.8 GM",
      iso: 3200,
      fNumber: 1.8,
      exposureTime: "8s",
      focalLength: "14mm",
      gps: { latitude: 69.6492, longitude: 18.9553, altitude: 42.0 },
      capturedAt: "2026-10-15T22:45:10Z",
    },
    created_at: new Date(Date.now() - 86400000 * 3).toISOString(),
  },
  {
    id: "photo-mountains",
    album_id: "album-landscapes",
    title: "Lofoten Reine Morning",
    description: "Golden sunrise reflecting off still arctic waters",
    url: "https://images.unsplash.com/photo-1517411032315-54ef2cb783bb?auto=format&fit=crop&w=1600&q=80",
    thumbnail_url: "https://images.unsplash.com/photo-1517411032315-54ef2cb783bb?auto=format&fit=crop&w=400&q=80",
    width: 5800,
    height: 3867,
    file_size: 11800000,
    content_type: "image/jpeg",
    tags: "lofoten, sunrise, arctic, mountains",
    is_favorite: true,
    exif_data: {
      make: "Hasselblad",
      model: "X2D 100C",
      lens: "XCD 38mm F2.5 V",
      iso: 64,
      fNumber: 8.0,
      exposureTime: "1/125s",
      focalLength: "38mm",
      gps: { latitude: 67.9333, longitude: 13.0833, altitude: 12.0 },
      capturedAt: "2026-10-18T06:12:00Z",
    },
    created_at: new Date(Date.now() - 86400000 * 2).toISOString(),
  },
  {
    id: "photo-concrete",
    album_id: "album-architecture",
    title: "Brutalist Pavilion Angle",
    description: "Raw concrete geometries and cast shadows",
    url: "https://images.unsplash.com/photo-1513694203232-719a280e022f?auto=format&fit=crop&w=1600&q=80",
    thumbnail_url: "https://images.unsplash.com/photo-1513694203232-719a280e022f?auto=format&fit=crop&w=400&q=80",
    width: 4896,
    height: 3264,
    file_size: 9400000,
    content_type: "image/jpeg",
    tags: "brutalism, concrete, architecture, shadow",
    is_favorite: false,
    exif_data: {
      make: "Leica",
      model: "Leica M11-P",
      lens: "Summilux-M 35mm f/1.4 ASPH",
      iso: 100,
      fNumber: 4.0,
      exposureTime: "1/500s",
      focalLength: "35mm",
      gps: { latitude: 52.52, longitude: 13.405, altitude: 35.0 },
      capturedAt: "2026-10-24T14:30:22Z",
    },
    created_at: new Date(Date.now() - 86400000 * 1).toISOString(),
  },
  {
    id: "photo-facade",
    album_id: "album-architecture",
    title: "Copenhagen Glass Facade",
    description: "Geometric reflection of blue sky across glass curtain wall",
    url: "https://images.unsplash.com/photo-1486406146926-c627a92ad1ab?auto=format&fit=crop&w=1600&q=80",
    thumbnail_url: "https://images.unsplash.com/photo-1486406146926-c627a92ad1ab?auto=format&fit=crop&w=400&q=80",
    width: 5472,
    height: 3648,
    file_size: 8900000,
    content_type: "image/jpeg",
    tags: "glass, skyscraper, facade, copenhagen",
    is_favorite: false,
    exif_data: {
      make: "Fujifilm",
      model: "GFX 100 II",
      lens: "GF 32-64mm F4 R LM WR",
      iso: 160,
      fNumber: 5.6,
      exposureTime: "1/640s",
      focalLength: "45mm",
      gps: { latitude: 55.6761, longitude: 12.5683, altitude: 8.0 },
      capturedAt: "2026-10-28T11:15:45Z",
    },
    created_at: new Date().toISOString(),
  },
];

export const useEditorStore = create<EditorStore>((set, get) => ({
  albums: INITIAL_ALBUMS,
  selectedAlbumId: null,
  setAlbums: (albums) => set({ albums }),
  setSelectedAlbumId: (id) => set({ selectedAlbumId: id }),

  createAlbum: (name: string, description = "") => {
    const newAlbum: AlbumData = {
      id: `album-${Date.now()}`,
      name: name.trim() || "Untitled Album",
      description: description.trim(),
      photo_count: 0,
      created_at: new Date().toISOString(),
    };
    set((s) => ({ albums: [...s.albums, newAlbum], selectedAlbumId: newAlbum.id }));
    fetch("/api/v1/photos/albums", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ name: newAlbum.name, description: newAlbum.description }),
    }).catch(() => {});
    return newAlbum;
  },

  deleteAlbum: (id: string) => {
    set((s) => ({
      albums: s.albums.filter((a) => a.id !== id),
      photos: s.photos.map((p) => (p.album_id === id ? { ...p, album_id: null } : p)),
      selectedAlbumId: s.selectedAlbumId === id ? null : s.selectedAlbumId,
    }));
    fetch(`/api/v1/photos/albums/${id}`, { method: "DELETE" }).catch(() => {});
  },

  photos: INITIAL_PHOTOS,
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

  selectAll: () =>
    set((s) => {
      const activePhotos = s.photos.filter((p) => {
        if (s.selectedAlbumId === "favorites") return p.is_favorite;
        if (s.selectedAlbumId) return p.album_id === s.selectedAlbumId;
        return true;
      });
      return { selectedPhotoIds: new Set(activePhotos.map((p) => p.id)) };
    }),

  clearSelection: () => set({ selectedPhotoIds: new Set() }),

  setViewingPhoto: (photo) => set({ viewingPhoto: photo }),

  toggleFavorite: (id) => {
    set((s) => ({
      photos: s.photos.map((p) => (p.id === id ? { ...p, is_favorite: !p.is_favorite } : p)),
      viewingPhoto: s.viewingPhoto?.id === id ? { ...s.viewingPhoto, is_favorite: !s.viewingPhoto.is_favorite } : s.viewingPhoto,
    }));
    fetch(`/api/v1/photos/${id}/favorite`, { method: "PATCH" }).catch(() => {});
  },

  deletePhotos: (ids) => {
    set((s) => {
      const remaining = s.photos.filter((p) => !ids.includes(p.id));
      const updatedAlbums = s.albums.map((a) => ({
        ...a,
        photo_count: remaining.filter((p) => p.album_id === a.id).length,
      }));
      return {
        photos: remaining,
        albums: updatedAlbums,
        selectedPhotoIds: new Set([...s.selectedPhotoIds].filter((id) => !ids.includes(id))),
        viewingPhoto: s.viewingPhoto && ids.includes(s.viewingPhoto.id) ? null : s.viewingPhoto,
      };
    });
    fetch("/api/v1/photos/batch/delete", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ ids }),
    }).catch(() => {});
  },

  batchMoveToAlbum: (albumId: string) => {
    const { selectedPhotoIds } = get();
    const ids = Array.from(selectedPhotoIds);
    if (ids.length === 0) return;

    set((s) => {
      const updatedPhotos = s.photos.map((p) => (ids.includes(p.id) ? { ...p, album_id: albumId } : p));
      const updatedAlbums = s.albums.map((a) => ({
        ...a,
        photo_count: updatedPhotos.filter((p) => p.album_id === a.id).length,
      }));
      return {
        photos: updatedPhotos,
        albums: updatedAlbums,
        selectedPhotoIds: new Set(),
      };
    });

    fetch("/api/v1/photos/batch/move", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ ids, albumId }),
    }).catch(() => {});
  },

  batchDownload: () => {
    const { selectedPhotoIds, photos } = get();
    const ids = Array.from(selectedPhotoIds);
    const selected = photos.filter((p) => ids.includes(p.id));
    if (selected.length === 0) return;

    const exportManifest = {
      generatedAt: new Date().toISOString(),
      itemCount: selected.length,
      photos: selected.map((p) => ({
        id: p.id,
        title: p.title,
        url: p.url,
        exif: p.exif_data,
      })),
    };

    const blob = new Blob([JSON.stringify(exportManifest, null, 2)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `nexus-photos-batch-${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
  },

  triggerThumbnailGen: (photoId: string) => {
    fetch(`/api/v1/photos/${photoId}/thumbnail`, { method: "POST" })
      .then((r) => r.json())
      .then((data: any) => {
        if (data.url) {
          set((s) => ({
            photos: s.photos.map((p) => (p.id === photoId ? { ...p, thumbnail_url: data.url } : p)),
            viewingPhoto: s.viewingPhoto?.id === photoId ? { ...s.viewingPhoto, thumbnail_url: data.url } : s.viewingPhoto,
          }));
        }
      })
      .catch(() => {});
  },

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
