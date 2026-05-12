$ErrorActionPreference = "Stop"

$projectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$assetDir = Join-Path $projectRoot "src\PcActivityTracker.App\Assets"
$icoPath = Join-Path $assetDir "PcActivityTracker.ico"
$previewPath = Join-Path $assetDir "PcActivityTracker-256.png"

New-Item -ItemType Directory -Force -Path $assetDir | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies "System.Drawing.dll" -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;

public static class PcActivityTrackerIconBuilder
{
    public static void SaveIcon(string icoPath, string previewPath)
    {
        int[] sizes = new[] { 16, 20, 24, 32, 40, 48, 64, 128, 256 };
        var frames = new List<IconFrame>();

        foreach (int size in sizes)
        {
            byte[] png = RenderPng(size);
            frames.Add(new IconFrame(size, png));

            if (size == 256)
            {
                File.WriteAllBytes(previewPath, png);
            }
        }

        using (var stream = File.Create(icoPath))
        using (var writer = new BinaryWriter(stream))
        {
            writer.Write((ushort)0);
            writer.Write((ushort)1);
            writer.Write((ushort)frames.Count);

            int offset = 6 + frames.Count * 16;
            foreach (IconFrame frame in frames)
            {
                writer.Write((byte)(frame.Size >= 256 ? 0 : frame.Size));
                writer.Write((byte)(frame.Size >= 256 ? 0 : frame.Size));
                writer.Write((byte)0);
                writer.Write((byte)0);
                writer.Write((ushort)1);
                writer.Write((ushort)32);
                writer.Write((uint)frame.Png.Length);
                writer.Write((uint)offset);
                offset += frame.Png.Length;
            }

            foreach (IconFrame frame in frames)
            {
                writer.Write(frame.Png);
            }
        }
    }

    private static byte[] RenderPng(int size)
    {
        using (var bitmap = new Bitmap(size, size, PixelFormat.Format32bppArgb))
        using (var graphics = Graphics.FromImage(bitmap))
        {
            graphics.SmoothingMode = SmoothingMode.AntiAlias;
            graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;
            graphics.CompositingQuality = CompositingQuality.HighQuality;
            graphics.Clear(Color.Transparent);

            float s = size;
            float scale = Math.Max(1.0f, s / 64.0f);
            RectangleF background = new RectangleF(s * 0.075f, s * 0.075f, s * 0.85f, s * 0.85f);

            using (GraphicsPath backgroundPath = RoundedRect(background, s * 0.18f))
            using (var backgroundBrush = new LinearGradientBrush(
                background,
                Color.FromArgb(255, 18, 58, 74),
                Color.FromArgb(255, 14, 165, 168),
                45.0f))
            using (var borderPen = new Pen(Color.FromArgb(42, 255, 255, 255), Math.Max(1.0f, s * 0.025f)))
            {
                graphics.FillPath(backgroundBrush, backgroundPath);
                graphics.DrawPath(borderPen, backgroundPath);
            }

            RectangleF screen = new RectangleF(s * 0.20f, s * 0.27f, s * 0.61f, s * 0.40f);
            using (GraphicsPath screenPath = RoundedRect(screen, s * 0.075f))
            using (var screenBrush = new LinearGradientBrush(
                screen,
                Color.FromArgb(255, 255, 255, 255),
                Color.FromArgb(255, 221, 247, 244),
                90.0f))
            using (var screenPen = new Pen(Color.FromArgb(48, 11, 37, 48), Math.Max(1.0f, s * 0.022f)))
            {
                graphics.FillPath(screenBrush, screenPath);
                graphics.DrawPath(screenPen, screenPath);
            }

            PointF[] pulse = new[]
            {
                new PointF(s * 0.28f, s * 0.50f),
                new PointF(s * 0.37f, s * 0.50f),
                new PointF(s * 0.42f, s * 0.40f),
                new PointF(s * 0.50f, s * 0.60f),
                new PointF(s * 0.58f, s * 0.47f),
                new PointF(s * 0.70f, s * 0.47f)
            };

            using (var glowPen = new Pen(Color.FromArgb(76, 22, 214, 123), Math.Max(2.0f, s * 0.082f)))
            using (var pulsePen = new Pen(Color.FromArgb(255, 22, 214, 123), Math.Max(1.4f, s * 0.052f)))
            using (var highlightPen = new Pen(Color.FromArgb(228, 234, 255, 243), Math.Max(1.0f, s * 0.018f)))
            {
                glowPen.StartCap = glowPen.EndCap = LineCap.Round;
                glowPen.LineJoin = LineJoin.Round;
                pulsePen.StartCap = pulsePen.EndCap = LineCap.Round;
                pulsePen.LineJoin = LineJoin.Round;
                highlightPen.StartCap = highlightPen.EndCap = LineCap.Round;
                highlightPen.LineJoin = LineJoin.Round;

                graphics.DrawLines(glowPen, pulse);
                graphics.DrawLines(pulsePen, pulse);
                if (size >= 32)
                {
                    graphics.DrawLines(highlightPen, pulse);
                }
            }

            using (var darkBrush = new SolidBrush(Color.FromArgb(255, 11, 37, 48)))
            {
                graphics.FillRoundedRectangle(darkBrush, new RectangleF(s * 0.45f, s * 0.665f, s * 0.10f, s * 0.095f), s * 0.03f);
                graphics.FillRoundedRectangle(darkBrush, new RectangleF(s * 0.34f, s * 0.745f, s * 0.32f, s * 0.065f), s * 0.032f);
            }

            float radius = s * 0.135f;
            RectangleF clock = new RectangleF(s * 0.705f - radius, s * 0.70f - radius, radius * 2.0f, radius * 2.0f);
            using (var clockBrush = new SolidBrush(Color.FromArgb(255, 255, 200, 87)))
            using (var clockPen = new Pen(Color.FromArgb(255, 11, 37, 48), Math.Max(1.0f, s * 0.033f)))
            using (var handPen = new Pen(Color.FromArgb(255, 11, 37, 48), Math.Max(1.0f, s * 0.032f)))
            {
                graphics.FillEllipse(clockBrush, clock);
                graphics.DrawEllipse(clockPen, clock);
                handPen.StartCap = handPen.EndCap = LineCap.Round;
                graphics.DrawLine(handPen, s * 0.705f, s * 0.635f, s * 0.705f, s * 0.705f);
                graphics.DrawLine(handPen, s * 0.705f, s * 0.705f, s * 0.765f, s * 0.705f);
            }

            using (var stream = new MemoryStream())
            {
                bitmap.Save(stream, ImageFormat.Png);
                return stream.ToArray();
            }
        }
    }

