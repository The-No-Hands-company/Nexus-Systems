import { useState, useRef, useEffect } from "react";
import { Link, useNavigate, useSearchParams } from "react-router-dom";
import { sendMail, listUsers, type OutgoingAttachment, type SendOutcome } from "../../api";

interface Contact {
  name: string;
  email: string;
  role?: string;
}

const DEFAULT_CONTACTS: Contact[] = [
  { name: "Support Team", email: "support@tnhc.dev" },
  { name: "Platform Operator", email: "operator@tnhc.dev" },
  { name: "Platform Founder", email: "founder@tnhc.dev" },
];

/// Read a file as base64 without the data: prefix the reader adds.
function readAsBase64(file: File): Promise<OutgoingAttachment> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(new Error(`Could not read ${file.name}`));
    reader.onload = () => {
      const result = String(reader.result);
      // "data:<mime>;base64,<payload>" — only the payload is wanted.
      const comma = result.indexOf(",");
      resolve({
        filename: file.name,
        mime_type: file.type || "application/octet-stream",
        data: comma >= 0 ? result.slice(comma + 1) : result,
      });
    };
    reader.readAsDataURL(file);
  });
}

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${Math.round(bytes / 1024)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

export default function MailCompose() {
  const navigate = useNavigate();
  const [params] = useSearchParams();
  const [to, setTo] = useState(params.get("to") ?? "");
  const [cc, setCc] = useState("");
  const [bcc, setBcc] = useState("");
  const [showCcBcc, setShowCcBcc] = useState(false);
  const [subject, setSubject] = useState(params.get("subject") ?? "");
  const [text, setText] = useState(params.get("quote") ?? "");
  const [htmlContent, setHtmlContent] = useState("");
  const [editorMode, setEditorMode] = useState<"wysiwyg" | "source">("wysiwyg");
  const inReplyTo = params.get("inReplyTo") ?? undefined;
  const [attachments, setAttachments] = useState<OutgoingAttachment[]>([]);
  const [isDragging, setIsDragging] = useState(false);
  const [sending, setSending] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [outcomes, setOutcomes] = useState<SendOutcome[] | null>(null);
  const editorRef = useRef<HTMLDivElement>(null);
  const [contacts, setContacts] = useState<Contact[]>(DEFAULT_CONTACTS);
  const [activeSuggestField, setActiveSuggestField] = useState<"to" | "cc" | "bcc" | null>(null);

  useEffect(() => {
    void listUsers()
      .then((users) => {
        const directoryContacts: Contact[] = users
          .filter((u) => u.email)
          .map((u) => ({
            name: u.username,
            email: u.email,
            role: u.role,
          }));
        setContacts((prev) => {
          const map = new Map<string, Contact>();
          for (const c of [...prev, ...directoryContacts]) {
            map.set(c.email.toLowerCase(), c);
          }
          return Array.from(map.values());
        });
      })
      .catch(() => {});
  }, []);

  function getQueryForField(value: string): string {
    const parts = value.split(",");
    return (parts[parts.length - 1] || "").trim().toLowerCase();
  }

  function getFilteredContacts(value: string): Contact[] {
    const query = getQueryForField(value);
    if (!query) return [];
    return contacts
      .filter((c) => c.email.toLowerCase().includes(query) || c.name.toLowerCase().includes(query))
      .slice(0, 6);
  }

  function applyContact(
    field: "to" | "cc" | "bcc",
    contactEmail: string,
    currentValue: string,
  ) {
    const parts = currentValue.split(",").map((s) => s.trim()).filter(Boolean);
    parts.pop();
    parts.push(contactEmail);
    const newValue = parts.join(", ") + ", ";
    if (field === "to") setTo(newValue);
    if (field === "cc") setCc(newValue);
    if (field === "bcc") setBcc(newValue);
    setActiveSuggestField(null);
  }

  // Initialize editor content from reply quote if present
  useEffect(() => {
    const quote = params.get("quote");
    if (quote && editorRef.current) {
      const safeHtml = quote
        .split("\n")
        .map((l) => (l.startsWith(">") ? `<blockquote>${l.slice(1).trim()}</blockquote>` : `<p>${l}</p>`))
        .join("");
      editorRef.current.innerHTML = safeHtml;
      setHtmlContent(safeHtml);
    }
  }, [params]);

  // Execute formatting commands on the contentEditable div
  function execCmd(command: string, value: string | undefined = undefined) {
    if (editorMode !== "wysiwyg") return;
    document.execCommand(command, false, value);
    if (editorRef.current) {
      setHtmlContent(editorRef.current.innerHTML);
      setText(editorRef.current.innerText);
    }
  }

  function handleAddLink() {
    const url = prompt("Enter link URL (e.g. https://tnhc.dev):");
    if (url) {
      execCmd("createLink", url);
    }
  }

  function handleEditorInput() {
    if (editorRef.current) {
      setHtmlContent(editorRef.current.innerHTML);
      setText(editorRef.current.innerText);
    }
  }

  async function handleFiles(files: File[]) {
    try {
      const read = await Promise.all(files.map(readAsBase64));
      setAttachments((prev) => [...prev, ...read]);
      setError(null);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Could not read that file.");
    }
  }

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    const recipients = to.split(",").map((s) => s.trim()).filter(Boolean);
    if (recipients.length === 0) {
      setError("Add at least one recipient.");
      return;
    }

    setSending(true);
    setError(null);

    // Prepare final text and html payloads
    let finalHtml = htmlContent;
    let finalText = text;
    if (editorMode === "wysiwyg" && editorRef.current) {
      finalHtml = editorRef.current.innerHTML;
      finalText = editorRef.current.innerText;
    } else if (editorMode === "source") {
      finalHtml = `<div style="font-family: sans-serif; line-height: 1.6;">${text
        .split("\n\n")
        .map((p) => `<p>${p.replace(/\n/g, "<br/>")}</p>`)
        .join("")}</div>`;
    }

    try {
      const ccRecipients = cc.split(",").map((s) => s.trim()).filter(Boolean);
      const bccRecipients = bcc.split(",").map((s) => s.trim()).filter(Boolean);
      const res = await sendMail({
        to: recipients,
        ...(ccRecipients.length ? { cc: ccRecipients } : {}),
        ...(bccRecipients.length ? { bcc: bccRecipients } : {}),
        subject,
        text: finalText,
        html: finalHtml || undefined,
        in_reply_to: inReplyTo,
        attachments: attachments.length ? attachments : undefined,
      });

      setOutcomes(res.outcomes);
      if (res.outcomes.every((o) => o.status !== "rejected")) {
        setTimeout(() => navigate("/mail"), 1200);
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : "Could not send.");
    } finally {
      setSending(false);
    }
  }

  return (
    <section className="mx-auto max-w-3xl space-y-4 px-4 py-6 sm:p-8">
      <div className="flex items-center justify-between">
        <Link to="/mail" className="text-sm text-zinc-400 hover:text-zinc-200">
          ← Mail
        </Link>
        <span className="text-xs text-zinc-500 font-mono">Sovereign Mail API</span>
      </div>

      <div className="flex items-center justify-between">
        <h1 className="text-xl font-semibold text-zinc-100">New message</h1>
        <button
          type="button"
          onClick={() => setShowCcBcc((v) => !v)}
          className="text-xs text-zinc-400 hover:text-zinc-200"
        >
          {showCcBcc ? "Hide CC/BCC" : "Show CC/BCC"}
        </button>
      </div>

      <form onSubmit={submit} className="space-y-4">
        <div className="relative block">
          <label className="block">
            <span className="text-xs uppercase tracking-wide text-zinc-500">To</span>
            <input
              value={to}
              onChange={(e) => {
                setTo(e.target.value);
                setActiveSuggestField("to");
              }}
              onFocus={() => setActiveSuggestField("to")}
              onBlur={() => setTimeout(() => setActiveSuggestField(null), 200)}
              placeholder="someone@tnhc.dev, other@example.com"
              className="mt-1 w-full rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-sm text-zinc-100 placeholder-zinc-600 focus:border-accent focus:outline-none"
            />
          </label>
          {activeSuggestField === "to" && getFilteredContacts(to).length > 0 && (
            <ul className="absolute z-20 left-0 right-0 mt-1 max-h-48 overflow-auto rounded-md border border-zinc-700 bg-zinc-950 p-1 shadow-xl">
              {getFilteredContacts(to).map((c) => (
                <li
                  key={c.email}
                  onMouseDown={(e) => {
                    e.preventDefault();
                    applyContact("to", c.email, to);
                  }}
                  className="cursor-pointer rounded px-2.5 py-1.5 text-xs text-zinc-300 hover:bg-zinc-800 hover:text-white flex items-center justify-between"
                >
                  <span className="font-medium">{c.name} &lt;{c.email}&gt;</span>
                  {c.role && <span className="text-[10px] uppercase text-zinc-500">{c.role}</span>}
                </li>
              ))}
            </ul>
          )}
        </div>

        {showCcBcc && (
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
            <div className="relative block">
              <label className="block">
                <span className="text-xs uppercase tracking-wide text-zinc-500">CC</span>
                <input
                  value={cc}
                  onChange={(e) => {
                    setCc(e.target.value);
                    setActiveSuggestField("cc");
                  }}
                  onFocus={() => setActiveSuggestField("cc")}
                  onBlur={() => setTimeout(() => setActiveSuggestField(null), 200)}
                  placeholder="cc@example.com"
                  className="mt-1 w-full rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-sm text-zinc-100 placeholder-zinc-600 focus:border-accent focus:outline-none"
                />
              </label>
              {activeSuggestField === "cc" && getFilteredContacts(cc).length > 0 && (
                <ul className="absolute z-20 left-0 right-0 mt-1 max-h-48 overflow-auto rounded-md border border-zinc-700 bg-zinc-950 p-1 shadow-xl">
                  {getFilteredContacts(cc).map((c) => (
                    <li
                      key={c.email}
                      onMouseDown={(e) => {
                        e.preventDefault();
                        applyContact("cc", c.email, cc);
                      }}
                      className="cursor-pointer rounded px-2.5 py-1.5 text-xs text-zinc-300 hover:bg-zinc-800 hover:text-white flex items-center justify-between"
                    >
                      <span className="font-medium">{c.name} &lt;{c.email}&gt;</span>
                      {c.role && <span className="text-[10px] uppercase text-zinc-500">{c.role}</span>}
                    </li>
                  ))}
                </ul>
              )}
            </div>

            <div className="relative block">
              <label className="block">
                <span className="text-xs uppercase tracking-wide text-zinc-500">BCC</span>
                <input
                  value={bcc}
                  onChange={(e) => {
                    setBcc(e.target.value);
                    setActiveSuggestField("bcc");
                  }}
                  onFocus={() => setActiveSuggestField("bcc")}
                  onBlur={() => setTimeout(() => setActiveSuggestField(null), 200)}
                  placeholder="bcc@example.com"
                  className="mt-1 w-full rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-sm text-zinc-100 placeholder-zinc-600 focus:border-accent focus:outline-none"
                />
              </label>
              {activeSuggestField === "bcc" && getFilteredContacts(bcc).length > 0 && (
                <ul className="absolute z-20 left-0 right-0 mt-1 max-h-48 overflow-auto rounded-md border border-zinc-700 bg-zinc-950 p-1 shadow-xl">
                  {getFilteredContacts(bcc).map((c) => (
                    <li
                      key={c.email}
                      onMouseDown={(e) => {
                        e.preventDefault();
                        applyContact("bcc", c.email, bcc);
                      }}
                      className="cursor-pointer rounded px-2.5 py-1.5 text-xs text-zinc-300 hover:bg-zinc-800 hover:text-white flex items-center justify-between"
                    >
                      <span className="font-medium">{c.name} &lt;{c.email}&gt;</span>
                      {c.role && <span className="text-[10px] uppercase text-zinc-500">{c.role}</span>}
                    </li>
                  ))}
                </ul>
              )}
            </div>
          </div>
        )}

        <label className="block">
          <span className="text-xs uppercase tracking-wide text-zinc-500">Subject</span>
          <input
            value={subject}
            onChange={(e) => setSubject(e.target.value)}
            placeholder="Subject of your message"
            className="mt-1 w-full rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-sm text-zinc-100 focus:border-accent focus:outline-none"
          />
        </label>

        {/* Rich WYSIWYG Editor Container */}
        <div className="rounded-lg border border-zinc-800 bg-zinc-900 overflow-hidden">
          {/* Editor Toolbar */}
          <div className="flex flex-wrap items-center justify-between border-b border-zinc-800 bg-zinc-950/60 px-3 py-2 text-xs text-zinc-400 gap-1.5">
            <div className="flex flex-wrap items-center gap-1">
              <button
                type="button"
                onClick={() => execCmd("bold")}
                title="Bold (Ctrl+B)"
                className="h-7 w-7 rounded border border-zinc-800 bg-zinc-900 font-bold text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                B
              </button>
              <button
                type="button"
                onClick={() => execCmd("italic")}
                title="Italic (Ctrl+I)"
                className="h-7 w-7 rounded border border-zinc-800 bg-zinc-900 italic font-serif text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                I
              </button>
              <button
                type="button"
                onClick={() => execCmd("underline")}
                title="Underline (Ctrl+U)"
                className="h-7 w-7 rounded border border-zinc-800 bg-zinc-900 underline text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                U
              </button>
              <button
                type="button"
                onClick={() => execCmd("strikeThrough")}
                title="Strikethrough"
                className="h-7 w-7 rounded border border-zinc-800 bg-zinc-900 line-through text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                S
              </button>
              <span className="mx-1 h-4 w-px bg-zinc-800" />
              <button
                type="button"
                onClick={() => execCmd("formatBlock", "<h2>")}
                title="Heading 2"
                className="px-2 h-7 rounded border border-zinc-800 bg-zinc-900 font-semibold text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                H2
              </button>
              <button
                type="button"
                onClick={() => execCmd("formatBlock", "<h3>")}
                title="Heading 3"
                className="px-2 h-7 rounded border border-zinc-800 bg-zinc-900 font-medium text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                H3
              </button>
              <button
                type="button"
                onClick={() => execCmd("formatBlock", "<p>")}
                title="Paragraph"
                className="px-2 h-7 rounded border border-zinc-800 bg-zinc-900 text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                ¶
              </button>
              <span className="mx-1 h-4 w-px bg-zinc-800" />
              <button
                type="button"
                onClick={() => execCmd("insertUnorderedList")}
                title="Bulleted List"
                className="h-7 w-7 rounded border border-zinc-800 bg-zinc-900 text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                •
              </button>
              <button
                type="button"
                onClick={() => execCmd("insertOrderedList")}
                title="Numbered List"
                className="h-7 w-7 rounded border border-zinc-800 bg-zinc-900 text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                1.
              </button>
              <button
                type="button"
                onClick={() => execCmd("formatBlock", "<blockquote>")}
                title="Quote"
                className="px-2 h-7 rounded border border-zinc-800 bg-zinc-900 text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                ”
              </button>
              <button
                type="button"
                onClick={handleAddLink}
                title="Insert Link"
                className="px-2 h-7 rounded border border-zinc-800 bg-zinc-900 text-zinc-200 hover:border-zinc-700 hover:text-white"
              >
                🔗 Link
              </button>
              <button
                type="button"
                onClick={() => execCmd("removeFormat")}
                title="Clear Formatting"
                className="px-2 h-7 rounded border border-zinc-800 bg-zinc-900 text-zinc-400 hover:border-zinc-700 hover:text-white"
              >
                Tx
              </button>
            </div>

            <div className="flex items-center gap-1">
              <button
                type="button"
                onClick={() => setEditorMode("wysiwyg")}
                className={`rounded px-2 py-1 text-xs transition-colors ${
                  editorMode === "wysiwyg"
                    ? "bg-zinc-800 text-zinc-100 font-medium"
                    : "text-zinc-400 hover:text-zinc-200"
                }`}
              >
                WYSIWYG
              </button>
              <button
                type="button"
                onClick={() => {
                  if (editorRef.current && editorMode === "wysiwyg") {
                    setText(editorRef.current.innerText);
                  }
                  setEditorMode("source");
                }}
                className={`rounded px-2 py-1 text-xs transition-colors ${
                  editorMode === "source"
                    ? "bg-zinc-800 text-zinc-100 font-medium"
                    : "text-zinc-400 hover:text-zinc-200"
                }`}
              >
                Plain Text
              </button>
            </div>
          </div>

          {/* Editor Body */}
          {editorMode === "wysiwyg" ? (
            <div
              ref={editorRef}
              contentEditable
              onInput={handleEditorInput}
              role="textbox"
              aria-multiline="true"
              aria-label="Message content"
              data-placeholder="Write your email here…"
              className="min-h-[16rem] p-4 text-sm text-zinc-100 leading-relaxed focus:outline-none select-text [&_blockquote]:border-l-2 [&_blockquote]:border-zinc-600 [&_blockquote]:pl-3 [&_blockquote]:text-zinc-400 [&_h2]:text-lg [&_h2]:font-bold [&_h3]:text-base [&_h3]:font-semibold [&_ul]:list-disc [&_ul]:pl-5 [&_ol]:list-decimal [&_ol]:pl-5 [&_a]:text-accent [&_a]:underline"
            />
          ) : (
            <textarea
              value={text}
              onChange={(e) => setText(e.target.value)}
              rows={14}
              placeholder="Write your email in plain text…"
              className="w-full bg-transparent p-4 font-mono text-sm text-zinc-100 focus:outline-none"
            />
          )}
        </div>

        {/* Drag-and-Drop Attachment Dropzone */}
        <div
          onDragOver={(e) => {
            e.preventDefault();
            setIsDragging(true);
          }}
          onDragLeave={() => setIsDragging(false)}
          onDrop={async (e) => {
            e.preventDefault();
            setIsDragging(false);
            const files = Array.from(e.dataTransfer.files);
            if (files.length) await handleFiles(files);
          }}
          className={`rounded-lg border-2 border-dashed p-4 text-center transition-colors ${
            isDragging
              ? "border-accent bg-accent/5 text-accent"
              : "border-zinc-800 bg-zinc-900/50 text-zinc-400 hover:border-zinc-700"
          }`}
        >
          <div className="flex flex-col items-center justify-center gap-1.5">
            <svg className="h-6 w-6 text-zinc-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={1.5} d="M15.172 7l-6.586 6.586a2 2 0 102.828 2.828l6.414-6.586a4 4 0 00-5.656-5.656l-6.415 6.585a6 6 0 108.486 8.486L20.5 13" />
            </svg>
            <span className="text-xs font-medium text-zinc-300">
              Drag & drop attachments here, or{" "}
              <label className="cursor-pointer text-accent underline hover:text-accent/80">
                browse files
                <input
                  type="file"
                  multiple
                  onChange={async (e) => {
                    const files = Array.from(e.target.files ?? []);
                    if (files.length) await handleFiles(files);
                    e.target.value = "";
                  }}
                  className="sr-only"
                />
              </label>
            </span>
            <span className="text-[11px] text-zinc-500">Supports documents, PDFs, images up to 25MB</span>
          </div>

          {attachments.length > 0 && (
            <ul className="mt-3 grid grid-cols-1 sm:grid-cols-2 gap-2 text-left">
              {attachments.map((a, i) => (
                <li
                  key={`${a.filename}-${i}`}
                  className="flex items-center justify-between rounded border border-zinc-800 bg-zinc-900 px-3 py-2 text-xs text-zinc-300"
                >
                  <div className="min-w-0 flex items-center gap-2">
                    <span className="text-zinc-500">📎</span>
                    <div className="truncate">
                      <p className="truncate font-medium text-zinc-200">{a.filename}</p>
                      <p className="text-[10px] text-zinc-500 font-mono">
                        {formatBytes(Math.round(a.data.length * 0.75))} • {a.mime_type}
                      </p>
                    </div>
                  </div>
                  <button
                    type="button"
                    onClick={() => setAttachments((prev) => prev.filter((_, n) => n !== i))}
                    className="ml-2 text-zinc-500 hover:text-red-400 p-1"
                    title="Remove attachment"
                  >
                    ✕
                  </button>
                </li>
              ))}
            </ul>
          )}
        </div>

        {error && <p role="alert" className="text-sm text-red-400">{error}</p>}

        {outcomes && (
          <ul className="space-y-1 text-sm rounded bg-zinc-900/80 p-3 border border-zinc-800" role="status">
            {outcomes.map((o) => (
              <li
                key={o.recipient}
                className={o.status === "rejected" ? "text-red-400 font-medium" : "text-emerald-400"}
              >
                {o.recipient}: {o.status}
                {o.reason ? ` — ${o.reason}` : ""}
              </li>
            ))}
          </ul>
        )}

        <div className="flex items-center gap-3">
          <button
            type="submit"
            disabled={sending}
            className="rounded bg-zinc-100 px-5 py-2 text-sm font-semibold text-zinc-900 hover:bg-white disabled:opacity-50 transition-colors shadow-sm"
          >
            {sending ? "Sending…" : "Send Message"}
          </button>
          <Link
            to="/mail"
            className="rounded border border-zinc-800 px-4 py-2 text-sm text-zinc-400 hover:bg-zinc-900 hover:text-zinc-200"
          >
            Cancel
          </Link>
        </div>
      </form>
    </section>
  );
}
