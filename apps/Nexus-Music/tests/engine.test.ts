import { describe, expect, it } from "bun:test";
import { MusicEngine } from "../src/music-engine";

describe("Nexus Music Engine", () => {
  it("manages tracks, playlists and track ordering", () => {
    const engine = new MusicEngine(":memory:");
    const track = engine.addTrack("Sovereignty", "Nexus Wave", "Ecosystem Vol 1", 240, "synthwave", "/audio/sample.mp3");
    expect(track.id).toBeDefined();

    const playlist = engine.createPlaylist("Focus Beats", "Deep work background music");
    expect(playlist.id).toBeDefined();

    const item = engine.addToPlaylist(playlist.id, track.id, 1);
    expect(item.playlistId).toBe(playlist.id);

    const tracks = engine.getPlaylistTracks(playlist.id);
    expect(tracks.length).toBe(1);
    expect(tracks[0]?.title).toBe("Sovereignty");
  });
});
