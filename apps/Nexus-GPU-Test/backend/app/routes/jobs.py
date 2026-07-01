from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.models import TestJob
from app.deps import get_db

router = APIRouter(prefix="/api/v1/gpu-test", tags=["jobs"])


class JobCreate(BaseModel):
    name: str
    shader_source: str | None = None
    reference_image_key: str | None = None
    mesh_type: str = "triangle"
    viewport_width: int = 256
    viewport_height: int = 256
    tolerance: float = 2.3


class JobStatus(BaseModel):
    id: str
    name: str
    status: str
    worker_id: str | None = None
    tolerance: float
    viewport_width: int
    viewport_height: int


@router.get("/jobs")
async def list_jobs(db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(TestJob).order_by(TestJob.created_at.desc()))
    return result.scalars().all()


@router.post("/jobs", status_code=201)
async def create_job(body: JobCreate, db: AsyncSession = Depends(get_db)):
    job = TestJob(
        name=body.name,
        shader_source=body.shader_source,
        reference_image_key=body.reference_image_key,
        mesh_type=body.mesh_type,
        viewport_width=body.viewport_width,
        viewport_height=body.viewport_height,
        tolerance=body.tolerance,
    )
    db.add(job)
    await db.commit()
    await db.refresh(job)
    return job


@router.get("/jobs/{job_id}")
async def get_job(job_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(TestJob).where(TestJob.id == job_id))
    job = result.scalar_one_or_none()
    if not job:
        raise HTTPException(404, "Job not found")
    return job


@router.delete("/jobs/{job_id}")
async def delete_job(job_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(TestJob).where(TestJob.id == job_id))
    job = result.scalar_one_or_none()
    if not job:
        raise HTTPException(404, "Job not found")
    await db.delete(job)
    await db.commit()
    return {"ok": True}
