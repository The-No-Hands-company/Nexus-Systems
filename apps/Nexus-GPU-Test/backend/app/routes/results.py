from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.models import TestJob
from app.deps import get_db

router = APIRouter(prefix="/api/v1/gpu-test/results", tags=["results"])


class ResultSubmit(BaseModel):
    job_id: str
    passed: bool
    mean_error: float
    max_error: float
    pixels_above_threshold: int
    total_pixels: int
    diff_image_key: str | None = None
    rendered_image_key: str | None = None
    duration_ms: int = 0
    worker_id: str = ""


@router.post("")
async def submit_result(body: ResultSubmit, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(TestJob).where(TestJob.id == body.job_id))
    job = result.scalar_one_or_none()
    if not job:
        raise HTTPException(404, "Job not found")
    job.status = "passed" if body.passed else "failed"
    job.worker_id = body.worker_id
    job.diff_image_key = body.diff_image_key
    job.rendered_image_key = body.rendered_image_key
    job.duration_ms = body.duration_ms
    job.result_metrics = {
        "mean_error": body.mean_error,
        "max_error": body.max_error,
        "pixels_above_threshold": body.pixels_above_threshold,
        "total_pixels": body.total_pixels,
    }
    await db.commit()
    return {"ok": True, "status": job.status}


@router.get("/{job_id}")
async def get_result(job_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(TestJob).where(TestJob.id == job_id))
    job = result.scalar_one_or_none()
    if not job:
        raise HTTPException(404, "Job not found")
    return {
        "job_id": job.id,
        "name": job.name,
        "status": job.status,
        "worker_id": job.worker_id,
        "metrics": job.result_metrics,
        "duration_ms": job.duration_ms,
        "diff_image_key": job.diff_image_key,
        "rendered_image_key": job.rendered_image_key,
    }
