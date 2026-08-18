use std::sync::Arc;
use std::time::Duration;

use nexus_mailstore::{FolderKind, MailStore};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpListener, TcpStream};
use uuid::Uuid;

use crate::session::{flag_list, folder_attributes, select_response, Action, Request, Session};

/// Verifies a username and password.
///
/// A trait so the IMAP server holds no opinion about where accounts live. In
/// this ecosystem they live in Nexus-Auth, and duplicating that check here
/// would create a second place for a password to be wrong.
pub trait Authenticator: Send + Sync + 'static {
    fn verify(
        &self,
        user: &str,
        password: &str,
    ) -> impl std::future::Future<Output = Option<String>> + Send;
}

/// IMAP sessions are long-lived by design — a client idles for minutes — so
/// this is generous compared with SMTP's.
const IDLE_TIMEOUT: Duration = Duration::from_secs(1800);

pub struct ImapServer<A: Authenticator> {
    pub hostname: String,
    pub store: MailStore,
    pub auth: Arc<A>,
}

/// The mailbox and folder a connection currently has open.
struct Open {
    mailbox_id: Uuid,
    folder_id: Uuid,
}

impl<A: Authenticator> ImapServer<A> {
    pub async fn serve(self: Arc<Self>, listener: TcpListener) -> std::io::Result<()> {
        loop {
            let (stream, peer) = listener.accept().await?;
            let me = Arc::clone(&self);
            tokio::spawn(async move {
                if let Err(e) = me.handle(stream).await {
                    tracing::debug!(%peer, error = %e, "imap connection ended");
                }
            });
        }
    }

    async fn handle(&self, stream: TcpStream) -> std::io::Result<()> {
        let (read_half, mut write) = stream.into_split();
        let mut reader = BufReader::new(read_half);
        let mut session = Session::new(self.hostname.clone());
        let mut mailbox: Option<Uuid> = None;
        let mut open: Option<Open> = None;

        write.write_all(session.greeting().as_bytes()).await?;

        let mut line = String::new();
        loop {
            line.clear();
            let read =
                tokio::time::timeout(IDLE_TIMEOUT, reader.read_line(&mut line)).await;
            let n = match read {
                Ok(Ok(n)) => n,
                Ok(Err(e)) => return Err(e),
                Err(_) => {
                    let _ = write.write_all(b"* BYE Idle timeout\r\n").await;
                    return Ok(());
                }
            };
            if n == 0 {
                return Ok(());
            }

            match session.line(&line) {
                Action::Reply(r) => write.write_all(r.as_bytes()).await?,
                Action::ReplyAndClose(r) => {
                    write.write_all(r.as_bytes()).await?;
                    return Ok(());
                }
                Action::Need(req) => {
                    let reply = self
                        .fulfil(req, &mut session, &mut mailbox, &mut open)
                        .await;
                    write.write_all(reply.as_bytes()).await?;
                }
            }
        }
    }

