use nexus_mailstore::{FolderKind, ImapMessage};

use crate::command::{expand_set, parse, Command};

/// What the session wants the caller to do.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Action {
    /// Write this and keep going.
    Reply(String),
    /// Write this and close.
    ReplyAndClose(String),
    /// The session needs data. The server fulfils these because the state
    /// machine deliberately owns no database handle.
    Need(Request),
}

/// Something only the server can answer.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Request {
    Authenticate { tag: String, user: String, password: String },
    ListFolders { tag: String },
    OpenFolder { tag: String, name: String, read_only: bool },
    Fetch { tag: String, uids: Vec<i64>, items: String },
    Store { tag: String, uids: Vec<i64>, action: String, flags: String },
    Search { tag: String, criteria: String },
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum State {
    /// Before login. Almost nothing is permitted here, and that is the point:
    /// a server that answers questions before authentication leaks who exists.
    NotAuthenticated,
    Authenticated,
    Selected,
}

pub struct Session {
    state: State,
    hostname: String,
    /// UIDs currently in the selected folder, so a UID set can be expanded
    /// without asking the database what exists.
    uids: Vec<i64>,
}

impl Session {
    pub fn new(hostname: impl Into<String>) -> Self {
        Self { state: State::NotAuthenticated, hostname: hostname.into(), uids: Vec::new() }
    }

    pub fn greeting(&self) -> String {
        format!("* OK [CAPABILITY {}] {} Nexus IMAP ready\r\n", CAPABILITIES, self.hostname)
    }

    pub fn is_authenticated(&self) -> bool {
        self.state != State::NotAuthenticated
    }

    /// Called by the server once credentials check out.
    pub fn mark_authenticated(&mut self) {
        self.state = State::Authenticated;
    }

    /// Called by the server after a folder is opened, with what it found.
    pub fn mark_selected(&mut self, uids: Vec<i64>) {
        self.uids = uids;
        self.state = State::Selected;
    }

    pub fn selected_uids(&self) -> &[i64] {
        &self.uids
    }

    pub fn line(&mut self, raw: &str) -> Action {
        let Some(cmd) = parse(raw) else {
            return Action::Reply("* BAD Unparseable command\r\n".into());
        };
        let tag = cmd.tag.clone();

        match cmd.command {
            Command::Capability => Action::Reply(format!(
                "* CAPABILITY {CAPABILITIES}\r\n{tag} OK CAPABILITY completed\r\n"
            )),
            Command::Noop => Action::Reply(format!("{tag} OK NOOP completed\r\n")),
            Command::Logout => Action::ReplyAndClose(format!(
                "* BYE {} closing\r\n{tag} OK LOGOUT completed\r\n",
                self.hostname
            )),
            Command::Login { user, password } => {
                if self.state != State::NotAuthenticated {
                    return Action::Reply(format!("{tag} BAD Already authenticated\r\n"));
                }
                Action::Need(Request::Authenticate { tag, user, password })
            }

            // Everything below requires authentication. Answering any of them
            // to an anonymous client would disclose which mailboxes and
            // messages exist.
            _ if self.state == State::NotAuthenticated => {
                Action::Reply(format!("{tag} NO Authenticate first\r\n"))
            }

            Command::List { .. } => Action::Need(Request::ListFolders { tag }),
            Command::Select { mailbox } => {
                Action::Need(Request::OpenFolder { tag, name: mailbox, read_only: false })
            }
            Command::Examine { mailbox } => {
                Action::Need(Request::OpenFolder { tag, name: mailbox, read_only: true })
            }

            _ if self.state != State::Selected => {
                Action::Reply(format!("{tag} BAD Select a mailbox first\r\n"))
            }

            Command::UidFetch { set, items } => {
                Action::Need(Request::Fetch { tag, uids: expand_set(&set, &self.uids), items })
            }
            Command::UidStore { set, action, flags } => Action::Need(Request::Store {
                tag,
                uids: expand_set(&set, &self.uids),
                action,
                flags,
            }),
            Command::UidSearch { criteria } => Action::Need(Request::Search { tag, criteria }),
            Command::Close => {
                self.state = State::Authenticated;
                self.uids.clear();
                Action::Reply(format!("{tag} OK CLOSE completed\r\n"))
            }
            Command::Unknown(what) => {
                Action::Reply(format!("{tag} BAD Unsupported command {what}\r\n"))
            }
        }
    }
}

/// What this server admits to supporting.
///
/// LOGINDISABLED is deliberately absent because there is no TLS here yet and
/// advertising it would be a lie; the daemon is expected to run behind a
/// loopback bind or a TLS terminator, which the README says plainly.
pub const CAPABILITIES: &str = "IMAP4rev1 UIDPLUS";

/// The untagged response describing an opened folder.
///
/// UIDVALIDITY is the load-bearing one: a client caches by it, and if it ever
/// changes the client throws away everything it knew. Sending a value that
/// changes when it should not causes a full resync every connection.
pub fn select_response(
    tag: &str,
    messages: &[ImapMessage],
    uid_validity: i64,
    uid_next: i64,
    read_only: bool,
) -> String {
    let unseen = messages.iter().filter(|m| !m.flags.seen).count();
    let mut out = String::new();
    out.push_str(&format!("* {} EXISTS\r\n", messages.len()));
    // RECENT is a per-session notion we do not track; 0 is honest and clients
    // handle it, whereas inventing a number would be a lie they act on.
    out.push_str("* 0 RECENT\r\n");
    out.push_str(&format!("* OK [UIDVALIDITY {uid_validity}] UIDs valid\r\n"));
    out.push_str(&format!("* OK [UIDNEXT {uid_next}] Predicted next UID\r\n"));
    out.push_str("* FLAGS (\\Seen \\Answered \\Flagged \\Deleted \\Draft)\r\n");
    if unseen > 0 {
        if let Some(first) = messages.iter().position(|m| !m.flags.seen) {
            out.push_str(&format!("* OK [UNSEEN {}] First unseen\r\n", first + 1));
        }
    }
    let access = if read_only { "READ-ONLY" } else { "READ-WRITE" };
    out.push_str(&format!("{tag} OK [{access}] SELECT completed\r\n"));
    out
}

/// Render the flags of a message as IMAP names.
pub fn flag_list(m: &ImapMessage) -> String {
    let mut flags = Vec::new();
    if m.flags.seen {
        flags.push("\\Seen");
    }
    if m.flags.answered {
        flags.push("\\Answered");
    }
    if m.flags.flagged {
        flags.push("\\Flagged");
    }
    if m.flags.draft {
        flags.push("\\Draft");
    }
    if m.flags.deleted {
        flags.push("\\Deleted");
    }
    flags.join(" ")
}

/// The IMAP name and special-use attribute for a folder.
pub fn folder_attributes(kind: FolderKind) -> &'static str {
    match kind {
        FolderKind::Inbox => "\\HasNoChildren",
        FolderKind::Sent => "\\HasNoChildren \\Sent",
        FolderKind::Drafts => "\\HasNoChildren \\Drafts",
        FolderKind::Trash => "\\HasNoChildren \\Trash",
        FolderKind::Archive => "\\HasNoChildren \\Archive",
        FolderKind::Custom => "\\HasNoChildren",
    }
}
