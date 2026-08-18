use std::sync::Arc;
use std::time::Duration;

use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpListener, TcpStream};

use crate::session::{Action, Limits, RelayPolicy, Role, Session};

/// What the server does with a completed message.
pub trait Sink: Send + Sync + 'static {
    /// Accept a message. Returning an error produces a temporary failure, so
    /// the sender retries rather than losing the mail.
    fn deliver(
        &self,
        from: String,
        recipients: Vec<String>,
        data: Vec<u8>,
    ) -> impl std::future::Future<Output = Result<(), String>> + Send;
}

/// How long a connection may sit idle. RFC 5321 suggests five minutes; a
/// stranger holding sockets open is otherwise free denial of service.
const IDLE_TIMEOUT: Duration = Duration::from_secs(300);

pub struct SmtpServer<P, S> {
    pub hostname: String,
    pub role: Role,
    pub limits: Limits,
    pub policy: Arc<P>,
    pub sink: Arc<S>,
}

impl<P, S> SmtpServer<P, S>
where
    P: RelayPolicy + Clone + Send + Sync + 'static,
    S: Sink,
{
    pub async fn serve(self: Arc<Self>, listener: TcpListener) -> std::io::Result<()> {
        loop {
            let (stream, peer) = listener.accept().await?;
            let me = Arc::clone(&self);
            tokio::spawn(async move {
                if let Err(e) = me.handle(stream).await {
                    // A failed connection is routine on a public port — a port
                    // scanner, a broken client, a dropped link. Logged at debug
                    // so real problems are not buried in noise.
                    tracing::debug!(%peer, error = %e, "smtp connection ended");
                }
            });
        }
    }

    async fn handle(&self, stream: TcpStream) -> std::io::Result<()> {
        let (read_half, mut write) = stream.into_split();
        let mut reader = BufReader::new(read_half);
        let mut session = Session::new(
            self.hostname.clone(),
            self.role,
            (*self.policy).clone(),
            self.limits,
        );

        write.write_all(session.greeting().as_bytes()).await?;

        let mut line = Vec::new();
        loop {
            line.clear();
            let read = tokio::time::timeout(
                IDLE_TIMEOUT,
                reader.read_until(b'\n', &mut line),
            )
            .await;

            let n = match read {
                Ok(Ok(n)) => n,
                Ok(Err(e)) => return Err(e),
                Err(_) => {
                    let _ = write.write_all(b"421 4.4.2 Idle timeout\r\n").await;
                    return Ok(());
                }
            };
            if n == 0 {
                return Ok(()); // peer hung up
            }

            // Strip the line ending; the session works in terms of lines.
            while line.last() == Some(&b'\n') || line.last() == Some(&b'\r') {
                line.pop();
            }

            match session.line(&line) {
                Action::Continue => {}
                Action::Reply(r) => write.write_all(r.as_bytes()).await?,
                Action::ReplyAndClose(r) => {
                    write.write_all(r.as_bytes()).await?;
                    return Ok(());
                }
                Action::Deliver { from, recipients, data, reply } => {
                    // The reply is sent only after the message is safely
                    // stored. Acknowledging first and then failing would tell
                    // the sender their mail was accepted while it was not,
                    // which is how mail is silently lost.
                    match self.sink.deliver(from, recipients, data).await {
                        Ok(()) => write.write_all(reply.as_bytes()).await?,
                        Err(e) => {
                            tracing::warn!(error = %e, "smtp delivery failed");
                            write
                                .write_all(b"451 4.3.0 Temporary failure; try again later\r\n")
                                .await?
                        }
                    }
                }
            }

            if session.is_closed() {
                return Ok(());
            }
        }
    }
}
