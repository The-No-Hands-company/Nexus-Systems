from app.celery_app import celery_app


@celery_app.task(bind=True, max_retries=3)
def generate_thumbnail_task(self, video_id: str):
    try:
        from app.ai_worker import generate_thumbnail
        result = generate_thumbnail(video_id)
        return {"status": "completed", "video_id": video_id, "thumbnail_url": result}
    except Exception as exc:
        raise self.retry(exc=exc, countdown=30)


@celery_app.task(bind=True, max_retries=3)
def transcode_video_task(self, video_id: str, target_resolution: str = "720p"):
    try:
        from app.ai_worker import transcode_video
        result = transcode_video(video_id, target_resolution)
        return {"status": "completed", "video_id": video_id, "url": result}
    except Exception as exc:
        raise self.retry(exc=exc, countdown=30)


@celery_app.task(bind=True, max_retries=3)
def detect_scenes_task(self, video_id: str):
    try:
        from app.ai_worker import detect_scenes
        result = detect_scenes(video_id)
        return {"status": "completed", "video_id": video_id, "scenes": result}
    except Exception as exc:
        raise self.retry(exc=exc, countdown=30)


@celery_app.task(bind=True, max_retries=3)
def generate_captions_task(self, video_id: str, language: str = "en"):
    try:
        from app.ai_worker import generate_captions
        result = generate_captions(video_id, language)
        return {"status": "completed", "video_id": video_id, "captions_url": result}
    except Exception as exc:
        raise self.retry(exc=exc, countdown=30)
