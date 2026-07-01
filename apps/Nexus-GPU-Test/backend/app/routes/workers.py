import datetime
from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.models import GPUTestWorker
from app.deps import get_db

router = APIRouter(prefix="/api/v1/gpu-test/workers", tags=["workers"])


class WorkerRegister(BaseModel):
    id: str
    gpu_name: str
    driver_version: str = ""
    vulkan_version: str = ""
    memory_mb: int = 0
    capabilities: list[str] = []


@router.get("")
async def list_workers(db: AsyncSession = Depends(get_db)):
    result = await db.execute(
        select(GPUTestWorker).order_by(GPUTestWorker.last_heartbeat.desc().nullslast())
    )
    return result.scalars().all()


@router.post("/register")
async def register_worker(body: WorkerRegister, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(GPUTestWorker).where(GPUTestWorker.id == body.id))
    existing = result.scalar_one_or_none()
    if existing:
        existing.gpu_name = body.gpu_name
        existing.driver_version = body.driver_version
        existing.vulkan_version = body.vulkan_version
        existing.memory_mb = body.memory_mb
        existing.capabilities = body.capabilities
        existing.online = True
        existing.last_heartbeat = datetime.datetime.utcnow()
    else:
        worker = GPUTestWorker(
            id=body.id,
            gpu_name=body.gpu_name,
            driver_version=body.driver_version,
            vulkan_version=body.vulkan_version,
            memory_mb=body.memory_mb,
            capabilities=body.capabilities,
            online=True,
            last_heartbeat=datetime.datetime.utcnow(),
        )
        db.add(worker)
    await db.commit()
    return {"ok": True, "worker_id": body.id}


@router.post("/{worker_id}/heartbeat")
async def worker_heartbeat(worker_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(GPUTestWorker).where(GPUTestWorker.id == worker_id))
    worker = result.scalar_one_or_none()
    if not worker:
        raise HTTPException(404, "Worker not found")
    worker.last_heartbeat = datetime.datetime.utcnow()
    worker.online = True
    await db.commit()
    return {"ok": True}
