package renpy

import (
	"bytes"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"time"
)

type Ffmpeg struct {
	Path string
}

func NewFfmpeg(explicit string) (*Ffmpeg, error) {
	p := resolveFfmpeg(explicit)
	if p == "" {
		return nil, fmt.Errorf("ffmpeg not found. Put ffmpeg(.exe) next to the tool, add it to PATH, or pass --ffmpeg <path>")
	}
	return &Ffmpeg{Path: p}, nil
}

func ffmpegExeName() string {
	if runtime.GOOS == "windows" {
		return "ffmpeg.exe"
	}
	return "ffmpeg"
}

func resolveFfmpeg(p string) string {
	if p != "" {
		if st, err := os.Stat(p); err == nil && !st.IsDir() {
			return p
		}
	}
	exe := ffmpegExeName()
	if execPath, err := os.Executable(); err == nil {
		dir := filepath.Dir(execPath)
		candidates := []string{
			filepath.Join(dir, exe),
			filepath.Join(dir, "..", exe),
			filepath.Join(dir, "..", "..", exe),
		}
		for _, c := range candidates {
			if st, err := os.Stat(c); err == nil && !st.IsDir() {
				abs, _ := filepath.Abs(c)
				return abs
			}
		}
	}
	if wd, err := os.Getwd(); err == nil {
		c := filepath.Join(wd, exe)
		if st, err := os.Stat(c); err == nil && !st.IsDir() {
			return c
		}
	}
	if found, err := exec.LookPath(exe); err == nil {
		return found
	}
	if runtime.GOOS != "windows" {
		if found, err := exec.LookPath("ffmpeg"); err == nil {
			return found
		}
	}
	return ""
}

func (ff *Ffmpeg) Run(args []string, timeout time.Duration) (ok bool, errText string) {
	if timeout <= 0 {
		timeout = 10 * time.Minute
	}
	cmd := exec.Command(ff.Path, args...)
	var stderr bytes.Buffer
	cmd.Stdout = nil
	cmd.Stderr = &stderr
	if err := cmd.Start(); err != nil {
		return false, err.Error()
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	select {
	case <-time.After(timeout):
		_ = cmd.Process.Kill()
		return false, "ffmpeg timed out"
	case err := <-done:
		errText = stderr.String()
		return err == nil, errText
	}
}

func scaleFilter(maxW, maxH int) string {
	return "scale='min(iw," + strconv.Itoa(maxW) + ")':'min(ih," + strconv.Itoa(maxH) + ")':force_original_aspect_ratio=decrease"
}

func uniformScale(factor float64, even bool) string {
	f := strconv.FormatFloat(factor, 'f', 6, 64)
	f = strings.TrimRight(strings.TrimRight(f, "0"), ".")
	if even {
		return "scale=trunc(iw*" + f + "/2)*2:trunc(ih*" + f + "/2)*2"
	}
	return "scale=trunc(iw*" + f + "):trunc(ih*" + f + ")"
}

func (ff *Ffmpeg) Image(inp, outp string, maxW, maxH int) (bool, string) {
	return ff.Run([]string{"-y", "-hide_banner", "-loglevel", "error", "-i", inp, "-vf", scaleFilter(maxW, maxH), outp}, 10*time.Minute)
}

func (ff *Ffmpeg) ImageScaled(inp, outp string, factor float64) (bool, string) {
	if factor >= 1.0 {
		return ff.Run([]string{"-y", "-hide_banner", "-loglevel", "error", "-i", inp, outp}, 10*time.Minute)
	}
	return ff.Run([]string{"-y", "-hide_banner", "-loglevel", "error", "-i", inp, "-vf", uniformScale(factor, false), outp}, 10*time.Minute)
}

func (ff *Ffmpeg) Audio(inp, outp string) (bool, string) {
	return ff.Run([]string{"-y", "-hide_banner", "-loglevel", "error", "-i", inp,
		"-c:a", "libvorbis", "-q:a", "4", "-ar", "48000", "-ac", "2", outp}, 10*time.Minute)
}

func (ff *Ffmpeg) Video(inp, outp string, maxW, maxH int) (bool, string) {
	return ff.Run([]string{"-y", "-hide_banner", "-loglevel", "error", "-i", inp,
		"-c:v", "libx264", "-profile:v", "main", "-level", "4.0", "-pix_fmt", "yuv420p",
		"-preset", "veryfast", "-crf", "23", "-vf", scaleFilter(maxW, maxH),
		"-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", outp}, 30*time.Minute)
}

func (ff *Ffmpeg) VideoScaled(inp, outp string, factor float64) (bool, string) {
	f := factor
	if f >= 1.0 {
		f = 1.0
	}
	return ff.Run([]string{"-y", "-hide_banner", "-loglevel", "error", "-i", inp,
		"-c:v", "libx264", "-profile:v", "main", "-level", "4.0", "-pix_fmt", "yuv420p",
		"-preset", "veryfast", "-crf", "23", "-vf", uniformScale(f, true),
		"-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", outp}, 30*time.Minute)
}

func ffmpegFingerprint(ff *Ffmpeg) string {
	st, err := os.Stat(ff.Path)
	if err == nil {
		return strconv.FormatInt(st.Size(), 10) + ":" + strconv.FormatInt(st.ModTime().UTC().UnixNano(), 10)
	}
	return ff.Path
}