    async fn fulfil(
        &self,
        req: Request,
        session: &mut Session,
        mailbox: &mut Option<Uuid>,
        open: &mut Option<Open>,
    ) -> String {
        match req {
            Request::Authenticate { tag, user, password } => {
                match self.auth.verify(&user, &password).await {
                    Some(subject) => match self.store.mailbox_for_subject(&subject).await {
                        Ok(Some(id)) => {
                            *mailbox = Some(id);
                            session.mark_authenticated();
                            format!("{tag} OK LOGIN completed\r\n")
                        }
                        // Correct credentials but no mailbox is a different
                        // problem from a wrong password, and saying so saves
                        // someone a long argument with their client.
                        _ => format!("{tag} NO Account has no mailbox on this node\r\n"),
                    },
                    // Deliberately identical for an unknown user and a wrong
                    // password: distinguishing them turns this into an account
                    // enumeration oracle.
                    None => format!("{tag} NO Invalid credentials\r\n"),
                }
            }

            Request::ListFolders { tag } => {
                let Some(id) = *mailbox else {
                    return format!("{tag} NO Not authenticated\r\n");
                };
                match self.store.folders(id).await {
                    Ok(folders) => {
                        let mut out = String::new();
                        for f in folders {
                            out.push_str(&format!(
                                "* LIST ({}) \"/\" \"{}\"\r\n",
                                folder_attributes(f.kind),
                                f.name
                            ));
                        }
                        out.push_str(&format!("{tag} OK LIST completed\r\n"));
                        out
                    }
                    Err(e) => format!("{tag} NO {e}\r\n"),
                }
            }

            Request::OpenFolder { tag, name, read_only } => {
                let Some(id) = *mailbox else {
                    return format!("{tag} NO Not authenticated\r\n");
                };
                let folders = match self.store.folders(id).await {
                    Ok(f) => f,
                    Err(e) => return format!("{tag} NO {e}\r\n"),
                };
                // INBOX is case-insensitive per RFC 3501; every other name is
                // matched as given.
                let found = folders.iter().find(|f| {
                    f.name == name
                        || (name.eq_ignore_ascii_case("INBOX") && f.kind == FolderKind::Inbox)
                });
                let Some(folder) = found else {
                    return format!("{tag} NO [NONEXISTENT] No such mailbox\r\n");
                };

                let messages = match self.store.list_by_uid(id, folder.id).await {
                    Ok(m) => m,
                    Err(e) => return format!("{tag} NO {e}\r\n"),
                };
                let (uid_validity, uid_next) = match self.store.mailbox_state(id).await {
                    Ok(s) => s,
                    Err(e) => return format!("{tag} NO {e}\r\n"),
                };

                session.mark_selected(messages.iter().map(|m| m.uid).collect());
                *open = Some(Open { mailbox_id: id, folder_id: folder.id });
                select_response(&tag, &messages, uid_validity, uid_next, read_only)
            }

            Request::Fetch { tag, uids, items } => {
                let Some(o) = open.as_ref() else {
                    return format!("{tag} BAD No mailbox selected\r\n");
                };
                let messages = match self.store.list_by_uid(o.mailbox_id, o.folder_id).await {
                    Ok(m) => m,
                    Err(e) => return format!("{tag} NO {e}\r\n"),
                };

                // Careful: "RFC822.SIZE" contains "RFC822", and "BODYSTRUCTURE"
                // contains "BODY". Matching loosely made a header-only listing
                // download every message in full — the exact request a client
                // makes to *avoid* doing that, turned into the worst case.
                let upper = items.to_uppercase();
                let want_body = upper.contains("BODY[")
                    || upper.contains("BODY.PEEK[")
                    || upper.contains("RFC822.TEXT")
                    || upper
                        .split(|c: char| !c.is_ascii_alphanumeric() && c != '.')
                        .any(|item| item == "RFC822");
                let mut out = String::new();

                for (seq, m) in messages.iter().enumerate() {
                    if !uids.contains(&m.uid) {
                        continue;
                    }
                    let mut parts = vec![format!("UID {}", m.uid)];
                    parts.push(format!("FLAGS ({})", flag_list(m)));
                    parts.push(format!("RFC822.SIZE {}", m.size));
                    parts.push(format!(
                        "INTERNALDATE \"{}\"",
                        m.received_at.format("%d-%b-%Y %H:%M:%S %z")
                    ));

                    if want_body {
                        match self.store.raw_by_uid(o.mailbox_id, m.uid).await {
                            Ok(raw) => {
                                // A literal: the client is told the byte count
                                // and then reads exactly that many, which is the
                                // only safe way to send a message that may
                                // contain anything at all.
                                parts.push(format!(
                                    "BODY[] {{{}}}\r\n{}",
                                    raw.len(),
                                    String::from_utf8_lossy(&raw)
                                ));
                            }
                            Err(_) => parts.push("BODY[] NIL".into()),
                        }
                    }

                    out.push_str(&format!("* {} FETCH ({})\r\n", seq + 1, parts.join(" ")));
                }
                out.push_str(&format!("{tag} OK UID FETCH completed\r\n"));
                out
            }

            Request::Store { tag, uids, action, flags } => {
                let Some(o) = open.as_ref() else {
                    return format!("{tag} BAD No mailbox selected\r\n");
                };
                let adding = action.starts_with('+');
                let removing = action.starts_with('-');
                let upper = flags.to_uppercase();

                for uid in &uids {
                    for (imap_name, column) in [
                        ("\\SEEN", "seen"),
                        ("\\ANSWERED", "answered"),
                        ("\\FLAGGED", "flagged"),
                        ("\\DRAFT", "draft"),
                        ("\\DELETED", "deleted"),
                    ] {
                        if !upper.contains(imap_name) {
                            continue;
                        }
                        // Bare FLAGS (no + or -) replaces the set; with only
                        // the named flag present, that means setting it — so
                        // anything other than an explicit removal sets.
                        let _ = adding;
                        let value = !removing;
                        let _ = self
                            .store
                            .set_flag_by_uid(o.mailbox_id, *uid, column, value)
                            .await;
                    }
                }
                format!("{tag} OK UID STORE completed\r\n")
            }

            Request::Search { tag, criteria } => {
                let Some(o) = open.as_ref() else {
                    return format!("{tag} BAD No mailbox selected\r\n");
                };
                let messages = match self.store.list_by_uid(o.mailbox_id, o.folder_id).await {
                    Ok(m) => m,
                    Err(e) => return format!("{tag} NO {e}\r\n"),
                };
                let upper = criteria.to_uppercase();
                // Only the criteria a client actually leans on at sync time.
                // An unrecognised search returns everything rather than
                // nothing: showing too much is recoverable, hiding mail is not.
                let hits: Vec<String> = messages
                    .iter()
                    .filter(|m| {
                        if upper.contains("UNSEEN") {
                            !m.flags.seen
                        } else if upper.contains("SEEN") {
                            m.flags.seen
                        } else if upper.contains("DELETED") {
                            m.flags.deleted
                        } else {
                            true
                        }
                    })
                    .map(|m| m.uid.to_string())
                    .collect();

                format!(
                    "* SEARCH {}\r\n{tag} OK UID SEARCH completed\r\n",
                    hits.join(" ")
                )
            }
        }
    }
}
