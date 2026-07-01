import uuid
from datetime import datetime, timezone
from sqlalchemy import Column, String, Integer, Float, Text, Boolean, DateTime, ForeignKey, JSON
from sqlalchemy.orm import DeclarativeBase, relationship


class Base(DeclarativeBase):
    pass


def utcnow() -> datetime:
    return datetime.now(timezone.utc)


def new_id() -> str:
    return uuid.uuid4().hex


class Channel(Base):
    __tablename__ = "channels"

    id = Column(String(32), primary_key=True, default=new_id)
    name = Column(String(255), nullable=False)
    description = Column(Text, default="")
    avatar_url = Column(String(512), default="")
    banner_url = Column(String(512), default="")
    subscriber_count = Column(Integer, default=0)
    video_count = Column(Integer, default=0)
    is_verified = Column(Boolean, default=False)
    created_at = Column(DateTime, default=utcnow)

    videos = relationship("Video", back_populates="channel", cascade="all, delete-orphan")
    playlists = relationship("Playlist", back_populates="channel", cascade="all, delete-orphan")


class Video(Base):
    __tablename__ = "videos"

    id = Column(String(32), primary_key=True, default=new_id)
    channel_id = Column(String(32), ForeignKey("channels.id"), nullable=False)
    title = Column(String(512), nullable=False)
    description = Column(Text, default="")
    url = Column(String(1024), default="")
    thumbnail_url = Column(String(512), default="")
    duration = Column(Float, default=0.0)
    width = Column(Integer, default=0)
    height = Column(Integer, default=0)
    file_size = Column(Integer, default=0)
    content_type = Column(String(128), default="video/mp4")
    tags = Column(String(1024), default="")
    views = Column(Integer, default=0)
    likes = Column(Integer, default=0)
    is_published = Column(Boolean, default=False)
    status = Column(String(32), default="uploading")
    created_at = Column(DateTime, default=utcnow)

    channel = relationship("Channel", back_populates="videos")


class Playlist(Base):
    __tablename__ = "playlists"

    id = Column(String(32), primary_key=True, default=new_id)
    name = Column(String(255), nullable=False)
    description = Column(Text, default="")
    channel_id = Column(String(32), ForeignKey("channels.id"), nullable=False)
    video_ids = Column(JSON, default=list)
    is_public = Column(Boolean, default=True)
    created_at = Column(DateTime, default=utcnow)

    channel = relationship("Channel", back_populates="playlists")


class Asset(Base):
    __tablename__ = "assets"

    id = Column(String(32), primary_key=True, default=new_id)
    filename = Column(String(255), nullable=False)
    content_type = Column(String(128), default="application/octet-stream")
    size = Column(Integer, default=0)
    storage_key = Column(String(512), nullable=False)
    created_at = Column(DateTime, default=utcnow)
