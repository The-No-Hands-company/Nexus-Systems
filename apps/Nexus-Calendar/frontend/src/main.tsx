import { createRoot } from "react-dom/client";
import CalendarApp from "./CalendarApp";
import { calendarApi } from "./calendar-api";
import PublicEventPage from "./PublicEventPage";
import { resolveCalendarRuntime } from "./runtime";

const runtime = resolveCalendarRuntime();
const page = runtime.publicToken
  ? <PublicEventPage token={runtime.publicToken} />
  : <CalendarApp runtime={runtime} api={calendarApi(runtime.apiBase)} />;

createRoot(document.getElementById("root")!).render(page);
