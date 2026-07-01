from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlalchemy import select, func
from sqlalchemy.ext.asyncio import AsyncSession
from app.models import Channel, Video, Playlist
from app.deps import get_db

router = APIRouter(prefix="/api/v1/video", tags=["video"])


class ChannelCreate(BaseModel):
    name: str
    description: str = ""
    avatar_url: str = ""
    banner_url: str = ""


class ChannelUpdate(BaseModel):
    name: str | None = None
    description: str | None = None
    avatar_url: str | None = None
    banner_url: str | None = None


class VideoCreate(BaseModel):
    channel_id: str
    title: str
    description: str = ""
    url: str = ""
    thumbnail_url: str = ""
    duration: float = 0.0
    width: int = 0
    height: int = 0
    file_size: int = 0
    content_type: str = "video/mp4"
    tags: str = ""


class VideoUpdate(BaseModel):
    title: str | None = None
    description: str | None = None
    url: str | None = None
    thumbnail_url: str | None = None
    is_published: bool | None = None
    status: str | None = None


class PlaylistCreate(BaseModel):
    name: str
    description: str = ""
    channel_id: str
    is_public: bool = True


class PlaylistUpdate(BaseModel):
    name: str | None = None
    description: str | None = None
    is_public: bool | None = None


# ---- Channels ----

@router.get("/channels")
async def list_channels(db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Channel).order_by(Channel.name))
    return result.scalars().all()


@router.post("/channels")
async def create_channel(body: ChannelCreate, db: AsyncSession = Depends(get_db)):
    channel = Channel(**body.model_dump())
    db.add(channel)
    await db.commit()
    await db.refresh(channel)
    return channel


@router.get("/channels/{channel_id}")
async def get_channel(channel_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Channel).where(Channel.id == channel_id))
    channel = result.scalar_one_or_none()
    if not channel:
        raise HTTPException(404, "Channel not found")
    return channel


@router.put("/channels/{channel_id}")
async def update_channel(channel_id: str, body: ChannelUpdate, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Channel).where(Channel.id == channel_id))
    channel = result.scalar_one_or_none()
    if not channel:
        raise HTTPException(404, "Channel not found")
    for key, val in body.model_dump(exclude_unset=True).items():
        setattr(channel, key, val)
    await db.commit()
    await db.refresh(channel)
    return channel


@router.delete("/channels/{channel_id}")
async def delete_channel(channel_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Channel).where(Channel.id == channel_id))
    channel = result.scalar_one_or_none()
    if not channel:
        raise HTTPException(404, "Channel not found")
    await db.delete(channel)
    await db.commit()
    return {"status": "deleted"}


# ---- Videos ----

@router.get("/videos")
async def list_videos(
    channel_id: str | None = None,
    search: str | None = None,
    limit: int = 50,
    offset: int = 0,
    db: AsyncSession = Depends(get_db),
):
    query = select(Video).where(Video.is_published == True)
    if channel_id:
        query = query.where(Video.channel_id == channel_id)
    if search:
        query = query.where(Video.title.ilike(f"%{search}%"))
    query = query.order_by(Video.created_at.desc()).limit(limit).offset(offset)
    result = await db.execute(query)
    return result.scalars().all()


@router.post("/videos")
async def create_video(body: VideoCreate, db: AsyncSession = Depends(get_db)):
    video = Video(**body.model_dump())
    db.add(video)
    result = await db.execute(select(Channel).where(Channel.id == body.channel_id))
    channel = result.scalar_one_or_none()
    if channel:
        channel.video_count = (await db.execute(
            select(func.count()).select_from(Video).where(Video.channel_id == body.channel_id)
        )).scalar()
    await db.commit()
    await db.refresh(video)
    return video


@router.get("/videos/{video_id}")
async def get_video(video_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Video).where(Video.id == video_id))
    video = result.scalar_one_or_none()
    if not video:
        raise HTTPException(404, "Video not found")
    return video


@router.put("/videos/{video_id}")
async def update_video(video_id: str, body: VideoUpdate, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Video).where(Video.id == video_id))
    video = result.scalar_one_or_none()
    if not video:
        raise HTTPException(404, "Video not found")
    for key, val in body.model_dump(exclude_unset=True).items():
        setattr(video, key, val)
    await db.commit()
    await db.refresh(video)
    return video


@router.delete("/videos/{video_id}")
async def delete_video(video_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Video).where(Video.id == video_id))
    video = result.scalar_one_or_none()
    if not video:
        raise HTTPException(404, "Video not found")
    await db.delete(video)
    await db.commit()
    return {"status": "deleted"}


@router.post("/videos/{video_id}/like")
async def like_video(video_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Video).where(Video.id == video_id))
    video = result.scalar_one_or_none()
    if not video:
        raise HTTPException(404, "Video not found")
    video.likes += 1
    await db.commit()
    return {"likes": video.likes}


@router.post("/videos/{video_id}/view")
async def view_video(video_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Video).where(Video.id == video_id))
    video = result.scalar_one_or_none()
    if not video:
        raise HTTPException(404, "Video not found")
    video.views += 1
    await db.commit()
    return {"views": video.views}


# ---- Playlists ----

@router.get("/playlists")
async def list_playlists(channel_id: str | None = None, db: AsyncSession = Depends(get_db)):
    query = select(Playlist)
    if channel_id:
        query = query.where(Playlist.channel_id == channel_id)
    query = query.order_by(Playlist.created_at.desc())
    result = await db.execute(query)
    return result.scalars().all()


@router.post("/playlists")
async def create_playlist(body: PlaylistCreate, db: AsyncSession = Depends(get_db)):
    playlist = Playlist(**body.model_dump())
    db.add(playlist)
    await db.commit()
    await db.refresh(playlist)
    return playlist


@router.get("/playlists/{playlist_id}")
async def get_playlist(playlist_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Playlist).where(Playlist.id == playlist_id))
    playlist = result.scalar_one_or_none()
    if not playlist:
        raise HTTPException(404, "Playlist not found")
    return playlist


@router.put("/playlists/{playlist_id}")
async def update_playlist(playlist_id: str, body: PlaylistUpdate, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Playlist).where(Playlist.id == playlist_id))
    playlist = result.scalar_one_or_none()
    if not playlist:
        raise HTTPException(404, "Playlist not found")
    for key, val in body.model_dump(exclude_unset=True).items():
        setattr(playlist, key, val)
    await db.commit()
    await db.refresh(playlist)
    return playlist


@router.delete("/playlists/{playlist_id}")
async def delete_playlist(playlist_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Playlist).where(Playlist.id == playlist_id))
    playlist = result.scalar_one_or_none()
    if not playlist:
        raise HTTPException(404, "Playlist not found")
    await db.delete(playlist)
    await db.commit()
    return {"status": "deleted"}


@router.post("/playlists/{playlist_id}/videos")
async def add_video_to_playlist(playlist_id: str, video_id: str, db: AsyncSession = Depends(get_db)):
    result = await db.execute(select(Playlist).where(Playlist.id == playlist_id))
    playlist = result.scalar_one_or_none()
    if not playlist:
        raise HTTPException(404, "Playlist not found")
    vresult = await db.execute(select(Video).where(Video.id == video_id))
    video = vresult.scalar_one_or_none()
    if not video:
        raise HTTPException(404, "Video not found")
    ids = list(playlist.video_ids or [])
    if video_id not in ids:
        ids.append(video_id)
    playlist.video_ids = ids
    await db.commit()
    return playlist
