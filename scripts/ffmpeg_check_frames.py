"""
check_frames    - display frame information of a video
"""
import sys
import subprocess
import re
import dataclasses

#---
# Classes
#---

class FrameException(Exception):
    """ internal exception """

@dataclasses.dataclass(frozen=True)
class FrameInfo():
    """ basic frame info """
    id: int
    pts: int
    pts_time: float
    duration: int
    duration_time: float

@dataclasses.dataclass(frozen=True)
class VideoInfo():
    """ basic video info """
    size: str
    frames: str
    time: str

#---
# frames
#---

__ffmpeg_output: list[str] = []

def ffmpeg_info(video: str) -> list[str]:
    """ call ffmpeg if needed and return the output
    """
    global __ffmpeg_output
    if __ffmpeg_output:
        return __ffmpeg_output
    procinfo = subprocess.run(
        f"ffmpeg -hide_banner -i {video} -vf showinfo -f null -".split(' '),
        stdout = subprocess.PIPE,
        stderr = subprocess.STDOUT,
    )
    if not procinfo:
        raise FrameException('subprocess error')
    __ffmpeg_output = str(procinfo.stdout).split('\\n')
    return __ffmpeg_output

def frames_get(video: str) -> list[FrameInfo]:
    """ use ffmpeg and get information
    """
    frame_re = re.compile(
        r"^\[Parsed_showinfo_[0-9]+ @ 0x[0-9a-f]+] " +
        r"*n: *(?P<n>[0-9]+) *" +
        r"pts: *(?P<pts>[0-9]+) *" +
        r"pts_time: *(?P<pts_time>[0-9]+(\.[0-9]+)?) *" +
        r"duration: *(?P<duration>[0-9]+) *" +
        r"duration_time: *(?P<duration_time>[0-9]+(\.[0-9]+)?)"
    )
    frame_info: list[FrameInfo] = []
    for line in ffmpeg_info(video):
        if not (reginfo := frame_re.match(line)):
            continue
        frame_info.append(
            FrameInfo(
                id            = int(reginfo['n']),
                pts           = int(reginfo['pts']),
                pts_time      = float(reginfo['pts_time']),
                duration      = int(reginfo['duration']),
                duration_time = float(reginfo['duration_time']),
            )
        )
    return frame_info

def video_info_get(video: str) -> VideoInfo|None:
    """ return video information
    """
    video_re = re.compile(
        r"^\[out#[0-9]+/null @ 0x[0-9a-f]+] *" +
        r"video: *(?P<video>[0-9]+[a-zA-Z]+).+"
    )
    end_re = re.compile(
        r"frame= *(?P<frame>[0-9]+) *" +
        r"fps= *(?P<fps>[0-9]+(\.[0-9]+)?).+" +
        r"time= *(?P<time>([0-9]{2}:){2}[0-9]{2}(\.[0-9]+)?)"
    )
    test: dict[str,str] = {}
    for line in ffmpeg_info(video):
        if reginfo := video_re.match(line):
            test['size'] = reginfo['video']
            continue
        if reginfo := end_re.match(line):
            test['frames'] = reginfo['frame']
            test['time']   = reginfo['time']
            continue
    if 'size' not in test or 'frames' not in test or 'time' not in test:
        return None
    return VideoInfo(**test)

#---
# entry
#---

def main(argv: list[str]) -> None:
    """ CLI entry
    """
    if len(argv) < 2:
        raise FrameException(f"{argv[0]} <VIDEO_FILE>")
    if len(argv) >= 3 and argv[2] == '--raw':
        print("\n".join(line for line in ffmpeg_info(argv[1])))
        return
    frames = frames_get(argv[1])
    frame_len   = max([len(str(x.id)) for x in frames])
    pts_len     = max([len(str(x.pts)) for x in frames])
    dur_len     = max([len(str(x.duration)) for x in frames])
    pts_tim_len = max([len(str(x.pts_time)) for x in frames])
    dur_tim_len = max([len(str(x.duration_time)) for x in frames])
    frame_broken = 0
    frame_prev_pts = 0
    for frame in frames_get(argv[1]):
        print(
            f"[{frame.id: >{frame_len}}] " +
            f"pts: {frame.pts: >{pts_len}} | " +
            f"duration: {frame.duration: >{dur_len}} | " +
            f"pts_time: {frame.pts_time: <{pts_tim_len}} | " +
            f"dur_time: {frame.duration_time: <{dur_tim_len}} | " +
            f"{"✅" if frame_prev_pts == frame.pts else "❌"}"
        )
        frame_broken += (frame_prev_pts != frame.pts)
        frame_prev_pts = frame.pts + frame.duration
    if not (video := video_info_get(argv[1])):
        raise FrameException('missing end video info')
    print(
        f"size: {video.size} - " +
        f"time: {video.time} - " +
        f"frames: {video.frames} - " +
        f"broken: {frame_broken}"
    )

if __name__ == '__main__':
    try:
        main(sys.argv)
        sys.exit(0)
    except FrameException as err:
        print(f"\033[31m{err}\033[0m")
        sys.exit(1)
