export type AppGuide = {
  id: string;
  name: string;
  tagline: string;
  category: "Everyday" | "Work & Docs" | "Create" | "Develop" | "Operate";
  whatIsIt: string;
  whenToUse: string;
  topCapabilities: { title: string; desc: string }[];
  tutorialSteps: {
    step: number;
    title: string;
    description: string;
    actionTip: string;
  }[];
  proTips: string[];
  keyboardShortcuts?: { key: string; action: string }[];
  docSectionId: string;
};

export const APP_GUIDES: Record<string, AppGuide> = {
  "nexus-email": {
    id: "nexus-email",
    name: "Nexus Mail",
    tagline: "Private, sovereign webmail with full HTML formatting and secure S3 attachments",
    category: "Everyday",
    whatIsIt:
      "Nexus Mail is a sovereign email client and API engine built on high-performance Rust services (nexus-mailapi and nexus-mailsmtp). It replaces commercial corporate email with zero telemetry, automatic HTML sanitization, and encrypted credential storage.",
    whenToUse:
      "Use Nexus Mail for all personal and business correspondence where privacy is non-negotiable. Ideal for sending large file attachments directly stored in your sovereign MinIO S3 cluster, searching long historical email threads, and routing mail through verified SMTP relays.",
    topCapabilities: [
      { title: "WYSIWYG Rich-Text Composer", desc: "Craft formatted emails with headings, lists, blockquotes, inline links, and code snippets." },
      { title: "S3 MinIO Attachments", desc: "Upload and download attachments safely via authenticated presigned S3 URLs." },
      { title: "Smart Search & Filter Pills", desc: "Filter your inbox instantly by sender (from:), attachments (has:attachment), or unread state." },
      { title: "Graceful Queueing", desc: "Outbox background queueing that retries automatically if rate limits are reached." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Access your Mailbox",
        description: "Open Nexus Mail from the Dashboard. New accounts are automatically provisioned with a dedicated mailbox upon first sign-in.",
        actionTip: "Click 'Nexus Mail' in the Everyday shelf or press Cmd+K and type 'Mail'.",
      },
      {
        step: 2,
        title: "Compose and Format an Email",
        description: "Click '+ Compose' in the top bar. Use the rich toolbar to format your message and click the paperclip icon to attach files.",
        actionTip: "Type recipient names for instant auto-completion from the Nexus-Auth user directory.",
      },
      {
        step: 3,
        title: "Organize with Folders & Search",
        description: "Organize messages into Inbox, Sent, Drafts, and Trash. Click search pills to find important correspondence in milliseconds.",
        actionTip: "Use 'has:attachment' in the search bar to locate sent documents quickly.",
      },
    ],
    proTips: [
      "Press Cmd+Enter in the compose window to send your email immediately.",
      "Attachment uploads are scanned and stream directly to MinIO S3 without overloading memory.",
      "All inbound HTML emails are sanitized against CSS injections and tracking pixels.",
    ],
    keyboardShortcuts: [
      { key: "c", action: "Open new compose modal" },
      { key: "Cmd + Enter", action: "Send message" },
      { key: "/", action: "Focus mail search input" },
      { key: "Esc", action: "Close message or compose window" },
    ],
    docSectionId: "nexus-email",
  },

  "nexus-calendar": {
    id: "nexus-calendar",
    name: "Nexus Calendar",
    tagline: "Sovereign time-management with day/week time grids and RFC 5545 .ics interoperability",
    category: "Everyday",
    whatIsIt:
      "Nexus Calendar is a full-featured scheduling application featuring interactive Month, Week, and Day time-grid views, recurring event management, timezone conversions, and seamless import/export with Google Calendar, Apple Calendar, and Outlook via RFC 5545 .ics files.",
    whenToUse:
      "Use Nexus Calendar to organize your day, coordinate team schedules, plan multi-day milestones, and book meetings without letting third-party cloud providers mine your calendar data.",
    topCapabilities: [
      { title: "Week & Day Time-Grid Views", desc: "Hourly slot view with dragging, time indicators, and quick event placement." },
      { title: "RFC 5545 .ics Import & Export", desc: "Seamless two-way sync with Apple Calendar, Google Calendar, and mobile devices." },
      { title: "Recurring Events Core", desc: "Manage daily, weekly, and monthly recurrence rules with 'only this event' vs 'all future' edits." },
      { title: "Timezone Conversion", desc: "Accurate UTC and localized timezone display for distributed remote teams." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Select Your Time View",
        description: "Toggle between Month, Week, and Day views using the top navigation bar to see your agenda at your preferred zoom level.",
        actionTip: "Week view gives you an hourly breakdown of your schedule from 8 AM to 8 PM.",
      },
      {
        step: 2,
        title: "Create and Schedule Events",
        description: "Click any time slot on the grid or click '+ New Event'. Set title, location, category color, and recurring rules.",
        actionTip: "Set recurrence to 'Weekly' for recurring team standups or sync meetings.",
      },
      {
        step: 3,
        title: "Sync with External Calendars",
        description: "Click 'Export .ics' to download your calendar file, or 'Import .ics' to bring in external calendar feeds.",
        actionTip: "You can import existing Google or Apple calendar exports in one click.",
      },
    ],
    proTips: [
      "Double click any date on Month view to jump directly into the Day time-grid for that date.",
      "Export your personal .ics feed to subscribe to your Nexus Calendar on your phone.",
    ],
    keyboardShortcuts: [
      { key: "w", action: "Switch to Week view" },
      { key: "m", action: "Switch to Month view" },
      { key: "d", action: "Switch to Day view" },
      { key: "t", action: "Jump to Today" },
    ],
    docSectionId: "nexus-calendar",
  },

  "nexus-project": {
    id: "nexus-project",
    name: "Nexus Project",
    tagline: "Agile task tracking, milestone critical-path scheduling, and team Kanban boards",
    category: "Work & Docs",
    whatIsIt:
      "Nexus Project is an enterprise-grade project management application designed for agile teams and solo creators. It features drag-and-drop Kanban boards, Work Breakdown Structure (WBS), subtask checklists with progress bars, milestone timelines, and assignees linked to Nexus-Auth.",
    whenToUse:
      "Use Nexus Project when leading complex software releases, client deliverables, or personal initiatives that require clear milestone tracking, dependency sequencing, and owner accountability.",
    topCapabilities: [
      { title: "Interactive Kanban Board", desc: "Move tasks fluidly across Backlog, In Progress, In Review, and Done columns." },
      { title: "Subtask Checklists & Progress", desc: "Break tasks into actionable steps with real-time percentage progress bars." },
      { title: "Milestone Critical-Path Timeline", desc: "Visualize milestone dependencies and deadlines with interactive Gantt view." },
      { title: "Auth Assignee Directory", desc: "Assign owners directly from your registered Nexus team members." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Create a Project Board",
        description: "Open Nexus Project and click '+ New Project'. Give your project a title, description, and target completion date.",
        actionTip: "Group related tasks under specific milestone phases (e.g., 'Phase 1: Alpha', 'Phase 2: Launch').",
      },
      {
        step: 2,
        title: "Add Tasks with Subtask Checklists",
        description: "Click '+ Add Task' in any column. Add subtasks inside the task modal to track granular checkboxes and progress.",
        actionTip: "As you check off subtasks, the card's progress bar automatically updates.",
      },
      {
        step: 3,
        title: "Switch to Milestone Timeline View",
        description: "Click 'Timeline / Gantt' to visualize the critical path of milestones, deadlines, and dependencies.",
        actionTip: "Hover over milestone blocks to see duration, assignees, and remaining deliverables.",
      },
    ],
    proTips: [
      "Use subtask checklists to prevent monolithic tickets that sit in 'In Progress' for weeks.",
      "Filter boards by assignee to see your personal daily punchlist.",
    ],
    keyboardShortcuts: [
      { key: "n", action: "New task" },
      { key: "b", action: "Switch to Board view" },
      { key: "g", action: "Switch to Gantt timeline view" },
      { key: "/", action: "Filter project tasks" },
    ],
    docSectionId: "nexus-project",
  },

  "nexus-docs": {
    id: "nexus-docs",
    name: "Nexus Docs",
    tagline: "Notion-style rich-text document workspace with folders, covers, and block controls",
    category: "Work & Docs",
    whatIsIt:
      "Nexus Docs is a sovereign document editing workspace. It gives you flexible block-level rich-text editing (callouts, toggle lists, headings, code formatting, checklists) organized in clean folders with custom emoji icons and header covers.",
    whenToUse:
      "Use Nexus Docs for collaborative specs, meeting minutes, company handbooks, and design proposals where structured, beautiful document layout is needed.",
    topCapabilities: [
      { title: "Modular Block Editor", desc: "Format headings (H1-H3), callout blocks, checkboxes, quote blocks, and code syntax." },
      { title: "Folder & Document Tree", desc: "Nest documents infinitely in folders with drag-and-drop reorganization." },
      { title: "Cover & Emoji Customization", desc: "Personalize every document with custom banner covers and expressive emoji icons." },
      { title: "Markdown & HTML Export", desc: "Export your documents cleanly to standard Markdown or print-ready PDF/HTML." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Create a Document or Folder",
        description: "Click '+ New Page' in the sidebar. Choose whether to place it at the root or nest it inside an existing folder.",
        actionTip: "Click 'Add Icon' to pick an emoji and 'Add Cover' for a banner aesthetic.",
      },
      {
        step: 2,
        title: "Format with Block Controls",
        description: "Type '/' anywhere in the editor to open the block insertion menu (Heading, Checklist, Code, Callout).",
        actionTip: "Use callout blocks for important warnings, notes, or key takeaways.",
      },
      {
        step: 3,
        title: "Share or Export",
        description: "Click 'Export' in the top right corner to download as standard Markdown for documentation or archiving.",
        actionTip: "Your documents are stored in local SQLite databases with zero cloud vendor lock-in.",
      },
    ],
    proTips: [
      "Type '[] ' at the start of any line to automatically create an interactive checklist item.",
      "Use '```' followed by language name (e.g. ```typescript) for syntax-highlighted code blocks.",
    ],
    keyboardShortcuts: [
      { key: "/", action: "Open block command menu" },
      { key: "Cmd + b", action: "Toggle Bold" },
      { key: "Cmd + i", action: "Toggle Italic" },
      { key: "Cmd + k", action: "Insert Link" },
    ],
    docSectionId: "nexus-docs",
  },

  "nexus-notes": {
    id: "nexus-notes",
    name: "Nexus Notes",
    tagline: "Lightning-fast Markdown scratchpad with live side-by-side preview and tag indexing",
    category: "Work & Docs",
    whatIsIt:
      "Nexus Notes is a high-speed, distraction-free Markdown notepad. It provides instantaneous full-text search, live preview, custom notebook organization, and tag filtering for daily thoughts, code snippets, and meeting logs.",
    whenToUse:
      "Use Nexus Notes when you need to capture ideas immediately without waiting for heavy editors to load. Perfect for developer scratchpads, daily standup logs, and quick research bookmarks.",
    topCapabilities: [
      { title: "Live Side-by-Side Preview", desc: "Write raw Markdown on the left and see instant rendered HTML on the right." },
      { title: "Notebook & Tag Taxonomies", desc: "Group notes by notebooks and tag them with '#hashtags' for instant filtering." },
      { title: "Pin Shortcuts", desc: "Pin your most critical daily notes to the top of the sidebar for one-click access." },
      { title: "Instant Sub-millisecond Search", desc: "Search through thousands of notes with instant full-text filtering as you type." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Create a Note",
        description: "Click '+ New Note' or press 'n'. The note is instantly saved to your local database as you type.",
        actionTip: "Give the note a title on the first line with '# Note Title'.",
      },
      {
        step: 2,
        title: "Tag and Categorize",
        description: "Type hashtags like '#dev', '#meeting', or '#ideas' anywhere in the body to index them automatically.",
        actionTip: "Click any tag pill in the sidebar to view all matching notes instantly.",
      },
      {
        step: 3,
        title: "Pin Critical Notes",
        description: "Click the pin icon on any note to keep it anchored at the top of your notebook list.",
        actionTip: "Pinned notes also surface in the Dashboard home widget for fast reference.",
      },
    ],
    proTips: [
      "Notes are autosaved on every keystroke with zero chance of lost work.",
      "Toggle preview mode with Cmd+P when reviewing formatted tables and diagrams.",
    ],
    keyboardShortcuts: [
      { key: "n", action: "Create new note" },
      { key: "Cmd + p", action: "Toggle preview mode" },
      { key: "Cmd + f", action: "Find in note" },
    ],
    docSectionId: "nexus-notes",
  },

  "nexus-wiki": {
    id: "nexus-wiki",
    name: "Nexus Wiki",
    tagline: "Structured organizational knowledge base with category indexes, backlinks, and revision rollbacks",
    category: "Work & Docs",
    whatIsIt:
      "Nexus Wiki is a sovereign knowledge repository powered by a high-performance FastAPI/SQLite engine. It provides structured categorization, article revision rollbacks, automatic bidirectional [[wikilinks]], and team documentation hubs.",
    whenToUse:
      "Use Nexus Wiki for canonical engineering documentation, architectural decisions (ADRs), platform onboarding guides, and team playbooks that require strict revision tracking and historical audits.",
    topCapabilities: [
      { title: "Automatic [[Wikilinks]]", desc: "Link articles using [[Article Title]] syntax with automatic backlink discovery." },
      { title: "Revision History & Rollback", desc: "Inspect diffs across past revisions and restore any historical snapshot in one click." },
      { title: "Category Hierarchies", desc: "Organize articles by department, platform shelf, or engineering vertical." },
      { title: "TOC Generation", desc: "Automatic table of contents generated from article heading structure." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Browse or Create an Article",
        description: "Open the Wiki and click '+ New Article' or navigate through categories on the left sidebar.",
        actionTip: "Select the relevant category (e.g. Architecture, Operations, Guides) to index it properly.",
      },
      {
        step: 2,
        title: "Link Articles with [[Wikilinks]]",
        description: "Connect concepts by typing [[Target Article Name]] to create automatic cross-references.",
        actionTip: "The Wiki automatically builds a 'Backlinks' section showing all incoming references.",
      },
      {
        step: 3,
        title: "Audit and Roll Back Revisions",
        description: "Click 'History' to review previous edits, inspect line-by-line diffs, or roll back to any past state.",
        actionTip: "Every edit preserves author DID, timestamp, and full change summary.",
      },
    ],
    proTips: [
      "Use the Wiki for persistent canonical truths and Nexus Notes for temporary daily scratchpads.",
      "The revision log is cryptographically immutable and safe for compliance records.",
    ],
    keyboardShortcuts: [
      { key: "e", action: "Edit current article" },
      { key: "h", action: "View revision history" },
      { key: "/", action: "Search wiki knowledge base" },
    ],
    docSectionId: "nexus-wiki",
  },

  "nexus-draw": {
    id: "nexus-draw",
    name: "Nexus Draw",
    tagline: "Infinite-canvas whiteboard, architecture sketching, and wireframing studio",
    category: "Create",
    whatIsIt:
      "Nexus Draw is an infinite-canvas sketching and diagramming tool. It offers vector shapes, freehand drawing, smart arrow connectors, full Undo/Redo history (Cmd+Z), and high-resolution PNG/SVG asset exports.",
    whenToUse:
      "Use Nexus Draw for system architecture diagrams, database schema visualizers, UI wireframes, flowcharts, and collaborative brainstorm whiteboards.",
    topCapabilities: [
      { title: "Infinite Vector Canvas", desc: "Pan and zoom freely with high-DPI crisp vector rendering." },
      { title: "Smart Connector Arrows", desc: "Snap arrows to shape bounding boxes that dynamically adjust as you move shapes." },
      { title: "History Stack (Undo/Redo)", desc: "Full Cmd+Z and Cmd+Shift+Z undo/redo stack across all canvas operations." },
      { title: "PNG & SVG Vector Export", desc: "Export high-resolution transparent PNGs or lossless SVGs for documentation." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Select a Drawing Tool",
        description: "Choose Rectangle, Ellipse, Diamond, Arrow, Text, or Pencil from the top toolbar.",
        actionTip: "Press 'r' for rectangle, 'a' for arrow, or 't' for text.",
      },
      {
        step: 2,
        title: "Connect Architecture Nodes",
        description: "Draw two boxes and drag an arrow from the edge of one box to another to snap a smart connector.",
        actionTip: "Moving either box keeps the arrow automatically linked and routed.",
      },
      {
        step: 3,
        title: "Export Diagram Asset",
        description: "Click 'Export' to save your diagram as a clean SVG for your README or a PNG for presentations.",
        actionTip: "Enable 'Transparent Background' for sleek dark-mode documentation embedding.",
      },
    ],
    proTips: [
      "Hold Spacebar and drag the mouse to pan smoothly across the canvas.",
      "Hold Shift while drawing shapes to lock 1:1 aspect ratio (perfect squares and circles).",
    ],
    keyboardShortcuts: [
      { key: "v", action: "Select tool" },
      { key: "r", action: "Rectangle tool" },
      { key: "a", action: "Arrow connector tool" },
      { key: "Cmd + z", action: "Undo" },
      { key: "Cmd + Shift + z", action: "Redo" },
    ],
    docSectionId: "nexus-draw",
  },

  "nexus-design": {
    id: "nexus-design",
    name: "Nexus Design",
    tagline: "Professional UI and component design studio with infinite artboards and styling inspectors",
    category: "Create",
    whatIsIt:
      "Nexus Design is a sovereign alternative to commercial interface design tools. It offers multi-artboard canvas editing, layer hierarchies, precise typography/fill/stroke styling inspectors, and design token integration.",
    whenToUse:
      "Use Nexus Design when creating high-fidelity website layouts, mobile app screens, design systems, and icon components before writing production code.",
    topCapabilities: [
      { title: "Multi-Artboard Workspaces", desc: "Design desktop, tablet, and mobile breakpoints side-by-side on one canvas." },
      { title: "Layer Tree & Groups", desc: "Organize components with intuitive nesting, locking, and visibility toggles." },
      { title: "Style & Layout Inspector", desc: "Pixel-perfect CSS control over auto-layout, padding, borders, shadows, and fills." },
      { title: "CSS & Code Generator", desc: "Inspect any design element to copy clean Tailwind, CSS, or React JSX code." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Create an Artboard",
        description: "Click 'Frame' in the toolbar and select Desktop (1440x900) or Mobile (390x844).",
        actionTip: "Frames serve as isolated viewports for designing specific device layouts.",
      },
      {
        step: 2,
        title: "Assemble UI Components",
        description: "Add shapes, typography, and buttons. Adjust typography, colors, and border radius in the right Inspector panel.",
        actionTip: "Use the Nexus Void & Slate design tokens for consistent dark-mode styling.",
      },
      {
        step: 3,
        title: "Export Assets or Copy CSS",
        description: "Select any frame or component and click 'Export @2x PNG' or copy the generated CSS directly into your code.",
        actionTip: "Use the 'Inspect' tab to get exact margin, padding, and hex values.",
      },
    ],
    proTips: [
      "Hold Alt / Option while hovering over elements to measure exact pixel distances between them.",
      "Double-click any text layer to enter inline editing mode.",
    ],
    keyboardShortcuts: [
      { key: "f", action: "Frame / Artboard tool" },
      { key: "t", action: "Text tool" },
      { key: "Cmd + g", action: "Group selected layers" },
      { key: "Option + drag", action: "Duplicate element" },
    ],
    docSectionId: "nexus-design",
  },

  "nexus-graphic": {
    id: "nexus-graphic",
    name: "Nexus Graphic",
    tagline: "Vector graphic illustration studio for icons, logos, and digital artwork",
    category: "Create",
    whatIsIt:
      "Nexus Graphic is a precision vector illustration tool. It features Bézier curve pen tools, Boolean path operations (Union, Subtract, Intersect), gradient fills, and clean SVG optimization pipelines.",
    whenToUse:
      "Use Nexus Graphic when crafting app icons, branding marks, vector logos, and illustrative badges that need to scale infinitely without pixelation.",
    topCapabilities: [
      { title: "Precision Pen & Bézier Tool", desc: "Place anchor points and manipulate control handles for smooth organic curves." },
      { title: "Boolean Path Operations", desc: "Combine and subtract multiple shapes to construct intricate logo silhouettes." },
      { title: "Linear & Radial Gradients", desc: "Apply multi-stop gradient fills with interactive on-canvas angle handles." },
      { title: "Clean SVG Export", desc: "Export minified SVGs stripped of unnecessary metadata for blazing-fast web loading." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Start a Vector Canvas",
        description: "Open Nexus Graphic and configure your artboard dimensions (e.g. 512x512 for an app icon).",
        actionTip: "Turn on the pixel grid for pixel-snapped icon design.",
      },
      {
        step: 2,
        title: "Draw Shapes with the Pen Tool",
        description: "Click to create sharp corners or click-and-drag to pull smooth Bézier curve handles.",
        actionTip: "Close the path by clicking back on the starting anchor point.",
      },
      {
        step: 3,
        title: "Combine Paths and Export",
        description: "Select overlapping shapes and click 'Union' or 'Subtract' in the path operations bar, then export SVG.",
        actionTip: "SVGs generated by Nexus Graphic are 100% compliant with standard web browsers.",
      },
    ],
    proTips: [
      "Press 'p' to switch instantly to the Pen tool.",
      "Hold Shift to constrain line angles to 45° and 90° increments.",
    ],
    keyboardShortcuts: [
      { key: "p", action: "Pen tool" },
      { key: "a", action: "Direct Selection (Anchor point edit)" },
      { key: "Cmd + e", action: "Export vector asset" },
    ],
    docSectionId: "nexus-graphic",
  },

  "nexus-photos": {
    id: "nexus-photos",
    name: "Nexus Photos",
    tagline: "Private photo library with album organization, EXIF inspection, and thumbnail caching",
    category: "Create",
    whatIsIt:
      "Nexus Photos is a private media library and gallery service. It organizes your photos into albums, reads camera EXIF metadata (camera model, exposure, date taken), generates fast cached thumbnails, and stores originals safely in MinIO S3.",
    whenToUse:
      "Use Nexus Photos for personal and project media storage without sending your private family or company photos to commercial ad networks.",
    topCapabilities: [
      { title: "Album & Collection Organizer", desc: "Create themed albums and organize photos by year, event, or project." },
      { title: "EXIF Metadata Inspector", desc: "View capture timestamps, focal length, aperture, ISO, and camera models." },
      { title: "High-Performance Thumbnailing", desc: "Responsive thumbnail grids that scroll smoothly through thousands of images." },
      { title: "Full-Resolution Download", desc: "Preserve original RAW/JPEG image quality with secure direct download." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Create an Album",
        description: "Click '+ New Album' in the sidebar and enter an album name (e.g., 'Product Launch 2026').",
        actionTip: "Albums can be shared with specific team members or kept strictly private.",
      },
      {
        step: 2,
        title: "Upload Photos",
        description: "Drag and drop image files directly onto the browser window or click 'Upload Photos'.",
        actionTip: "Files are uploaded in parallel and stored in dedicated MinIO buckets.",
      },
      {
        step: 3,
        title: "View and Inspect",
        description: "Click any photo to open the lightbox viewer. Click the 'Info' icon to inspect EXIF metadata.",
        actionTip: "Use left and right arrow keys to navigate through the gallery.",
      },
    ],
    proTips: [
      "Press Spacebar while viewing a thumbnail for a quick preview.",
      "EXIF location data is preserved privately and never leaked to external third parties.",
    ],
    keyboardShortcuts: [
      { key: "Left / Right", action: "Previous / Next photo" },
      { key: "i", action: "Toggle EXIF metadata panel" },
      { key: "Esc", action: "Close lightbox" },
    ],
    docSectionId: "nexus-photos",
  },

  "nexus-video": {
    id: "nexus-video",
    name: "Nexus Video",
    tagline: "Sovereign video streaming, playlist management, and media player platform",
    category: "Create",
    whatIsIt:
      "Nexus Video is a decentralized video streaming and catalog service. It provides a modern HTML5 media player, playlist management, channel catalogs, and streaming video delivery backed by sovereign storage.",
    whenToUse:
      "Use Nexus Video for company tutorials, engineering demo recordings, webinars, and team screencasts without relying on public video platforms with ads and tracking.",
    topCapabilities: [
      { title: "Responsive HTML5 Video Player", desc: "Full playback controls, speed toggles (0.5x - 2x), and fullscreen mode." },
      { title: "Channel & Playlist Management", desc: "Curate series of videos into structured playlists for sequential learning." },
      { title: "Secure Streaming Delivery", desc: "Stream MP4/WebM videos smoothly without buffering or third-party ads." },
      { title: "Video Metadata & Chapters", desc: "Add video descriptions, tags, and timestamped chapter markers." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Upload a Video",
        description: "Click '+ Upload Video' and select an MP4 or WebM video file from your computer.",
        actionTip: "Add a descriptive title, category, and custom thumbnail image.",
      },
      {
        step: 2,
        title: "Organize into Playlists",
        description: "Create a playlist (e.g. 'Platform Onboarding Course') and add videos to the playlist queue.",
        actionTip: "Playlists automatically play the next video in sequence upon completion.",
      },
      {
        step: 3,
        title: "Watch and Share",
        description: "Stream the video directly in the browser or copy the direct link to share with teammates.",
        actionTip: "Adjust playback speed to 1.5x for efficient review of technical presentations.",
      },
    ],
    proTips: [
      "Use keyboard shortcuts (Space to Pause, F for Fullscreen, M to Mute) for effortless viewing.",
      "Videos are streamed directly from storage without tracking analytics.",
    ],
    keyboardShortcuts: [
      { key: "Space", action: "Play / Pause" },
      { key: "f", action: "Toggle Fullscreen" },
      { key: "m", action: "Mute / Unmute" },
      { key: "j / l", action: "Seek backward / forward 10 seconds" },
    ],
    docSectionId: "nexus-video",
  },

  "nexus-deploy": {
    id: "nexus-deploy",
    name: "Nexus Deploy",
    tagline: "Push-to-deploy cloud platform with automatic HTTPS, rootless sandboxing, and instant rollbacks",
    category: "Develop",
    whatIsIt:
      "Nexus Deploy is a self-hostable, sovereign alternative to Railway and Render. It connects to Git repositories or container registries, automatically builds containers with zero configuration, provisions HTTPS subdomains, and enforces rootless cgroup sandboxing.",
    whenToUse:
      "Use Nexus Deploy to deploy web services, full-stack apps, background workers, and APIs with automatic SSL certificates, environment variables, live build logs, and one-click rollbacks.",
    topCapabilities: [
      { title: "Zero-Config Railpack Builds", desc: "Automatically detects Bun, Node, Python, Rust, and Go apps and builds minimal OCI containers." },
      { title: "Rootless Sandboxing (CapEff=0)", desc: "Customer workloads run with dropped capabilities and loopback/LAN network isolation." },
      { title: "Live Streaming Build Logs", desc: "Watch build outputs, dependency installations, and startup checks in real-time." },
      { title: "Instant Deployment Rollbacks", desc: "Roll back to any previous successful release in milliseconds with zero downtime." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Connect a Repository",
        description: "Open Nexus Deploy and click '+ New Service'. Select a Git repository URL or connect a Docker container image.",
        actionTip: "Nexus Deploy automatically configures build scripts, port detection, and health checks.",
      },
      {
        step: 2,
        title: "Configure Environment Variables",
        description: "Add encrypted environment variables (e.g., DATABASE_URL, API_KEY) in the 'Variables' tab.",
        actionTip: "Secrets are encrypted with AES-256 and injected securely into container runtime environments.",
      },
      {
        step: 3,
        title: "Deploy and Monitor",
        description: "Click 'Deploy'. Watch live streaming logs as the container compiles and provisions an HTTPS domain under *.tnhc.dev.",
        actionTip: "If any deploy fails health checks, traffic remains safely pinned to the previous release.",
      },
    ],
    proTips: [
      "Run 'npm run doctor' inside apps/Nexus-Deploy to verify all 13 sandbox preflight isolation gates.",
      "Containers cannot access host loopback or LAN subnets, ensuring complete tenant isolation.",
    ],
    keyboardShortcuts: [
      { key: "d", action: "Trigger manual deployment" },
      { key: "l", action: "View live streaming logs" },
      { key: "r", action: "Roll back to previous release" },
    ],
    docSectionId: "nexus-deploy",
  },

  "nexus-code": {
    id: "nexus-code",
    name: "Nexus Code",
    tagline: "Sovereign Git repository browser, branch navigator, commit history tree, and gists",
    category: "Develop",
    whatIsIt:
      "Nexus Code is a web-based Git repository viewer and snippet manager. It provides syntax-highlighted source browsing, branch/tag switching, commit history trees with diffs, and private gists.",
    whenToUse:
      "Use Nexus Code to review source code, inspect recent commit histories, navigate repository branches, and share code snippets across your organization.",
    topCapabilities: [
      { title: "Syntax-Highlighted Code Viewer", desc: "Clean, high-performance viewing for TypeScript, Rust, Python, Go, and 50+ languages." },
      { title: "Commit History & Diff Inspector", desc: "View detailed commit logs, author signatures, and side-by-side file diffs." },
      { title: "Branch & Tag Navigator", desc: "Switch fluidly between feature branches, release tags, and the main branch." },
      { title: "Private Code Snippets / Gists", desc: "Save and share quick scripts, configuration files, and code examples securely." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Select a Repository",
        description: "Browse the repository list to select any platform service or connected project repository.",
        actionTip: "Use the search bar to locate specific repositories instantly.",
      },
      {
        step: 2,
        title: "Explore Files and Commits",
        description: "Navigate the file tree to inspect source code. Click 'History' to see the chronological commit log.",
        actionTip: "Click on any commit hash to inspect the full diff showing added and removed lines.",
      },
      {
        step: 3,
        title: "Share a Snippet",
        description: "Click 'Snippets' to paste a reusable script or config file, set visibility, and copy the link.",
        actionTip: "Snippets can be embedded in Wiki articles and Docs pages.",
      },
    ],
    proTips: [
      "Press 't' on any repository file view to activate fuzzy file search.",
      "Nexus Code runs directly against local Git bare repos with zero external dependency on GitHub.",
    ],
    keyboardShortcuts: [
      { key: "t", action: "Fuzzy file finder" },
      { key: "b", action: "Switch branch or tag" },
      { key: "y", action: "Copy permanent link to current commit" },
    ],
    docSectionId: "nexus-code",
  },

  "nexus-vault": {
    id: "nexus-vault",
    name: "Nexus Vault",
    tagline: "AES-256 encrypted API secrets manager with secret sharing audit logs and key rotation",
    category: "Develop",
    whatIsIt:
      "Nexus Vault is a centralized, cryptographically secure secrets and credentials manager. It encrypts sensitive API keys, database credentials, and service tokens with AES-256-GCM, logs every access in an immutable audit trail, and supports automated key rotation.",
    whenToUse:
      "Use Nexus Vault to store and share credentials among team members, manage production secrets for Nexus Deploy services, and rotate expiring tokens safely.",
    topCapabilities: [
      { title: "AES-256-GCM Encryption", desc: "Zero-knowledge secret storage where keys are encrypted before hitting SQLite storage." },
      { title: "Secret Sharing Audit Trail", desc: "Immutable log recording who viewed, created, or rotated a secret, with timestamps and IPs." },
      { title: "Cryptographic Key Rotation", desc: "Rotate master encryption keys and re-encrypt all stored secrets with zero downtime." },
      { title: "Environment Injection", desc: "Inject secrets directly into containers running under Nexus Deploy." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Create a Secret",
        description: "Click '+ New Secret'. Enter key name (e.g., STRIPE_SECRET_KEY), value, and optional expiration date.",
        actionTip: "Assign tags like 'Production', 'Staging', or 'Finance' for easy organization.",
      },
      {
        step: 2,
        title: "Reveal and Copy",
        description: "Secrets are masked by default. Click the eye icon to reveal the value. The access is immediately logged in the audit trail.",
        actionTip: "Clipboard auto-clears after 30 seconds to prevent clipboard sniffing.",
      },
      {
        step: 3,
        title: "Inspect Audit Logs",
        description: "Click 'Audit Log' to review access history and verify that no unauthorized team member viewed production credentials.",
        actionTip: "Audit logs are cryptographically verifiable and cannot be edited or deleted.",
      },
    ],
    proTips: [
      "Never hardcode secrets in Git repositories; store them in Nexus Vault and reference them by key name.",
      "Enable expiration alerts to get notified before critical SSL or API tokens expire.",
    ],
    keyboardShortcuts: [
      { key: "n", action: "New secret" },
      { key: "/", action: "Filter secrets" },
      { key: "a", action: "View audit logs" },
    ],
    docSectionId: "nexus-vault",
  },

  "nexus-terminal": {
    id: "nexus-terminal",
    name: "Nexus Terminal",
    tagline: "Web-based PTY terminal emulator with multi-tab sessions and authenticated operator access",
    category: "Develop",
    whatIsIt:
      "Nexus Terminal is a secure, browser-based terminal emulator connected to real pseudo-terminals (PTY) over authenticated WebSockets. It enables authorized node operators to run shell commands, monitor background processes, and inspect server health directly in the browser.",
    whenToUse:
      "Use Nexus Terminal when managing remote Nexus nodes, running diagnostic check scripts, reviewing logs, or executing CLI tools without needing external SSH clients.",
    topCapabilities: [
      { title: "Full PTY Terminal Emulation", desc: "Supports ANSI colors, interactive ncurses tools (htop, vim, tmux), and window resizing." },
      { title: "Multi-Tab Sessions", desc: "Run multiple parallel shell sessions in independent tabs within the same workspace." },
      { title: "Command Audit Logging", desc: "All executed commands are recorded with operator identity for security compliance." },
      { title: "Nexus CLI Integration", desc: "Pre-loaded with platform CLI tools including radar, deploy.sh, and bun/cargo tools." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Open a Terminal Session",
        description: "Click 'Nexus Terminal' in the Develop shelf. An authenticated PTY session launches automatically.",
        actionTip: "Only authenticated operators with the 'admin' or 'founder' role can access the terminal.",
      },
      {
        step: 2,
        title: "Run Platform Diagnostics",
        description: "Type './deploy/production/deploy.sh status' or 'bun run radar' to check platform health.",
        actionTip: "Use 'clear' to reset screen output or 'Ctrl+C' to terminate running commands.",
      },
      {
        step: 3,
        title: "Open Additional Tabs",
        description: "Click the '+' button in the terminal header to launch a secondary terminal session.",
        actionTip: "You can tail logs in one tab while executing builds in another.",
      },
    ],
    proTips: [
      "Use Ctrl+Shift+C / Ctrl+Shift+V for copying and pasting text inside the terminal.",
      "Terminal sessions use rootless unprivileged sandboxes to protect host integrity.",
    ],
    keyboardShortcuts: [
      { key: "Ctrl + `", action: "Toggle terminal drawer" },
      { key: "Ctrl + t", action: "New terminal tab" },
      { key: "Ctrl + w", action: "Close current tab" },
    ],
    docSectionId: "nexus-terminal",
  },

  "nexus-ai": {
    id: "nexus-ai",
    name: "Nexus AI",
    tagline: "Autonomous agentic reasoning, FastAPI model orchestration, and memory store",
    category: "Develop",
    whatIsIt:
      "Nexus AI is a sovereign artificial intelligence service powered by Python FastAPI. It coordinates LLM inference, autonomous task planning loops, tool dispatching across ecosystem APIs, and vector embeddings for semantic code/doc search.",
    whenToUse:
      "Use Nexus AI for intelligent workflow automation, querying platform documentation via natural language, code review assistance, and orchestrating multi-step autonomous tasks.",
    topCapabilities: [
      { title: "FastAPI Model Routing Engine", desc: "Supports local models and external providers with streaming response pipelines." },
      { title: "Agentic Loop Planning", desc: "Autonomous reasoning engine with step-by-step reflection, scratchpad, and verification." },
      { title: "Ecosystem Tool Dispatch", desc: "Agents can call platform APIs to inspect databases, check deployments, and send notifications." },
      { title: "RAG Vector Knowledge Base", desc: "Semantic retrieval across all platform code, guides, and architectural decisions." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Open the AI Interface",
        description: "Navigate to Nexus AI to access the AI playground and agent console.",
        actionTip: "You can prompt the agent to analyze system logs or summarize ecosystem docs.",
      },
      {
        step: 2,
        title: "Execute Agent Tasks",
        description: "Provide a multi-step objective. Watch the agent plan tasks, execute tools, and verify outputs.",
        actionTip: "The agent displays its scratchpad reasoning so you can observe each decision.",
      },
      {
        step: 3,
        title: "Connect with Ecosystem APIs",
        description: "Use the REST API at https://ai.tnhc.dev/health and /api/v1/chat to integrate AI into your custom apps.",
        actionTip: "All AI requests authenticate via your existing Nexus Auth session cookie.",
      },
    ],
    proTips: [
      "Query platform status with 'Check ecosystem health' to receive an instant diagnostic summary.",
      "The service runs lean with lazy model loading to minimize CPU and RAM footprint.",
    ],
    keyboardShortcuts: [
      { key: "Enter", action: "Send prompt" },
      { key: "Shift + Enter", action: "New line in prompt" },
      { key: "Esc", action: "Stop generating response" },
    ],
    docSectionId: "nexus-ai",
  },

  "nexus-database": {
    id: "nexus-database",
    name: "Nexus Database",
    tagline: "Sovereign Rust database engine with ACID transactions, indexing, and SQL parsing",
    category: "Develop",
    whatIsIt:
      "Nexus Database is a custom, high-performance database engine written in Rust. It features an ACID storage engine, B-Tree index structures, an SQL query parser and execution engine, and the Web Database Studio interface.",
    whenToUse:
      "Use Nexus Database for structured application data storage, executing SQL queries, managing relational schemas, and auditing database transactions without running bloated third-party database servers.",
    topCapabilities: [
      { title: "Rust ACID Storage Engine", desc: "Crash-safe transactional storage with WAL (write-ahead logging) and B-Tree indexing." },
      { title: "Full SQL Parser & Executor", desc: "Standard SQL syntax support for SELECT, INSERT, UPDATE, DELETE, and CREATE TABLE." },
      { title: "Web Database Studio", desc: "Dark-mode browser GUI for inspecting tables, viewing schemas, and running ad-hoc queries." },
      { title: "Atomic Snapshots & Streaming", desc: "Point-in-time recovery and snapshot replication to S3 storage." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Open Web Database Studio",
        description: "Navigate to https://database.tnhc.dev to access the database landing page and web console.",
        actionTip: "Verify that the engine status displays 'Active & Healthy' with port 3088 connected.",
      },
      {
        step: 2,
        title: "Inspect Database Tables",
        description: "Browse the list of tables on the left sidebar to view columns, primary keys, and indexed fields.",
        actionTip: "Click any table name to preview the first 100 rows of data.",
      },
      {
        step: 3,
        title: "Execute SQL Queries",
        description: "Type an SQL query into the query editor and click 'Execute Query' (or press Cmd+Enter).",
        actionTip: "Query execution times and affected row counts are displayed in the results table.",
      },
    ],
    proTips: [
      "The engine binary is compiled in Rust with zero runtime dependencies.",
      "Check health at https://database.tnhc.dev/health which returns 200 OK.",
    ],
    keyboardShortcuts: [
      { key: "Cmd + Enter", action: "Execute SQL query" },
      { key: "Cmd + l", action: "Clear query editor" },
    ],
    docSectionId: "nexus-database",
  },

  "nexus-chat": {
    id: "nexus-chat",
    name: "Nexus Chat",
    tagline: "Real-time sovereign team messaging, topic channels, emoji reactions, and voice rooms",
    category: "Work & Docs",
    whatIsIt:
      "Nexus Chat is a private, community-owned messaging platform powered by a Rust Axum WebSocket backend and a responsive React frontend. It offers topic channels, direct messages, emoji reactions, user presence, and encrypted voice chat rooms.",
    whenToUse:
      "Use Nexus Chat for daily team communication, watercooler discussions, voice standups, and developer collaboration without sending messages through Discord or Slack.",
    topCapabilities: [
      { title: "Axum WebSocket Engine", desc: "Ultra-low latency message streaming capable of handling thousands of concurrent events." },
      { title: "Topic Channels & Direct Messages", desc: "Organize discussions into public channels or private encrypted direct messages." },
      { title: "Emoji Reactions & Markdown", desc: "Expressive reactions and full markdown support with code snippets and blockquotes." },
      { title: "Live User Presence", desc: "Real-time indicators showing who is online, idle, busy (DND), or offline." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Join or Browse Channels",
        description: "Browse public channels (#general, #announcements, #engineering) in the left sidebar.",
        actionTip: "Click the '+' icon next to Channels to create a new topic channel.",
      },
      {
        step: 2,
        title: "Send Messages & Code Snippets",
        description: "Type in the message box. Use markdown for bold (**text**), italics, and backticks (```) for code blocks.",
        actionTip: "Hover over any message to react with emojis or start a reply thread.",
      },
      {
        step: 3,
        title: "Connect to Voice Rooms",
        description: "Click any Voice channel in the channel list to join the WebRTC audio room with low-latency audio.",
        actionTip: "Use the voice bar at the bottom to mute, deafen, or disconnect.",
      },
    ],
    proTips: [
      "Use '@username' to mention a teammate and trigger an in-shell notification.",
      "Message history is stored locally in SQLite with zero telemetry.",
    ],
    keyboardShortcuts: [
      { key: "Up Arrow", action: "Edit your last sent message" },
      { key: "Cmd + k", action: "Quick channel switcher" },
      { key: "Esc", action: "Mark channel as read" },
    ],
    docSectionId: "nexus-chat",
  },

  "nexus-hosting": {
    id: "nexus-hosting",
    name: "Nexus Hosting",
    tagline: "Decentralized static site hosting, MinIO S3 object storage, and automated TLS",
    category: "Operate",
    whatIsIt:
      "Nexus Hosting provides static web hosting and sovereign object storage. It automatically provisions ACME TLS certificates via Caddy, serves web applications directly from MinIO S3 buckets, and handles custom domain routing.",
    whenToUse:
      "Use Nexus Hosting to publish documentation sites, marketing landing pages, single-page web applications (SPAs), and public media buckets with zero vendor lock-in.",
    topCapabilities: [
      { title: "MinIO S3 Object Storage", desc: "S3-compatible bucket storage with access keys, presigned URLs, and high throughput." },
      { title: "Virtual Host Static Web Serving", desc: "Deploy HTML, CSS, and JS bundles served with automatic Brotli/Gzip compression." },
      { title: "Automatic HTTPS Provisioning", desc: "Zero-configuration Let's Encrypt certificates managed automatically by Caddy." },
      { title: "Custom Domain Routing", desc: "Attach custom domains (e.g., mysite.com) with automated DNS verification." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Create a Hosting Site or Bucket",
        description: "Open Nexus Hosting and click '+ New Site'. Specify the site name and target subdomain.",
        actionTip: "A dedicated MinIO S3 bucket is created automatically for your site's files.",
      },
      {
        step: 2,
        title: "Upload Site Files",
        description: "Upload your compiled production assets (index.html, style.css, script.js) directly or via the S3 API.",
        actionTip: "You can use standard S3 CLI tools (aws s3 sync dist/ s3://mysite) to deploy.",
      },
      {
        step: 3,
        title: "Verify Live HTTPS",
        description: "Visit your live site at https://<site>.tnhc.dev to verify automatic HTTPS and fast asset loading.",
        actionTip: "Add a custom CNAME to route your personal domain name to the site.",
      },
    ],
    proTips: [
      "All static assets are cached at the edge for sub-20ms response times worldwide.",
      "Object storage supports presigned URLs for secure temporary file downloads.",
    ],
    keyboardShortcuts: [
      { key: "u", action: "Upload files to active bucket" },
      { key: "n", action: "New hosting site" },
    ],
    docSectionId: "nexus-hosting",
  },

  "nexus-cloud": {
    id: "nexus-cloud",
    name: "Nexus Cloud",
    tagline: "Central service registry, health monitoring, release governance, and node orchestration",
    category: "Operate",
    whatIsIt:
      "Nexus Cloud is the operational brain and service registry of the Nexus ecosystem. It manages live health monitoring across all 30 ecosystem daemons, handles catalog proposals and release gating, controls node federation, and orchestrates automated snapshot backups.",
    whenToUse:
      "Use Nexus Cloud to inspect platform health, monitor active service latencies, audit catalog releases, trigger service restarts, and view the live inter-service dependency graph.",
    topCapabilities: [
      { title: "Systems-API Tool Registry", desc: "Authoritative inventory of all ecosystem services, capabilities, and delivery routes." },
      { title: "Automated Health Supervision", desc: "Sub-minute polling and reachability probes detecting and reporting degraded services." },
      { title: "Release Governance Engine", desc: "Controls staged vs released vs retired states for all ecosystem applications." },
      { title: "Federation & Node Trust", desc: "Coordinates peer trust, cryptographic handshakes, and multi-node discovery." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Open Cloud Overview Console",
        description: "Navigate to the Cloud console from the Dashboard or visit https://cloud.tnhc.dev.",
        actionTip: "Review the system health strip showing all 30 services active and green.",
      },
      {
        step: 2,
        title: "Inspect Tool Registrations",
        description: "Click 'Tools & Services' to see every registered app, its upstream port, health, and capabilities.",
        actionTip: "Inspect individual tool records to view registration history and heartbeat counts.",
      },
      {
        step: 3,
        title: "Manage Node Federation",
        description: "Review peer nodes, trust attestations, and sync states in the Federation tab.",
        actionTip: "Each node signs requests with post-quantum Ed25519 Phantom identities.",
      },
    ],
    proTips: [
      "Query registered tools directly via 'curl http://localhost:8787/api/v1/tools'.",
      "Cloud coordinates atomic SQLite snapshot backups across all platform databases.",
    ],
    keyboardShortcuts: [
      { key: "g", action: "Open service dependency graph" },
      { key: "r", action: "Refresh service health checks" },
    ],
    docSectionId: "nexus-cloud",
  },

  "nexus-auth": {
    id: "nexus-auth",
    name: "Nexus Auth",
    tagline: "Sovereign identity provider, Ed25519 decentralized identifiers (DIDs), RBAC, and sessions",
    category: "Operate",
    whatIsIt:
      "Nexus Auth is the sovereign authentication and identity service of Nexus Systems. It generates cryptographic Ed25519 DIDs, issues secure domain-wide HttpOnly session cookies across *.tnhc.dev, manages role-based access control (RBAC), and supports 2FA TOTP authenticators.",
    whenToUse:
      "Use Nexus Auth to manage your account profile, change passwords, view active sessions and devices, configure two-factor authentication, and manage organization user roles.",
    topCapabilities: [
      { title: "Cryptographic Ed25519 DIDs", desc: "Zero-knowledge decentralized identifiers replacing corporate identity trackers." },
      { title: "Domain-Wide HttpOnly SSO", desc: "Sign in once at auth.tnhc.dev to seamlessly access all 26 platform applications." },
      { title: "Role-Based Access Control", desc: "Four privilege tiers: Founder, Admin, Member, and Guest with strict capability gates." },
      { title: "Active Session Management", desc: "View all logged-in devices with IP and browser info, and revoke remote sessions in one click." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Sign in or Create Account",
        description: "Visit https://auth.tnhc.dev/login to sign in with your sovereign credentials.",
        actionTip: "New users are automatically redirected to the Dashboard and provisioned with a mailbox.",
      },
      {
        step: 2,
        title: "Configure Profile & Security",
        description: "Open 'Account Settings' to update your display name, email address, or change your password.",
        actionTip: "Enable 2FA TOTP using Google Authenticator, Aegis, or 1Password for extra security.",
      },
      {
        step: 3,
        title: "Review Active Sessions",
        description: "Scroll to 'Active Sessions' to see all devices currently authenticated. Click 'Sign Out Others' to revoke stale logins.",
        actionTip: "Revoking a session invalidates its cookie immediately across all ecosystem subdomains.",
      },
    ],
    proTips: [
      "Session cookies use SameSite=Lax and Secure flags to prevent cross-site request forgery (CSRF).",
      "Account settings can be accessed directly in the dashboard by clicking your avatar in the top right.",
    ],
    keyboardShortcuts: [
      { key: "Cmd + Shift + a", action: "Open Account Settings" },
      { key: "Cmd + Shift + l", action: "Sign out of platform" },
    ],
    docSectionId: "nexus-auth",
  },

  "nexus-guardian": {
    id: "nexus-guardian",
    name: "Nexus Guardian",
    tagline: "Security policy daemon, secret leak scanner, vulnerability auditor, and threat prevention",
    category: "Operate",
    whatIsIt:
      "Nexus Guardian is the security sentinel of the Nexus platform. Running on port 4320, it scans codebases for leaked tokens or keys, audits software dependencies against vulnerability databases, enforces rate-limiting policies, and serves the security status dashboard at https://guardian.tnhc.dev.",
    whenToUse:
      "Use Nexus Guardian to audit platform security posture, check for open dependency advisories, verify that no credentials leaked in Git commits, and monitor rate-limiting anomalies.",
    topCapabilities: [
      { title: "Secret Leak Scanner", desc: "Scans commits and code changes for exposed API tokens, private keys, and passwords." },
      { title: "Ecosystem Security Auditor", desc: "Regularly scans all 89 monorepo apps and packages for outdated or vulnerable libraries." },
      { title: "Live Security Dashboard", desc: "Dark-mode dashboard displaying threat status, active policies, and advisory counts." },
      { title: "Real-time Security Alerting", desc: "Triggers notifications when suspicious login patterns or access anomalies occur." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Access Guardian Status",
        description: "Visit https://guardian.tnhc.dev to view the Security & Threat Policy Daemon status page.",
        actionTip: "Verify that all 5 security scanning modules report 100% operational status.",
      },
      {
        step: 2,
        title: "Run Security Scans",
        description: "Run 'bun run check.sh' in apps/Nexus-Guardian to execute dependency audits and vulnerability checks.",
        actionTip: "All checks must pass before code is permitted into production deployment.",
      },
      {
        step: 3,
        title: "Review Compliance & Canaries",
        description: "Check https://status.tnhc.dev/canary.json to verify warrant canary signatures and integrity records.",
        actionTip: "Canaries are cryptographically verified daily to prove zero government subpoenas or leaks.",
      },
    ],
    proTips: [
      "Guardian runs with zero external cloud dependencies to ensure sovereignty.",
      "The daemon binds to loopback port 4320 and is proxied with Strict-Transport-Security.",
    ],
    keyboardShortcuts: [
      { key: "s", action: "Trigger security audit scan" },
    ],
    docSectionId: "nexus-guardian",
  },

  "nexus-edge": {
    id: "nexus-edge",
    name: "Nexus Edge",
    tagline: "Edge caching, reverse proxy ingress routing, cache invalidation, and rate-limiting gate",
    category: "Operate",
    whatIsIt:
      "Nexus Edge is the ingress routing and edge delivery daemon of Nexus Systems. Running on port 4340, it enforces edge caching rules, strips malicious tracking headers, executes token-bucket rate limiting to prevent DoS attacks, and provides instant cache invalidation on deployment.",
    whenToUse:
      "Use Nexus Edge to monitor platform ingress traffic, purge cached static assets upon pushing new web app releases, and ensure low latency for users connecting from worldwide origins.",
    topCapabilities: [
      { title: "Edge Ingress Routing", desc: "Directs inbound requests to the nearest healthy service origin across all 26 applications." },
      { title: "Token-Bucket Rate Limiting", desc: "Prevents brute-force and DoS attacks while maintaining generous burst allowances for real users." },
      { title: "Instant Cache Invalidation", desc: "Purge stale HTML and JS bundles across the cluster in milliseconds upon deployment." },
      { title: "Header Hardening & Security", desc: "Injects CSP, HSTS, X-Content-Type-Options, and Permissions-Policy headers automatically." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "View Edge Status Dashboard",
        description: "Navigate to https://edge.tnhc.dev to inspect the Edge Routing & Ingress Daemon status card.",
        actionTip: "Check active ingress status, edge cache hit ratios, and upstream latency stats.",
      },
      {
        step: 2,
        title: "Test Ingress Routing",
        description: "Send a request to https://edge.tnhc.dev/health to verify sub-5ms loopback response times.",
        actionTip: "Edge ensures that static assets bypass heavy server rendering for peak performance.",
      },
      {
        step: 3,
        title: "Trigger Cache Invalidation",
        description: "When deploying an updated frontend bundle, execute the invalidation webhook to refresh client caches instantly.",
        actionTip: "Users immediately receive the latest bundle without hard-refreshing their browsers.",
      },
    ],
    proTips: [
      "Edge caching respects Cache-Control headers with immutable fingerprinting on Vite bundles.",
      "The service operates rootless and uses minimal memory (<25MB RSS).",
    ],
    keyboardShortcuts: [
      { key: "p", action: "Purge edge cache" },
    ],
    docSectionId: "nexus-edge",
  },

  "nexus-network": {
    id: "nexus-network",
    name: "Nexus Network",
    tagline: "Mesh networking, private peer tunnels, WireGuard bridge, and node discovery",
    category: "Operate",
    whatIsIt:
      "Nexus Network is the sovereign mesh interconnect for distributed Nexus clusters. It provisions WireGuard encrypted peer-to-peer tunnels between independent nodes, automates peer discovery, and provides an interactive visual topology map.",
    whenToUse:
      "Use Nexus Network when connecting multiple physical servers, cloud VPS instances, or home lab nodes into a unified, encrypted private mesh network.",
    topCapabilities: [
      { title: "WireGuard Tunnel Core", desc: "Kernel-speed point-to-point cryptographic tunnels with zero handshake overhead." },
      { title: "Peer Node Discovery", desc: "Automatically identify and register new hardware nodes joining the federation." },
      { title: "Visual Mesh Map", desc: "Interactive map displaying active nodes, connection latencies, and packet transfer metrics." },
      { title: "One-Click Peer Provisioning", desc: "Generate secure provisioning keys to add new nodes to the mesh in seconds." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Inspect Active Nodes",
        description: "Open Nexus Network to view all registered nodes in your cluster.",
        actionTip: "Check node connection states, IP addresses, and ping latencies.",
      },
      {
        step: 2,
        title: "Generate a Peer Provisioning Key",
        description: "Click '+ Invite Node' to generate an encrypted WireGuard configuration snippet.",
        actionTip: "Paste the snippet on your new server to establish an encrypted tunnel in seconds.",
      },
      {
        step: 3,
        title: "Monitor Mesh Throughput",
        description: "Watch live packet counters and route health in the visual mesh topology map.",
        actionTip: "If any node goes offline, the mesh routes traffic around it automatically.",
      },
    ],
    proTips: [
      "All tunnel traffic is encrypted with ChaCha20-Poly1305 and authenticated with Curve25519.",
      "The network mesh runs completely peer-to-peer without central relay bottlenecks.",
    ],
    keyboardShortcuts: [
      { key: "m", action: "Toggle mesh map view" },
      { key: "i", action: "Invite new node" },
    ],
    docSectionId: "nexus-network",
  },

  "nexus-forge": {
    id: "nexus-forge",
    name: "Nexus Forge",
    tagline: "Platform compilation engine, CLI tooling, and component code generator",
    category: "Develop",
    whatIsIt:
      "Nexus Forge is the developer tooling and compilation platform for the monorepo. It powers cross-app code generation, feature manifest tracking, component compilation, and internal package distribution.",
    whenToUse:
      "Use Nexus Forge when scaffolding new sovereign apps, inspecting ecosystem component dependencies, or running cross-package build pipelines.",
    topCapabilities: [
      { title: "Forge Tooling Engine", desc: "High-speed TypeScript and Rust compilation and code generation utilities." },
      { title: "Feature Manifest Tracking", desc: "Tracks capability completion, API contracts, and quality gate statuses." },
      { title: "Monorepo Package Registry", desc: "Coordinates shared packages in packages/ (phantom-sdk, nexus-discovery, etc.)." },
      { title: "Scaffold Generator", desc: "Generates production-ready app skeletons with check.sh, Dockerfile, and theme tokens in seconds." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Open Forge Console",
        description: "Navigate to https://forge.tnhc.dev to inspect active build pipelines and package registries.",
        actionTip: "Review the monorepo package inventory across all 12 shared libraries.",
      },
      {
        step: 2,
        title: "Scaffold a New Service",
        description: "Use Forge to generate a new application skeleton matching Nexus standards.",
        actionTip: "New apps are automatically configured with Void & Slate theme tokens and check.sh.",
      },
      {
        step: 3,
        title: "Run Ecosystem Quality Gates",
        description: "Execute 'bash check.sh' across all active apps to ensure 100% test pass rates.",
        actionTip: "Forge guarantees that no breaking API changes reach production.",
      },
    ],
    proTips: [
      "Use Forge to check compatibility with the VERSION_MATRIX.md specification.",
      "Shared packages are managed using Bun workspaces with zero duplication.",
    ],
    keyboardShortcuts: [
      { key: "b", action: "Build all packages" },
      { key: "t", action: "Test all packages" },
    ],
    docSectionId: "nexus-forge",
  },

  "nexus-dashboard": {
    id: "nexus-dashboard",
    name: "Nexus Shell",
    tagline: "Unified sovereign platform workspace shell, app launcher, notification drawer, and docs hub",
    category: "Everyday",
    whatIsIt:
      "Nexus Shell is the primary front door and operating shell of Nexus Systems. It hosts the dynamic app launcher grid, integrated shell-native apps (Mail, Terminal, Account), the unified Platform Documentation Hub, global Cmd+K search, and the Notification Center.",
    whenToUse:
      "Use Nexus Shell as your home base: launch any of the 26 sovereign applications, search across apps and documents, read platform documentation, manage your profile, and monitor overall ecosystem health.",
    topCapabilities: [
      { title: "Dynamic App Launcher Grid", desc: "Curated category shelves (Everyday, Work, Create, Develop, Operate) with instant filtering." },
      { title: "Global Search (Cmd+K)", desc: "Lightning-fast search across all applications, documents, notes, and platform guides." },
      { title: "Integrated Docs Hub", desc: "Comprehensive documentation chapters and guided tutorials built directly into the shell." },
      { title: "Notification Center", desc: "Unified notification bell aggregating alerts from Mail, Chat, Project, and Deploy." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Navigate the Workspace Shell",
        description: "Browse the launcher grid to explore your applications organized into intuitive category shelves.",
        actionTip: "Click any app tile to open its launchpad and interactive guided tutorial.",
      },
      {
        step: 2,
        title: "Use Global Cmd+K Search",
        description: "Press Cmd+K anywhere in the dashboard to open the Command Palette. Type an app name or action.",
        actionTip: "Press Enter to launch apps instantly without touching your mouse.",
      },
      {
        step: 3,
        title: "Explore the Docs Hub",
        description: "Click 'Documentation' in the sidebar or visit /docs to read comprehensive guides for every platform service.",
        actionTip: "Use the category filters and search box in the Docs Hub to find API details in seconds.",
      },
    ],
    proTips: [
      "Switch between Void and Slate themes in the user menu to find your preferred dark-mode aesthetic.",
      "Check the real-time health indicator in the header to ensure all ecosystem daemons are running smoothly.",
    ],
    keyboardShortcuts: [
      { key: "Cmd + k", action: "Open Command Palette / Global Search" },
      { key: "Cmd + /", action: "Open Platform Documentation Hub" },
      { key: "Esc", action: "Close drawers, modals, and palettes" },
    ],
    docSectionId: "nexus-dashboard",
  },

  "nexus-files": {
    id: "nexus-files",
    name: "Nexus Files",
    tagline: "Sovereign cloud drive, folder trees, MinIO S3 storage, and secure file sharing",
    category: "Everyday",
    whatIsIt:
      "Nexus Files is a decentralized cloud drive and file explorer. It combines local SSD performance with MinIO S3 object storage for reliable file uploads, downloads, folder hierarchies, and versioned backups.",
    whenToUse:
      "Use Nexus Files to store, organize, and share documents, PDFs, zip archives, and client deliverables with zero tracking and full ownership of your data.",
    topCapabilities: [
      { title: "S3 & Disk Storage Engine", desc: "Dual-backend storage supporting local file caching and MinIO S3 object replication." },
      { title: "Hierarchical Folder Trees", desc: "Organize files into directories with drag-and-drop moving and nesting." },
      { title: "File Versioning", desc: "Automatic snapshots of previous revisions when overwriting files." },
      { title: "Secure Direct Download", desc: "Fast presigned URLs for rapid file retrieval without heavy server memory overhead." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Create Folders",
        description: "Open Nexus Files and click '+ New Folder' to structure your personal or shared workspace.",
        actionTip: "Create separate folders for documents, designs, and project archives.",
      },
      {
        step: 2,
        title: "Upload Files",
        description: "Drag and drop files from your desktop directly into any folder.",
        actionTip: "Uploads stream in parallel to MinIO S3 with real-time transfer progress.",
      },
      {
        step: 3,
        title: "Share and Download",
        description: "Click any file to preview or click 'Download' to retrieve the original lossless file.",
        actionTip: "Generate temporary presigned links to share large files securely with external clients.",
      },
    ],
    proTips: [
      "Files are soft-deleted first so you can restore accidentally removed items from Trash.",
      "Check file metadata and storage size breakdown in the details drawer.",
    ],
    keyboardShortcuts: [
      { key: "u", action: "Upload file" },
      { key: "Shift + n", action: "New folder" },
      { key: "Delete", action: "Move to Trash" },
    ],
    docSectionId: "nexus-files",
  },

  "nexus-finance": {
    id: "nexus-finance",
    name: "Nexus Finance",
    tagline: "Sovereign accounting, double-entry bookkeeping, expense tracking, and financial analytics",
    category: "Work & Docs",
    whatIsIt:
      "Nexus Finance is a sovereign personal and business accounting ledger. It records income, expenses, invoices, and tax categories with ACID transactional integrity in SQLite, computing cash flow and category summaries instantly.",
    whenToUse:
      "Use Nexus Finance to track business expenses, reconcile invoices, monitor cash burn, and prepare tax reports without uploading sensitive financial figures to third-party SaaS vendors.",
    topCapabilities: [
      { title: "SQLite Double-Entry Ledger", desc: "Crash-safe transactional records of all income and expense flows." },
      { title: "Expense Categorization", desc: "Categorize expenses (Software, Hosting, Legal, Marketing) with instant summary rollups." },
      { title: "Cash Flow Analytics", desc: "Visualize monthly income vs expense trends and net margins." },
      { title: "Receipt & Invoice Archival", desc: "Attach receipts and PDFs directly to ledger transaction entries." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Record an Expense or Income",
        description: "Click '+ New Transaction'. Enter amount, description, vendor, and category.",
        actionTip: "Set date and payment method to keep accurate chronological records.",
      },
      {
        step: 2,
        title: "Review Category Breakdown",
        description: "Inspect the category pie chart to see where team or personal capital is being allocated.",
        actionTip: "Filter by month or quarter to evaluate budget compliance.",
      },
      {
        step: 3,
        title: "Export Financial Reports",
        description: "Click 'Export CSV' to generate spreadsheet reports ready for your accountant or tax filing.",
        actionTip: "Reports include full ledger details with zero tracking.",
      },
    ],
    proTips: [
      "Set recurring expense reminders to never miss subscription renewal dates.",
      "All financial calculations run locally with precision fixed-point arithmetic.",
    ],
    keyboardShortcuts: [
      { key: "n", action: "New transaction" },
      { key: "e", action: "Export ledger CSV" },
    ],
    docSectionId: "nexus-finance",
  },

  "nexus-tasks": {
    id: "nexus-tasks",
    name: "Nexus Tasks",
    tagline: "Minimalist task checklists, daily punchlists, priorities, and assignments",
    category: "Everyday",
    whatIsIt:
      "Nexus Tasks is a streamlined, distraction-free checklist and punchlist tool. It features multi-list management (Work, Personal, Errands), priority badges (Urgent, High, Normal), due dates, and instantaneous checkbox toggling.",
    whenToUse:
      "Use Nexus Tasks for quick daily to-do capture, packing checklists, sprint punchlists, and personal reminders that don't require the full complexity of Nexus Project.",
    topCapabilities: [
      { title: "Instant Fast Capture", desc: "Type task title and press Enter to instantly add tasks to your active list." },
      { title: "Multi-List Organization", desc: "Keep work deliverables, home errands, and shopping lists strictly separated." },
      { title: "Priority & Due Dates", desc: "Flag high-priority items and set due dates with countdown indicators." },
      { title: "Sub-millisecond Checkbox Toggle", desc: "Optimistic UI toggling that feels instantaneous on any device." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Create a Task List",
        description: "Click '+ New List' in the sidebar (e.g. 'Today's Priorities' or 'Release Checklist').",
        actionTip: "Lists help you compartmentalize different focus areas.",
      },
      {
        step: 2,
        title: "Add Daily Punchlist Items",
        description: "Type in the top input box and press Enter. Add priority and target deadline.",
        actionTip: "Press Enter repeatedly to rapid-fire add a list of items.",
      },
      {
        step: 3,
        title: "Check Off and Archive",
        description: "Click the checkbox to complete tasks. Completed items move to the bottom with subtle strikethrough.",
        actionTip: "Click 'Clear Completed' at the end of the day to reset your workspace.",
      },
    ],
    proTips: [
      "Use keyboard shortcuts (Cmd+N) to add a task from anywhere in the app.",
      "Tasks sync instantly across all your devices via the Nexus Auth session.",
    ],
    keyboardShortcuts: [
      { key: "Enter", action: "Add task" },
      { key: "j / k", action: "Navigate up / down" },
      { key: "Space", action: "Toggle task completion" },
    ],
    docSectionId: "nexus-tasks",
  },

  "nexus-music": {
    id: "nexus-music",
    name: "Nexus Music",
    tagline: "Sovereign audio player, streaming music library, playlists, and track metadata",
    category: "Create",
    whatIsIt:
      "Nexus Music is a private audio streaming player and media catalog. It catalogs your audio files (FLAC, MP3, WAV, AAC), reads artist and album tags, manages custom playlists, and streams audio directly from private S3 buckets.",
    whenToUse:
      "Use Nexus Music to listen to your personal audio collection, background focus beats, podcast episodes, and sound design assets without streaming compression or audio ads.",
    topCapabilities: [
      { title: "Persistent Web Audio Player", desc: "Play, pause, skip, seek, shuffle, and volume controls accessible across all views." },
      { title: "Playlist Curator", desc: "Build thematic playlists and reorder tracks with simple drag-and-drop." },
      { title: "Lossless Audio Streaming", desc: "Stream high-bitrate FLAC and MP3 audio directly from sovereign storage." },
      { title: "ID3 Metadata & Genre Index", desc: "Browse music by artist, album, duration, and musical genre." },
    ],
    tutorialSteps: [
      {
        step: 1,
        title: "Add Tracks to Your Library",
        description: "Click '+ Add Track' and upload your audio files or add track URLs with title and artist.",
        actionTip: "Audio files are automatically scanned for metadata duration and bitrate.",
      },
      {
        step: 2,
        title: "Create a Playlist",
        description: "Click '+ New Playlist' (e.g. 'Deep Work Focus') and add your favorite tracks.",
        actionTip: "Playlists automatically queue tracks in order for uninterrupted listening.",
      },
      {
        step: 3,
        title: "Play and Enjoy",
        description: "Click any track to start streaming. The persistent player bar at the bottom keeps playing as you work.",
        actionTip: "Press Spacebar to toggle playback from anywhere in the app.",
      },
    ],
    proTips: [
      "Supports Web Audio API for rich audio processing and zero distortion.",
      "Audio files can be stored in private MinIO S3 buckets for fast streaming.",
    ],
    keyboardShortcuts: [
      { key: "Space", action: "Play / Pause" },
      { key: "Right Arrow", action: "Next track" },
      { key: "Left Arrow", action: "Previous track" },
      { key: "m", action: "Mute / Unmute" },
    ],
    docSectionId: "nexus-music",
  },
};

export function getAppGuide(appId: string): AppGuide | undefined {
  const normalized = appId.toLowerCase().replace(/^(nexus-)?/, "nexus-");
  return APP_GUIDES[normalized] || APP_GUIDES[appId];
}
