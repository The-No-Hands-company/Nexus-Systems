import { useState } from "react";
import { Link } from "react-router-dom";
import type { AppEntry } from "../api";
import { getAppGuide } from "./appGuides";
import { appMeta } from "../components/appMeta";

export default function GuidedTutorial({
  app,
  onLaunch,
}: {
  app: AppEntry;
  onLaunch?: () => void;
}) {
  const guide = getAppGuide(app.id);
  const meta = appMeta(app.id);
  const Icon = meta.Icon;

  const [activeStep, setActiveStep] = useState(0);

  const steps = guide?.tutorialSteps || [
    {
      step: 1,
      title: "Explore the Interface",
      description: `Launch ${app.name} to familiarize yourself with its workspace layout and features.`,
      actionTip: "Click 'Launch App' to open the service in a dedicated tab.",
    },
    {
      step: 2,
      title: "Configure Preferences",
      description: "Set up your workspace settings, default categories, and notification rules.",
      actionTip: "Settings are synchronized across your sovereign devices.",
    },
    {
      step: 3,
      title: "Collaborate and Share",
      description: "Invite team members or export your assets with zero vendor lock-in.",
      actionTip: "All data is securely stored in local SQLite or private S3 storage.",
    },
  ];

  const currentStepData = steps[activeStep] || steps[0];

  return (
    <div className="mx-auto max-w-4xl space-y-6 px-4 py-8 sm:px-6">
      {/* Top Breadcrumb & Status */}
      <div className="flex flex-wrap items-center justify-between gap-3 border-b border-zinc-800/80 pb-4">
        <div className="flex items-center gap-2 text-xs font-medium text-zinc-400">
          <Link to="/" className="text-zinc-500 hover:text-zinc-300">
            Dashboard
          </Link>
          <span>/</span>
          <span className="text-zinc-500 capitalize">{guide?.category || app.primaryShelf}</span>
          <span>/</span>
          <span className="font-semibold text-zinc-200">{app.name}</span>
        </div>
        <div className="flex items-center gap-2">
          <span
            className={`inline-flex items-center gap-1.5 rounded-full px-2.5 py-0.5 text-xs font-medium ${
              app.health === "healthy"
                ? "bg-emerald-950/80 text-emerald-400 border border-emerald-800/50"
                : "bg-zinc-800 text-zinc-400"
            }`}
          >
            <span
              className={`h-1.5 w-1.5 rounded-full ${
                app.health === "healthy" ? "bg-emerald-400 animate-pulse" : "bg-zinc-500"
              }`}
            />
            {app.health === "healthy" ? "Service Live & Healthy" : "Offline"}
          </span>
          <span className="rounded-full bg-zinc-800/90 border border-zinc-700/60 px-2.5 py-0.5 text-xs font-medium text-zinc-300">
            v1.0 Production Grade
          </span>
        </div>
      </div>

      {/* Hero Header Area */}
      <div className="rounded-2xl border border-zinc-800 bg-gradient-to-b from-zinc-900/90 to-zinc-950 p-6 sm:p-8 shadow-xl">
        <div className="flex flex-col sm:flex-row items-start sm:items-center justify-between gap-6">
          <div className="flex items-center gap-4">
            <div
              className="flex h-16 w-16 shrink-0 items-center justify-center rounded-2xl border shadow-lg"
              style={{
                backgroundColor: "rgba(24, 24, 27, 0.8)",
                borderColor: meta.color ? `${meta.color}40` : "rgba(255, 255, 255, 0.1)",
              }}
            >
              <Icon size={32} weight="duotone" style={{ color: meta.color || "#10b981" }} />
            </div>
            <div>
              <div className="flex items-center gap-2">
                <h1 className="text-2xl sm:text-3xl font-bold tracking-tight text-zinc-100">
                  {app.name}
                </h1>
                <span className="rounded bg-accent/15 px-2 py-0.5 text-xs font-semibold uppercase tracking-wider text-accent border border-accent/30">
                  {guide?.category || app.primaryShelf}
                </span>
              </div>
              <p className="mt-1 text-sm sm:text-base text-zinc-400 font-normal">
                {guide?.tagline || app.description}
              </p>
            </div>
          </div>

          <div className="flex flex-wrap items-center gap-3 w-full sm:w-auto">
            {app.publicUrl ? (
              <a
                href={app.publicUrl}
                target="_blank"
                rel="noopener noreferrer"
                onClick={onLaunch}
                className="flex flex-1 sm:flex-initial items-center justify-center gap-2 rounded-xl bg-accent px-6 py-3 text-sm font-semibold text-accent-foreground shadow-lg transition-transform hover:scale-105 active:scale-95 focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-accent"
              >
                Launch {app.name} ↗
              </a>
            ) : app.path ? (
              <Link
                to={app.path}
                className="flex flex-1 sm:flex-initial items-center justify-center gap-2 rounded-xl bg-accent px-6 py-3 text-sm font-semibold text-accent-foreground shadow-lg transition-transform hover:scale-105 active:scale-95"
              >
                Open {app.name} →
              </Link>
            ) : null}
            <Link
              to={`/docs#${guide?.docSectionId || app.id}`}
              className="flex items-center justify-center gap-1.5 rounded-xl border border-zinc-700/80 bg-zinc-800/60 px-4 py-3 text-sm font-medium text-zinc-300 hover:bg-zinc-800 hover:text-zinc-100 transition-colors"
            >
              📖 Docs
            </Link>
          </div>
        </div>
      </div>

      {/* "What is it & When to use it" section */}
      {guide && (
        <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
          <div className="rounded-xl border border-zinc-800 bg-zinc-900/50 p-5">
            <div className="flex items-center gap-2 text-xs font-semibold uppercase tracking-wider text-sky-400">
              <span className="h-2 w-2 rounded-full bg-sky-400" />
              <span>What Is It?</span>
            </div>
            <p className="mt-2.5 text-sm leading-relaxed text-zinc-300">
              {guide.whatIsIt}
            </p>
          </div>

          <div className="rounded-xl border border-zinc-800 bg-zinc-900/50 p-5">
            <div className="flex items-center gap-2 text-xs font-semibold uppercase tracking-wider text-amber-400">
              <span className="h-2 w-2 rounded-full bg-amber-400" />
              <span>When To Use It?</span>
            </div>
            <p className="mt-2.5 text-sm leading-relaxed text-zinc-300">
              {guide.whenToUse}
            </p>
          </div>
        </div>
      )}

      {/* Interactive 3-Step Guided Tutorial */}
      <div className="rounded-2xl border border-zinc-800 bg-zinc-900/70 p-6 shadow-md">
        <div className="flex items-center justify-between border-b border-zinc-800/80 pb-4">
          <div>
            <span className="text-xs font-semibold uppercase tracking-wider text-accent">
              Guided Tutorial & Quick Start
            </span>
            <h2 className="mt-1 text-lg font-bold text-zinc-100">
              Step-by-Step Walkthrough
            </h2>
          </div>
          <div className="flex items-center gap-1.5">
            {steps.map((s, idx) => (
              <button
                key={s.step}
                type="button"
                onClick={() => setActiveStep(idx)}
                className={`flex h-7 w-7 items-center justify-center rounded-full text-xs font-bold transition-all ${
                  activeStep === idx
                    ? "bg-accent text-accent-foreground ring-2 ring-accent/40 scale-105"
                    : idx < activeStep
                    ? "bg-zinc-800 text-zinc-300 hover:bg-zinc-700"
                    : "bg-zinc-800/60 text-zinc-500 hover:text-zinc-300"
                }`}
                title={`Step ${s.step}: ${s.title}`}
              >
                {s.step}
              </button>
            ))}
          </div>
        </div>

        {/* Active Step Card */}
        <div className="mt-5 rounded-xl border border-zinc-800/80 bg-zinc-950/60 p-5 sm:p-6">
          <div className="flex items-center gap-2 text-xs font-semibold text-accent uppercase tracking-wider">
            <span>Step {currentStepData.step} of {steps.length}</span>
            <span>•</span>
            <span className="text-zinc-400">{currentStepData.title}</span>
          </div>

          <h3 className="mt-2 text-xl font-bold text-zinc-100">
            {currentStepData.title}
          </h3>

          <p className="mt-2 text-sm leading-relaxed text-zinc-300">
            {currentStepData.description}
          </p>

          <div className="mt-4 flex items-start gap-2.5 rounded-lg border border-accent/20 bg-accent/5 p-3.5 text-xs text-accent-foreground">
            <span className="font-bold uppercase tracking-wider text-accent shrink-0">Pro Tip:</span>
            <span className="text-zinc-300 leading-normal">{currentStepData.actionTip}</span>
          </div>

          {/* Stepper Navigation Buttons */}
          <div className="mt-6 flex items-center justify-between border-t border-zinc-800/60 pt-4">
            <button
              type="button"
              disabled={activeStep === 0}
              onClick={() => setActiveStep((prev) => Math.max(0, prev - 1))}
              className={`rounded-lg px-4 py-2 text-xs font-semibold transition-colors ${
                activeStep === 0
                  ? "opacity-30 cursor-not-allowed text-zinc-600 border border-transparent"
                  : "text-zinc-300 bg-zinc-800 hover:bg-zinc-700 border border-zinc-700"
              }`}
            >
              ← Previous Step
            </button>

            {activeStep < steps.length - 1 ? (
              <button
                type="button"
                onClick={() => setActiveStep((prev) => Math.min(steps.length - 1, prev + 1))}
                className="rounded-lg bg-zinc-100 px-4 py-2 text-xs font-semibold text-zinc-900 hover:bg-white shadow transition-colors"
              >
                Next Step: {steps[activeStep + 1]?.title} →
              </button>
            ) : app.publicUrl ? (
              <a
                href={app.publicUrl}
                target="_blank"
                rel="noopener noreferrer"
                onClick={onLaunch}
                className="rounded-lg bg-accent px-4 py-2 text-xs font-bold text-accent-foreground shadow hover:opacity-90 transition-opacity"
              >
                Launch {app.name} Now ↗
              </a>
            ) : (
              <Link
                to="/"
                className="rounded-lg bg-accent px-4 py-2 text-xs font-bold text-accent-foreground shadow hover:opacity-90 transition-opacity"
              >
                Back to Dashboard
              </Link>
            )}
          </div>
        </div>
      </div>

      {/* Top Capabilities 4-Grid */}
      {guide?.topCapabilities && guide.topCapabilities.length > 0 && (
        <div className="space-y-3">
          <h3 className="text-xs font-semibold uppercase tracking-wider text-zinc-400">
            Core Features & Architecture
          </h3>
          <div className="grid grid-cols-1 sm:grid-cols-2 gap-3">
            {guide.topCapabilities.map((cap, idx) => (
              <div
                key={idx}
                className="rounded-xl border border-zinc-800 bg-zinc-900/40 p-4 transition-colors hover:border-zinc-700"
              >
                <div className="flex items-center gap-2">
                  <div className="h-1.5 w-1.5 rounded-full bg-accent" />
                  <h4 className="text-sm font-semibold text-zinc-100">{cap.title}</h4>
                </div>
                <p className="mt-1 text-xs leading-relaxed text-zinc-400">{cap.desc}</p>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Pro Tips & Keyboard Shortcuts */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {guide?.proTips && guide.proTips.length > 0 && (
          <div className="rounded-xl border border-zinc-800 bg-zinc-900/40 p-5">
            <h4 className="text-xs font-semibold uppercase tracking-wider text-zinc-400">
              💡 Power User Tips
            </h4>
            <ul className="mt-3 space-y-2 text-xs text-zinc-300">
              {guide.proTips.map((tip, idx) => (
                <li key={idx} className="flex items-start gap-2">
                  <span className="text-accent">•</span>
                  <span>{tip}</span>
                </li>
              ))}
            </ul>
          </div>
        )}

        {guide?.keyboardShortcuts && guide.keyboardShortcuts.length > 0 && (
          <div className="rounded-xl border border-zinc-800 bg-zinc-900/40 p-5">
            <h4 className="text-xs font-semibold uppercase tracking-wider text-zinc-400">
              ⌨️ Keyboard Shortcuts
            </h4>
            <div className="mt-3 space-y-2">
              {guide.keyboardShortcuts.map((sc, idx) => (
                <div key={idx} className="flex items-center justify-between text-xs">
                  <span className="text-zinc-400">{sc.action}</span>
                  <kbd className="rounded border border-zinc-700 bg-zinc-800 px-2 py-0.5 font-mono text-[11px] font-semibold text-zinc-200">
                    {sc.key}
                  </kbd>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* Bottom Navigation */}
      <div className="flex items-center justify-between border-t border-zinc-800/80 pt-6">
        <Link
          to="/"
          className="text-xs font-medium text-zinc-400 hover:text-zinc-200 transition-colors"
        >
          ← Return to Dashboard
        </Link>
        <Link
          to={`/docs#${guide?.docSectionId || app.id}`}
          className="text-xs font-medium text-accent hover:underline"
        >
          Read Complete Platform Docs for {app.name} →
        </Link>
      </div>
    </div>
  );
}
