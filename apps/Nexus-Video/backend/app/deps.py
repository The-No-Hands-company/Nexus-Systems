from sqlalchemy.ext.asyncio import create_async_engine, async_sessionmaker, AsyncSession
from app.config import settings

engine = create_async_engine(settings.database_url, echo=False)
async_session = async_sessionmaker(engine, expire_on_commit=False)


async def get_db():
    async with async_session() as session:
        yield session


class S3Storage:
    def __init__(self):
        import boto3
        self.client = boto3.client(
            "s3",
            endpoint_url=settings.s3_endpoint,
            aws_access_key_id=settings.s3_access_key,
            aws_secret_access_key=settings.s3_secret_key,
        )
        self.bucket = settings.s3_bucket
        self._ensure_bucket()

    def _ensure_bucket(self):
        try:
            self.client.head_bucket(Bucket=self.bucket)
        except Exception:
            self.client.create_bucket(Bucket=self.bucket)

    async def put(self, key: str, data: bytes, content_type: str):
        self.client.put_object(Bucket=self.bucket, Key=key, Body=data, ContentType=content_type)

    async def get(self, key: str) -> bytes:
        resp = self.client.get_object(Bucket=self.bucket, Key=key)
        return resp["Body"].read()


storage: S3Storage | None = None


def get_storage():
    global storage
    if storage is None:
        storage = S3Storage()
    return storage
