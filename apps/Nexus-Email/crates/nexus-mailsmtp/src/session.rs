use crate::command::{parse, Command};

/// Limits applied to every session. All of them exist because this port is
/// reachable by anyone.
#[derive(Debug, Clone, Copy)]
pub struct Limits {
    pub max_message_bytes: usize,
    pub max_recipients: usize,
    /// Bad commands tolerated before the connection is dropped. Without this a
    /// stranger can hold a connection open forever feeding garbage.
    pub max_errors: usize,
}

impl Default for Limits {
    fn default() -> Self {
        Self { max_message_bytes: 25 * 1024 * 1024, max_recipients: 100, max_errors: 10 }
    }
}

/// Why a recipient was accepted, which decides whether we are relaying.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Role {
    /// Port 25. Anonymous, and may only deliver to mailboxes we host.
    Mx,
    /// Port 587. Authenticated, and may send anywhere on the sender's behalf.
    Submission,
}

#[derive(Debug, Clone, PartialEq, Eq)]
enum Phase {
    /// Before EHLO.
    Greeted,
    /// After EHLO, no transaction open.
    Ready,
    /// MAIL FROM accepted.
    Mail,
    /// At least one RCPT accepted.
    Rcpt,
    /// Inside DATA, collecting the message.
    Data,
    /// QUIT seen; the connection should close.
    Closed,
}

/// What the caller should do after feeding a line in.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Action {
    /// Write this reply and keep going.
    Reply(String),
    /// Write this reply and close the connection.
    ReplyAndClose(String),
    /// A complete message was received.
    Deliver {
        from: String,
        recipients: Vec<String>,
        data: Vec<u8>,
        /// What the client called itself. Passed through because SPF is
        /// evaluated against it when the reverse path is empty.
        helo: String,
        reply: String,
    },
    /// Nothing to say; keep reading (a line of message data).
    Continue,
}

/// The decision about whether we will carry mail for a recipient.
///
/// Async because the honest answer lives in the database. Accepting mail for a
/// domain we host and then discovering the mailbox does not exist makes this
/// server a backscatter source: it would generate a bounce to a return path
/// the spammer forged, i.e. at a victim. Refusing at RCPT is the standard
/// answer, and it needs a real lookup.
pub trait RelayPolicy: Send + Sync {
    fn is_local(&self, address: &str) -> impl std::future::Future<Output = bool> + Send;
}

/// One SMTP conversation.
///
/// Deliberately a pure state machine over lines of text: no sockets, no
/// database, no clock. Every rule that decides whether a stranger may send
/// mail through this server is therefore testable exhaustively, which matters
/// more here than anywhere else in the system — an open relay is discovered by
/// the internet within hours and the reputational damage is not recoverable.
pub struct Session<P: RelayPolicy> {
    phase: Phase,
    role: Role,
    policy: P,
    limits: Limits,
    hostname: String,
    authenticated: bool,
    errors: usize,

    /// The name the client gave in EHLO/HELO. SPF needs it, and for a bounce
    /// (null reverse path) it is the only identity there is to check.
    helo: Option<String>,
    from: Option<String>,
    recipients: Vec<String>,
    data: Vec<u8>,
    data_too_large: bool,
}

impl<P: RelayPolicy> Session<P> {
    pub fn new(hostname: impl Into<String>, role: Role, policy: P, limits: Limits) -> Self {
        Self {
            phase: Phase::Greeted,
            role,
            policy,
            limits,
            hostname: hostname.into(),
            // A submission session is not authenticated until it says so. The
            // default must be the restrictive one: a bug that leaves this true
            // is an open relay.
            authenticated: false,
            errors: 0,
            helo: None,
            from: None,
            recipients: Vec::new(),
            data: Vec::new(),
            data_too_large: false,
        }
    }

    /// Mark the session authenticated. Called by the server after a successful
    /// AUTH exchange, which is not modelled here.
    pub fn set_authenticated(&mut self, yes: bool) {
        self.authenticated = yes;
    }

    pub fn greeting(&self) -> String {
        format!("220 {} Nexus ESMTP ready\r\n", self.hostname)
    }

    pub fn is_closed(&self) -> bool {
        self.phase == Phase::Closed
    }

