//! Administer this node's mail: mailboxes, addresses, peers, and its key.
//! Reads the same settings as the daemons.

use nexus_mailfed::admin::{run, Context};
use sqlx::postgres::PgPoolOptions;

#[tokio::main]
async fn main() {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let setting = |name: &str| std::env::var(name).ok().filter(|v| !v.trim().is_empty());

    let Some(url) = setting("NEXUS_EMAIL_DATABASE_URL") else {
        eprintln!("NEXUS_EMAIL_DATABASE_URL must be set (see apps/Nexus-Email/.env)");
        std::process::exit(2);
    };
    let home = std::env::var("HOME").unwrap_or_else(|_| ".".into());
    let ctx = match PgPoolOptions::new().max_connections(1).connect(&url).await {
        Ok(pool) => Context {
            pool,
            domain: setting("NEXUS_EMAIL_DOMAIN").unwrap_or_else(|| "tnhc.dev".into()),
            key_path: setting("NEXUS_EMAIL_NODE_KEY_PATH")
                .unwrap_or_else(|| format!("{home}/.config/nexus-email/node.key"))
                .into(),
        },
        Err(e) => {
            eprintln!("cannot reach the mail database: {e}");
            std::process::exit(1);
        }
    };

    match run(&ctx, &args).await {
        Ok(out) => println!("{out}"),
        Err(e) => {
            eprintln!("{e}");
            std::process::exit(1);
        }
    }
}
