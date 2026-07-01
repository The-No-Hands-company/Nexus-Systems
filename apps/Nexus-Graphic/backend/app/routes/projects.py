from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession
from app.models import Project, Layer
from app.deps import get_db

router = APIRouter(prefix="/api/v1/graphic", tags=["projects"])


class ProjectCreate(BaseModel):
    name: str
    description: str = ""
    project_type: str = "vector"
    width: int = 1920
    height: int = 1080


class LayerCreate(BaseModel):
    name: str = "Layer"
    layer_type: str = "shape"
    data: dict = {}


@router.get("/projects")
async def list_projects(db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Project).order_by(Project.updated_at.desc()))
    return result.scalars().all()


@router.post("/projects")
async def create_project(body: ProjectCreate, db: AsyncSession = Depends(get_db)):
    project = Project(
        name=body.name, description=body.description,
        project_type=body.project_type, width=body.width, height=body.height,
    )
    db.add(project)
    await db.commit()
    await db.refresh(project)
    return project


@router.get("/projects/{project_id}")
async def get_project(project_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Project).where(Project.id == project_id))
    project = result.scalar_one_or_none()
    if not project:
        raise HTTPException(404, "Project not found")
    return project


@router.delete("/projects/{project_id}")
async def delete_project(project_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Project).where(Project.id == project_id))
    project = result.scalar_one_or_none()
    if not project:
        raise HTTPException(404, "Project not found")
    await db.delete(project)
    await db.commit()
    return {"ok": True}


@router.get("/projects/{project_id}/layers")
async def list_layers(project_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(
        select(Layer).where(Layer.project_id == project_id).order_by(Layer.order)
    )
    return result.scalars().all()


@router.post("/projects/{project_id}/layers")
async def create_layer(project_id: str, body: LayerCreate, db: AsyncSession = Depends(get_db)):
    result = await db.execute(
        select(Layer).where(Layer.project_id == project_id).order_by(Layer.order.desc())
    )
    last = result.scalar()
    order = (last.order + 1) if last else 0
    layer = Layer(project_id=project_id, name=body.name, layer_type=body.layer_type, data=body.data, order=order)
    db.add(layer)
    await db.commit()
    await db.refresh(layer)
    return layer


@router.patch("/projects/{project_id}/layers/{layer_id}")
async def update_layer(project_id: str, layer_id: str, body: dict, db: AsyncSession = Depends(get_db)):
    result = await db.execute(
        select(Layer).where(Layer.id == layer_id, Layer.project_id == project_id)
    )
    layer = result.scalar_one_or_none()
    if not layer:
        raise HTTPException(404, "Layer not found")
    for key, val in body.items():
        if hasattr(layer, key):
            setattr(layer, key, val)
    await db.commit()
    return layer
