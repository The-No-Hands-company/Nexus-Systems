from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    app_name: str = "Nexus-GPU-Test"
    database_url: str = "postgresql+asyncpg://nexus:nexus@localhost:5432/nexus_gpu_test"
    redis_url: str = "redis://localhost:6379/0"
    s3_endpoint: str = "http://localhost:9000"
    s3_access_key: str = "minioadmin"
    s3_secret_key: str = "minioadmin"
    s3_bucket: str = "nexus-gpu-test"
    cors_origins: str = "*"
    enable_cloud: bool = True

    model_config = {"env_prefix": "NEXUS_GPU_TEST_", "env_file": ".env"}


settings = Settings()
