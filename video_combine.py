import sys
from moviepy.editor import VideoFileClip, clips_array

def combine_videos_grid(video_paths):
    num_columns = 2
    num_rows = -(-len(video_paths) // num_columns)  # Ceil division
    clips = [VideoFileClip(path) for path in video_paths]
    grid = clips_array([clips[i:i+num_columns] for i in range(0, len(clips), num_columns)])
    
    final_clip = grid.resize(width=grid.w // num_columns)
    return final_clip

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python script.py video_path1 video_path2 ...")
        sys.exit(1)
    
    video_paths = sys.argv[1:]
    output_video = combine_videos_grid(video_paths)
    output_video.write_videofile("output_grid.mp4", codec="libx264")
