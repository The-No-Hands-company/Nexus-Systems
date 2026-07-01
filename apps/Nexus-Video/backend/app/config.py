from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    database_url: str = "postgresql+asyncpg://nexus:nexus@localhost:5432/nexus_video"
    redis_url: str = "redis://localhost:6379/0"
    s3_endpoint: str = "http://localhost:9000"
    s3_access_key: str = "minioadmin"
    s3_secret_key: str = "minioadmin"
    s3_bucket: str = "nexus-video"
    asset_max_size: int = 500 * 1024 * 1024
    cloud_url: str = "http://localhost:8787"
    enable_cloud: bool = True
    enable_ai: bool = True
    cors_origins: str = "http://localhost:5173,http://localhost:3113"
    port: int = 3114

    model_config = {"env_prefix": "NEXUS_VIDEO_"}


settings = Settings()
