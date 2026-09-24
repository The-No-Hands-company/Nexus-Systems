//! nexus-mailctl, driven through the same argv an operator types.
//! Needs Postgres with the federation migration applied.

use nexus_mailfed::admin::{run, Context};
use nexus_mailfed::{NodeKey, PeerDirectory};
use nexus_mailstore::{Address, MailStore};
use sqlx::postgres::PgPoolOptions;
use uuid::Uuid;

async fn ctx() -> (Context, String) {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let pool = PgPoolOptions::new().max_connections(2).connect(&url).await.unwrap();
    let domain = format!("d{}.test", Uuid::now_v7().simple());
    let key_path = std::env::temp_dir().join(format!("mailctl-{}.key", Uuid::now_v7().simple()));
    (Context { pool, domain: domain.clone(), key_path }, domain)
}

async fn cmd(ctx: &Context, line: &str) -> Result<String, String> {
    let args: Vec<String> = line.split_whitespace().map(str::to_string).collect();
    run(ctx, &args).await
}

#[tokio::test]
async fn an_operator_can_create_a_role_mailbox_and_give_it_an_address() {
    let (ctx, domain) = ctx().await;
    let id = cmd(&ctx, "mailbox create --node Info").await.unwrap();
    let id: Uuid = id.trim().parse().expect("prints the new mailbox id");

    cmd(&ctx, &format!("address add {id} info@{domain} --primary")).await.unwrap();

    let store = MailStore::new(ctx.pool.clone());
    let resolved = store.resolve(&Address::parse(&format!("info@{domain}")).unwrap()).await.unwrap();
    assert_eq!(resolved, id);
}

#[tokio::test]
async fn an_identity_mailbox_belongs_to_its_subject() {
    let (ctx, _) = ctx().await;
    let subject = format!("user-{}", Uuid::now_v7().simple());
    let id: Uuid = cmd(&ctx, &format!("mailbox create --identity {subject} Eric"))
        .await
        .unwrap()
        .trim()
        .parse()
        .unwrap();
    let store = MailStore::new(ctx.pool.clone());
    assert_eq!(store.mailbox_for_subject(&subject).await.unwrap(), Some(id));
}

#[tokio::test]
async fn an_address_outside_this_nodes_domain_is_refused() {
    // Binding gmail.com to a local mailbox would make this node claim mail it
    // cannot receive and sign mail it has no right to send.
    let (ctx, _) = ctx().await;
    let id = cmd(&ctx, "mailbox create --node Info").await.unwrap();
    let err = cmd(&ctx, &format!("address add {} someone@gmail.com", id.trim())).await.unwrap_err();
    assert!(err.contains("domain"), "{err}");
}

#[tokio::test]
async fn peers_can_be_pinned_listed_and_removed() {
    let (ctx, _) = ctx().await;
    let peer = format!("p{}.test", Uuid::now_v7().simple());
    let key = NodeKey::generate().public_b64();

    cmd(&ctx, &format!("peer add {peer} https://mail.{peer} {key} --may-relay")).await.unwrap();
    let stored = PeerDirectory::new(ctx.pool.clone()).get(&peer).await.unwrap().unwrap();
    assert!(stored.may_relay);
    assert_eq!(stored.public_key, key);

    let listed = cmd(&ctx, "peer list").await.unwrap();
    assert!(listed.contains(&peer), "{listed}");

    cmd(&ctx, &format!("peer remove {peer}")).await.unwrap();
    assert!(PeerDirectory::new(ctx.pool.clone()).get(&peer).await.unwrap().is_none());
    assert!(cmd(&ctx, &format!("peer remove {peer}")).await.is_err(), "removing twice says so");
}

#[tokio::test]
async fn a_peer_with_a_malformed_key_is_refused_before_it_reaches_the_database() {
    let (ctx, _) = ctx().await;
    let peer = format!("p{}.test", Uuid::now_v7().simple());
    // 32 zero bytes: the length the schema accepts, but a small-order point
    // that would verify forged signatures if it were ever pinned.
    let bad = format!("{}=", "A".repeat(43));
    let err = cmd(&ctx, &format!("peer add {peer} https://mail.{peer} {bad}")).await.unwrap_err();
    assert!(err.contains("key"), "{err}");
    assert!(PeerDirectory::new(ctx.pool.clone()).get(&peer).await.unwrap().is_none());
}

#[tokio::test]
async fn key_show_prints_this_nodes_public_key() {
    let (ctx, _) = ctx().await;
    let first = cmd(&ctx, "key show").await.unwrap();
    let again = cmd(&ctx, "key show").await.unwrap();
    assert_eq!(first, again);
    assert_eq!(
        first.trim(),
        NodeKey::load_or_create(&ctx.key_path).unwrap().public_b64()
    );
    std::fs::remove_file(&ctx.key_path).unwrap();
}

#[tokio::test]
async fn nonsense_is_a_usage_error() {
    let (ctx, _) = ctx().await;
    for line in ["", "frobnicate", "peer add onlyonearg", "mailbox create"] {
        let err = cmd(&ctx, line).await.unwrap_err();
        assert!(err.contains("usage"), "{line:?}: {err}");
    }
}