    private static GraphicsPath RoundedRect(RectangleF bounds, float radius)
    {
        float diameter = radius * 2.0f;
        var path = new GraphicsPath();

        path.AddArc(bounds.Left, bounds.Top, diameter, diameter, 180.0f, 90.0f);
        path.AddArc(bounds.Right - diameter, bounds.Top, diameter, diameter, 270.0f, 90.0f);
        path.AddArc(bounds.Right - diameter, bounds.Bottom - diameter, diameter, diameter, 0.0f, 90.0f);
        path.AddArc(bounds.Left, bounds.Bottom - diameter, diameter, diameter, 90.0f, 90.0f);
        path.CloseFigure();

        return path;
    }

    private sealed class IconFrame
    {
        public IconFrame(int size, byte[] png)
        {
            Size = size;
            Png = png;
        }

        public int Size { get; private set; }
        public byte[] Png { get; private set; }
    }
}

public static class GraphicsExtensions
{
    public static void FillRoundedRectangle(this Graphics graphics, Brush brush, RectangleF bounds, float radius)
    {
        using (GraphicsPath path = RoundedRect(bounds, radius))
        {
            graphics.FillPath(brush, path);
        }
    }

    private static GraphicsPath RoundedRect(RectangleF bounds, float radius)
    {
        float diameter = radius * 2.0f;
        var path = new GraphicsPath();

        path.AddArc(bounds.Left, bounds.Top, diameter, diameter, 180.0f, 90.0f);
        path.AddArc(bounds.Right - diameter, bounds.Top, diameter, diameter, 270.0f, 90.0f);
        path.AddArc(bounds.Right - diameter, bounds.Bottom - diameter, diameter, diameter, 0.0f, 90.0f);
        path.AddArc(bounds.Left, bounds.Bottom - diameter, diameter, diameter, 90.0f, 90.0f);
        path.CloseFigure();

        return path;
    }
}
"@

[PcActivityTrackerIconBuilder]::SaveIcon($icoPath, $previewPath)

Write-Host "Wrote $icoPath"
Write-Host "Wrote $previewPath"
