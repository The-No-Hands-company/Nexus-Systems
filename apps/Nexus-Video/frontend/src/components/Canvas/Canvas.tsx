import { useEffect, useRef } from "react";

export default function Canvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    const resize = () => {
      canvas.width = canvas.clientWidth * devicePixelRatio;
      canvas.height = canvas.clientHeight * devicePixelRatio;
      ctx.scale(devicePixelRatio, devicePixelRatio);
      draw();
    };

    const draw = () => {
      const w = canvas.clientWidth;
      const h = canvas.clientHeight;
      ctx.clearRect(0, 0, w, h);

      ctx.fillStyle = "#18181b";
      ctx.fillRect(0, 0, w, h);

      const bars = 30;
      const barW = (w - 20) / bars;
      const gap = 2;

      for (let i = 0; i < bars; i++) {
        const peak = 0.2 + Math.sin(i * 0.5 + Date.now() * 0.002) * 0.3 + 0.3;
        const barH = peak * (h - 20);
        const x = 10 + i * barW;
        const y = h - 10 - barH;

        const grad = ctx.createLinearGradient(x, y, x, h - 10);
        grad.addColorStop(0, "#3b82f6");
        grad.addColorStop(1, "#1d4ed8");
        ctx.fillStyle = grad;
        ctx.fillRect(x + gap / 2, y, barW - gap, barH);
      }
    };

    resize();
    const interval = setInterval(draw, 50);
    window.addEventListener("resize", resize);
    return () => {
      clearInterval(interval);
      window.removeEventListener("resize", resize);
    };
  }, []);

  return (
    <canvas
      ref={canvasRef}
      className="w-full h-32 rounded-lg"
    />
  );
}
