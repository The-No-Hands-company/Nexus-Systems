//! `nexus-mailctl`: mailbox, address, peer and key administration.
//!
//! A CLI on the node rather than an HTTP API. Shell access to the node already
//! means operator, whereas an HTTP endpoint would need a role check the
//! Dashboard does not forward — and an address-binding endpoint without one
//! lets any signed-in user take `info@` for themselves.

use std::path::PathBuf;

use nexus_mailstore::{Address, MailStore};
use sqlx::PgPool;
use uuid::Uuid;

use crate::key::NodeKey;
use crate::peers::{Peer, PeerDirectory};

pub struct Context {
    pub pool: PgPool,
    /// The domain this node serves. Addresses may only be bound in it.
    pub domain: String,
    pub key_path: PathBuf,
}

pub const USAGE: &str = "usage:
  nexus-mailctl mailbox create --node <display name>
  nexus-mailctl mailbox create --identity <subject> <display name>
  nexus-mailctl address add <mailbox-id> <address> [--primary]
  nexus-mailctl peer add <domain> <base-url> <public-key> [--may-relay]
  nexus-mailctl peer list
  nexus-mailctl peer remove <domain>
  nexus-mailctl key show";

fn usage() -> String {
    USAGE.to_string()
}

/// Run one command. `Ok` is what to print; `Err` is the message to exit with.
pub async fn run(ctx: &Context, args: &[String]) -> Result<String, String> {
    let words: Vec<&str> = args.iter().map(String::as_str).collect();
    match words.as_slice() {
        ["mailbox", "create", "--node", name @ ..] if !name.is_empty() => {
            let mb = store(ctx).create_node_mailbox(&name.join(" ")).await.map_err(err)?;
            Ok(mb.id.to_string())
        }
        ["mailbox", "create", "--identity", subject, name @ ..] if !name.is_empty() => {
            let mb = store(ctx).create_identity_mailbox(subject, &name.join(" ")).await.map_err(err)?;
            Ok(mb.id.to_string())
        }
        ["address", "add", mailbox, address, rest @ ..] if rest.iter().all(|f| *f == "--primary") => {
            let mailbox: Uuid = mailbox.parse().map_err(|_| format!("{mailbox} is not a mailbox id"))?;
            let addr = Address::parse(address).map_err(|_| format!("{address} is not an address"))?;
            if addr.domain != ctx.domain.to_ascii_lowercase() {
                return Err(format!(
                    "{address} is not in this node's domain ({}); only addresses this node receives can be bound",
                    ctx.domain
                ));
            }
            store(ctx).add_address(mailbox, &addr, rest.contains(&"--primary")).await.map_err(err)?;
            Ok(format!("{} -> {mailbox}", addr.as_string()))
        }
        ["peer", "add", domain, base_url, public_key, rest @ ..]
            if rest.iter().all(|f| *f == "--may-relay") =>
        {
            // Checked here, not only by the schema's length check: a key that is
            // the right length but not a curve point would be pinned, and then
            // silently refuse every request from that peer.
            crate::signature::verify_key(public_key)
                .map_err(|_| format!("{public_key} is not an Ed25519 public key"))?;
            let peer = Peer {
                domain: domain.to_string(),
                base_url: base_url.to_string(),
                public_key: public_key.to_string(),
                may_relay: rest.contains(&"--may-relay"),
            };
            PeerDirectory::new(ctx.pool.clone()).add(&peer).await.map_err(err)?;
            Ok(format!(
                "pinned {domain} at {base_url}{} — restart nexus-mailsmtpd and nexus-mailapi to route to it",
                if peer.may_relay { " (may relay)" } else { "" }
            ))
        }
        ["peer", "list"] => {
            let peers = PeerDirectory::new(ctx.pool.clone()).list().await.map_err(err)?;
            if peers.is_empty() {
                return Ok("no peers pinned".into());
            }
            Ok(peers
                .iter()
                .map(|p| {
                    format!("{}\t{}\t{}\t{}", p.domain, p.base_url, p.public_key, if p.may_relay { "may-relay" } else { "-" })
                })
                .collect::<Vec<_>>()
                .join("\n"))
        }
        ["peer", "remove", domain] => {
            if PeerDirectory::new(ctx.pool.clone()).remove(domain).await.map_err(err)? {
                Ok(format!("removed {domain}"))
            } else {
                Err(format!("{domain} is not pinned"))
            }
        }
        // Creates the key if absent, so an operator can exchange keys with a
        // peer before the daemon's first start. The daemon then loads the same
        // file.
        ["key", "show"] => NodeKey::load_or_create(&ctx.key_path).map(|k| k.public_b64()).map_err(err),
        _ => Err(usage()),
    }
}

fn store(ctx: &Context) -> MailStore {
    MailStore::new(ctx.pool.clone())
}

fn err(e: impl std::fmt::Display) -> String {
    e.to_string()
}
