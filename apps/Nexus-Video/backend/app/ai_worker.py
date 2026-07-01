"""
AI worker — runs on GPU nodes.
For production: use FFmpeg, ComfyUI, Whisper, or custom ML pipelines.
This module provides the Python-side interface that Celery tasks call.
"""


def generate_thumbnail(video_id: str) -> str:
    """
    Extract a thumbnail frame from a video.
    In production, uses FFmpeg to grab a frame at a key moment.
    Returns the S3 URL of the thumbnail image.
    """
    raise NotImplementedError("Thumbnail generation requires FFmpeg on worker node")


def transcode_video(video_id: str, target_resolution: str = "720p") -> str:
    """
    Transcode video to target resolution/format.
    In production, uses FFmpeg with hardware acceleration (NVENC/VAAPI).
    Returns the S3 URL of the transcoded video.
    """
    raise NotImplementedError("Transcoding requires FFmpeg with hwaccel on worker node")


def detect_scenes(video_id: str) -> list[dict]:
    """
    Detect scene changes in a video using PySceneDetect or FFmpeg.
    Returns a list of scene timestamps {start, end}.
    """
    raise NotImplementedError("Scene detection requires PySceneDetect on worker node")


def generate_captions(video_id: str, language: str = "en") -> str:
    """
    Generate captions/subtitles using Whisper or similar ASR model.
    Returns the S3 URL of the generated VTT/SRT file.
    """
    raise NotImplementedError("Caption generation requires Whisper or ASR model on worker node")
