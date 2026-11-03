use std::{future::Future, path::PathBuf};

use nexus_mailstore::MailStore;
use sqlx::{postgres::PgPoolOptions, PgPool};
use uuid::Uuid;

struct Fixture {
    admin: PgPool,
    pool: PgPool,
    schema: String,
}

async fn create_fixture() -> Fixture {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let admin = PgPoolOptions::new()
        .max_connections(1)
        .connect(&url)
        .await
        .unwrap();
    let schema = format!("mail_test_{}", Uuid::new_v4().simple());
    sqlx::query(&format!("CREATE SCHEMA {schema}"))
        .execute(&admin)
        .await
        .unwrap();
    let search_path = schema.clone();
    let pool = PgPoolOptions::new()
        .max_connections(4)
        .after_connect(move |connection, _| {
            let search_path = search_path.clone();
            Box::pin(async move {
                sqlx::query("SELECT set_config('search_path', $1, false)")
                    .bind(search_path)
                    .execute(connection)
                    .await?;
                Ok(())
            })
        })
        .connect(&url)
        .await
        .unwrap();
    let migrations =
        PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../nexus-mailstore/migrations");
    let mut files: Vec<_> = std::fs::read_dir(migrations)
        .unwrap()
        .map(|entry| entry.unwrap().path())
        .filter(|path| path.extension().is_some_and(|ext| ext == "sql"))
        .collect();
    files.sort();
    let mut connection = pool.acquire().await.unwrap();
    for path in files {
        let migration = std::fs::read_to_string(path).unwrap();
        sqlx::raw_sql(&migration)
            .execute(&mut *connection)
            .await
            .unwrap();
    }
    drop(connection);
    Fixture {
        admin,
        pool,
        schema,
    }
}

async fn cleanup(fixture: Fixture) {
    fixture.pool.close().await;
    sqlx::query(&format!("DROP SCHEMA {} CASCADE", fixture.schema))
        .execute(&fixture.admin)
        .await
        .unwrap();
    fixture.admin.close().await;
}

fn resume_panic(result: Result<(), tokio::task::JoinError>) {
    if let Err(error) = result {
        if error.is_panic() {
            std::panic::resume_unwind(error.into_panic());
        }
        panic!("isolated test task cancelled: {error}");
    }
}

pub async fn with_store<F, Fut>(test: F)
where
    F: FnOnce(MailStore) -> Fut + Send + 'static,
    Fut: Future<Output = ()> + Send + 'static,
{
    let fixture = create_fixture().await;
    let result = tokio::spawn(test(MailStore::new(fixture.pool.clone()))).await;
    cleanup(fixture).await;
    resume_panic(result);
}

#[allow(dead_code)]
pub async fn with_two_stores<F, Fut>(test: F)
where
    F: FnOnce(MailStore, MailStore) -> Fut + Send + 'static,
    Fut: Future<Output = ()> + Send + 'static,
{
    let first = create_fixture().await;
    let second = create_fixture().await;
    let result = tokio::spawn(test(
        MailStore::new(first.pool.clone()),
        MailStore::new(second.pool.clone()),
    ))
    .await;
    cleanup(first).await;
    cleanup(second).await;
    resume_panic(result);
}
