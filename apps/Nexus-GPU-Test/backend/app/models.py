import datetime
import uuid
from sqlalchemy import Column, String, Float, Integer, Boolean, DateTime, Text, ForeignKey, JSON
from sqlalchemy.orm import DeclarativeBase


class Base(DeclarativeBase):
    pass


class TestJob(Base):
    __tablename__ = "test_jobs"

    id = Column(String, primary_key=True, default=lambda: str(uuid.uuid4()))
    name = Column(String(255), nullable=False)
    shader_source = Column(Text, nullable=True)
    reference_image_key = Column(String, nullable=True)
    mesh_type = Column(String(64), default="triangle")
    viewport_width = Column(Integer, default=256)
    viewport_height = Column(Integer, default=256)
    tolerance = Column(Float, default=2.3)
    status = Column(String(32), default="pending")  # pending | running | passed | failed | error
    worker_id = Column(String, nullable=True)
    result_metrics = Column(JSON, nullable=True)
    diff_image_key = Column(String, nullable=True)
    rendered_image_key = Column(String, nullable=True)
    duration_ms = Column(Integer, nullable=True)
    created_at = Column(DateTime, default=datetime.datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.datetime.utcnow, onupdate=datetime.datetime.utcnow)


class GPUTestWorker(Base):
    __tablename__ = "gpu_test_workers"

    id = Column(String, primary_key=True)
    gpu_name = Column(String(255))
    driver_version = Column(String(64))
    vulkan_version = Column(String(64))
    memory_mb = Column(Integer)
    capabilities = Column(JSON, default=list)
    online = Column(Boolean, default=False)
    last_heartbeat = Column(DateTime, nullable=True)
    jobs_completed = Column(Integer, default=0)
    registered_at = Column(DateTime, default=datetime.datetime.utcnow)


class ReferenceImage(Base):
    __tablename__ = "reference_images"

    id = Column(String, primary_key=True, default=lambda: str(uuid.uuid4()))
    name = Column(String(255))
    storage_key = Column(String, nullable=False)
    width = Column(Integer)
    height = Column(Integer)
    test_job_id = Column(String, ForeignKey("test_jobs.id"), nullable=True)
    created_at = Column(DateTime, default=datetime.datetime.utcnow)
