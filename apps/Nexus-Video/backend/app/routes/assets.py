import hashlib
import hashlib
from fastapi import APIRouter, Depends, UploadFile, File, HTTPException
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.models import Asset
from app.deps import get_db, get_storage

router = APIRouter(prefix="/api/v1/video/assets", tags=["assets"])


@router.post("/upload")
async def upload_asset(
    file: UploadFile = File(...),
    db: AsyncSession = Depends(get_db),
    storage=Depends(get_storage),
):
    content = await file.read()
    content_hash = hashlib.sha256(content).hexdigest()
    ext = file.filename.split(".")[-1] if "." in (file.filename or "") else "bin"
    storage_key = f"assets/{content_hash[:4]}/{content_hash}.{ext}"

    await storage.put(storage_key, content, file.content_type or "application/octet-stream")

    asset = Asset(
        filename=file.filename or "untitled",
        content_type=file.content_type or "application/octet-stream",
        size=len(content),
        storage_key=storage_key,
    )
    db.add(asset)
    await db.commit()
    await db.refresh(asset)
    return {
        "id": asset.id,
        "filename": asset.filename,
        "content_type": asset.content_type,
        "size": asset.size,
        "storage_key": asset.storage_key,
    }


@router.post("/upload/chunk")
async def upload_chunk(
    filename: str,
    chunk_index: int,
    total_chunks: int,
    file: UploadFile = File(...),
    storage=Depends(get_storage),
):
    content = await file.read()
    temp_key = f"uploads/{hashlib.sha256(filename.encode()).hexdigest()}/{chunk_index}"
    await storage.put(temp_key, content, "application/octet-stream")
    return {"chunk_index": chunk_index, "total_chunks": total_chunks, "status": "received"}


@router.post("/upload/complete")
async def complete_upload(
    filename: str,
    total_chunks: int,
    content_type: str = "application/octet-stream",
    db: AsyncSession = Depends(get_db),
    storage=Depends(get_storage),
):
    base_key = hashlib.sha256(filename.encode()).hexdigest()
    combined = b""
    for i in range(total_chunks):
        temp_key = f"uploads/{base_key}/{i}"
        combined += await storage.get(temp_key)
    content_hash = hashlib.sha256(combined).hexdigest()
    ext = filename.split(".")[-1] if "." in filename else "bin"
    storage_key = f"assets/{content_hash[:4]}/{content_hash}.{ext}"
    await storage.put(storage_key, combined, content_type)
    asset = Asset(
        filename=filename,
        content_type=content_type,
        size=len(combined),
        storage_key=storage_key,
    )
    db.add(asset)
    await db.commit()
    await db.refresh(asset)
    return {
        "id": asset.id,
        "filename": asset.filename,
        "content_type": asset.content_type,
        "size": asset.size,
        "storage_key": asset.storage_key,
    }


@router.get("/{asset_id}")
async def get_asset(asset_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Asset).where(Asset.id == asset_id))
    asset = result.scalar_one_or_none()
    if not asset:
        raise HTTPException(404, "Asset not found")
    return asset
