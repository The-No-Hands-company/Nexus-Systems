from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from app.tasks import generate_thumbnail_task, transcode_video_task, detect_scenes_task, generate_captions_task

router = APIRouter(prefix="/api/v1/video/ai", tags=["ai"])


class ThumbnailRequest(BaseModel):
    video_id: str


class TranscodeRequest(BaseModel):
    video_id: str
    target_resolution: str = "720p"


class CaptionsRequest(BaseModel):
    video_id: str
    language: str = "en"


class SceneDetectRequest(BaseModel):
    video_id: str


@router.post("/thumbnail")
async def generate_thumbnail(body: ThumbnailRequest):
    if not body.video_id:
        raise HTTPException(400, "video_id is required")
    task = generate_thumbnail_task.delay(body.video_id)
    return {"task_id": task.id, "status": "queued"}


@router.post("/transcode")
async def transcode(body: TranscodeRequest):
    if not body.video_id:
        raise HTTPException(400, "video_id is required")
    task = transcode_video_task.delay(body.video_id, body.target_resolution)
    return {"task_id": task.id, "status": "queued"}


@router.post("/captions")
async def generate_captions(body: CaptionsRequest):
    if not body.video_id:
        raise HTTPException(400, "video_id is required")
    task = generate_captions_task.delay(body.video_id, body.language)
    return {"task_id": task.id, "status": "queued"}


@router.post("/detect-scenes")
async def detect_scenes(body: SceneDetectRequest):
    if not body.video_id:
        raise HTTPException(400, "video_id is required")
    task = detect_scenes_task.delay(body.video_id)
    return {"task_id": task.id, "status": "queued"}


@router.get("/tasks/{task_id}")
async def get_task_status(task_id: str):
    from celery.result import AsyncResult
    from app.celery_app import celery_app
    result = AsyncResult(task_id, app=celery_app)
    return {"task_id": task_id, "status": result.status, "result": result.result if result.ready() else None}