    /// Feed one line (without its CRLF) and get the action to take.
    pub async fn line(&mut self, raw: &[u8]) -> Action {
        if self.phase == Phase::Data {
            return self.data_line(raw);
        }

        if raw.len() > crate::command::MAX_COMMAND_LINE {
            return self.fail("500 5.5.2 Line too long");
        }

        // Commands are ASCII. Anything else is not a command we will guess at.
        let Ok(text) = std::str::from_utf8(raw) else {
            return self.fail("500 5.5.2 Syntax error");
        };

        match parse(text) {
            Command::Quit => {
                self.phase = Phase::Closed;
                Action::ReplyAndClose(format!("221 2.0.0 {} closing\r\n", self.hostname))
            }
            Command::Ehlo(name) | Command::Helo(name) => {
                self.helo = Some(name);
                self.reset_transaction();
                self.phase = Phase::Ready;
                // SIZE is advertised so a sender learns the cap before spending
                // bandwidth on a message we would reject.
                Action::Reply(format!(
                    "250-{} greets you\r\n250-SIZE {}\r\n250-8BITMIME\r\n250 SMTPUTF8\r\n",
                    self.hostname, self.limits.max_message_bytes
                ))
            }
            Command::Noop => Action::Reply("250 2.0.0 OK\r\n".into()),
            Command::Rset => {
                self.reset_transaction();
                if self.phase != Phase::Greeted {
                    self.phase = Phase::Ready;
                }
                Action::Reply("250 2.0.0 OK\r\n".into())
            }
            // Never confirm or deny that an address exists. Answering
            // truthfully hands a stranger a list of who lives here.
            Command::Refused(_) => {
                Action::Reply("252 2.5.2 Cannot verify; try sending the message\r\n".into())
            }
            Command::StartTls => Action::Reply("454 4.7.0 TLS not available\r\n".into()),
            Command::Auth => {
                if self.role == Role::Mx {
                    // Offering AUTH on the MX port invites credential stuffing
                    // against a service that has no reason to accept it.
                    self.fail("503 5.5.1 AUTH not available on this port")
                } else {
                    Action::Reply("504 5.5.4 Auth mechanism not supported\r\n".into())
                }
            }
            Command::MailFrom(addr) => self.mail_from(addr),
            Command::RcptTo(addr) => self.rcpt_to(addr).await,
            Command::Data => self.data_start(),
            Command::Unknown => self.fail("500 5.5.2 Syntax error"),
        }
    }

    fn mail_from(&mut self, addr: String) -> Action {
        match self.phase {
            Phase::Greeted => return self.fail("503 5.5.1 Send EHLO first"),
            Phase::Mail | Phase::Rcpt => {
                return self.fail("503 5.5.1 Transaction already in progress")
            }
            _ => {}
        }
        self.reset_transaction();
        // An empty reverse path is the bounce sender and is legal.
        self.from = Some(addr);
        self.phase = Phase::Mail;
        Action::Reply("250 2.1.0 Sender OK\r\n".into())
    }

    async fn rcpt_to(&mut self, addr: String) -> Action {
        if self.phase != Phase::Mail && self.phase != Phase::Rcpt {
            return self.fail("503 5.5.1 Send MAIL FROM first");
        }
        if self.recipients.len() >= self.limits.max_recipients {
            return self.fail("452 4.5.3 Too many recipients");
        }
        if addr.is_empty() {
            return self.fail("501 5.1.3 Recipient required");
        }

        // The open-relay guard, and the single most important decision in this
        // file. An anonymous session may only deliver to mailboxes we host.
        // Carrying mail to a third party for a stranger is what an open relay
        // is, and the internet finds one within hours.
        let permitted = match self.role {
            Role::Mx => self.policy.is_local(&addr).await,
            Role::Submission => self.authenticated,
        };
        if !permitted {
            return self.fail("550 5.7.1 Relay access denied");
        }

        self.recipients.push(addr);
        self.phase = Phase::Rcpt;
        Action::Reply("250 2.1.5 Recipient OK\r\n".into())
    }

    fn data_start(&mut self) -> Action {
        if self.phase != Phase::Rcpt {
            return self.fail("503 5.5.1 Need a recipient first");
        }
        self.phase = Phase::Data;
        self.data.clear();
        self.data_too_large = false;
        Action::Reply("354 Start mail input; end with <CRLF>.<CRLF>\r\n".into())
    }

    fn data_line(&mut self, raw: &[u8]) -> Action {
        // A lone dot ends the message.
        if raw == b"." {
            let reply = if self.data_too_large {
                format!("552 5.3.4 Message exceeds {} bytes\r\n", self.limits.max_message_bytes)
            } else {
                "250 2.0.0 Accepted\r\n".to_string()
            };

            let too_large = self.data_too_large;
            let from = self.from.clone().unwrap_or_default();
            let recipients = std::mem::take(&mut self.recipients);
            let data = std::mem::take(&mut self.data);
            self.reset_transaction();
            self.phase = Phase::Ready;

            let helo = self.helo.clone().unwrap_or_default();
            return if too_large {
                Action::Reply(reply)
            } else {
                Action::Deliver { from, recipients, data, helo, reply }
            };
        }

        // Once over the cap we keep reading to the terminating dot but stop
        // buffering. Closing the connection instead would leave the sender
        // retrying forever without ever learning why.
        if self.data.len() + raw.len() > self.limits.max_message_bytes {
            self.data_too_large = true;
            return Action::Continue;
        }

        // Undo dot-stuffing: a line the sender began with ".." is really one dot.
        let line = if raw.starts_with(b"..") { &raw[1..] } else { raw };
        self.data.extend_from_slice(line);
        self.data.extend_from_slice(b"\r\n");
        Action::Continue
    }

    fn fail(&mut self, reply: &str) -> Action {
        self.errors += 1;
        if self.errors >= self.limits.max_errors {
            self.phase = Phase::Closed;
            return Action::ReplyAndClose("421 4.7.0 Too many errors; goodbye\r\n".into());
        }
        Action::Reply(format!("{reply}\r\n"))
    }

    fn reset_transaction(&mut self) {
        self.from = None;
        self.recipients.clear();
        self.data.clear();
        self.data_too_large = false;
    }
}
