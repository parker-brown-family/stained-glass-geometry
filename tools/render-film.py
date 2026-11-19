import argparse
import json
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

SEASONS = ("Summer solstice", "Equinox", "Winter solstice")
SECONDS_PER_SEASON = 12
FPS = 30


def encode_chapter(renderer, season, work):
    chapter = work / f"chapter-{season}.mp4"
    pixel_format = "bgr0" if sys.byteorder == "little" else "0rgb"
    with subprocess.Popen([renderer, str(season)], stdout=subprocess.PIPE, cwd=work) as source:
        try:
            subprocess.run(
                [
                    "ffmpeg", "-hide_banner", "-loglevel", "error",
                    "-filter_threads", "1", "-f", "rawvideo", "-pixel_format", pixel_format,
                    "-video_size", "1920x1080", "-framerate", str(FPS), "-i", "pipe:0",
                    "-c:v", "libx264", "-threads", "3", "-pix_fmt", "yuv420p", "-crf", "18",
                    "-preset", "fast", "-g", "60", chapter,
                ],
                stdin=source.stdout,
                check=True,
            )
        finally:
            source.stdout.close()
        if source.wait():
            raise RuntimeError(f"Renderer failed for {SEASONS[season]}")
    return chapter


def join_chapters(chapters, work):
    order = work / "chapters.ffconcat"
    order.write_text("".join(f"file '{chapter.name}'\n" for chapter in chapters))
    metadata = work / "chapters.ffmeta"
    tags = [";FFMETADATA1", "title=Stained Glass Geometry"]
    for i, title in enumerate(SEASONS):
        start = i * SECONDS_PER_SEASON * 1000
        end = start + SECONDS_PER_SEASON * 1000
        tags.extend(("", "[CHAPTER]", "TIMEBASE=1/1000", f"START={start}", f"END={end}", f"title={title}"))
    metadata.write_text("\n".join(tags) + "\n")
    film = work / "film.mp4"
    subprocess.run(
        [
            "ffmpeg", "-hide_banner", "-loglevel", "error", "-f", "concat", "-safe", "1",
            "-i", order, "-f", "ffmetadata", "-i", metadata,
            "-map", "0:v:0", "-map_metadata", "1", "-map_chapters", "1",
            "-c", "copy", "-movflags", "+faststart", film,
        ],
        check=True,
    )
    return film


def check_film(film):
    report = json.loads(subprocess.check_output(
        [
            "ffprobe", "-v", "error", "-select_streams", "v:0", "-show_chapters",
            "-show_entries", "stream=nb_frames,avg_frame_rate:format=duration",
            "-of", "json", film,
        ],
        text=True,
    ))
    seconds = len(SEASONS) * SECONDS_PER_SEASON
    stream = report["streams"][0]
    if float(report["format"]["duration"]) != seconds or int(stream["nb_frames"]) != seconds * FPS:
        raise RuntimeError(f"Expected a {seconds}-second film at {FPS} fps")
    if stream["avg_frame_rate"] != f"{FPS}/1":
        raise RuntimeError(f"Expected a constant {FPS} fps")
    chapters = [
        (float(chapter["start_time"]), float(chapter["end_time"]), chapter["tags"]["title"])
        for chapter in report["chapters"]
    ]
    expected = [
        (i * SECONDS_PER_SEASON, (i + 1) * SECONDS_PER_SEASON, name)
        for i, name in enumerate(SEASONS)
    ]
    if chapters != expected:
        raise RuntimeError("Unexpected chapter timings or titles")


def main():
    parser = argparse.ArgumentParser(description="Render the three seasonal days into one MP4.")
    parser.add_argument("renderer", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    renderer = args.renderer.resolve()
    output = args.output.resolve()
    with TemporaryDirectory(prefix="film-", dir=output.parent) as scratch:
        work = Path(scratch)
        chapters = []
        for season, name in enumerate(SEASONS):
            print(f"Rendering {name.lower()}", flush=True)
            chapters.append(encode_chapter(renderer, season, work))
        film = join_chapters(chapters, work)
        check_film(film)
        film.replace(output)
    print(output)


if __name__ == "__main__":
    main()
