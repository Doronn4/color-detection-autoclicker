import subprocess
import time
import json
import re
from typing import List, Tuple
from csscolor import parse
import cv2
import numpy as np

def rgb_to_opencv_hsv(*rgb):
    """
    Convert an RGB color to OpenCV's HSV color format.

    Parameters:
        rgb (tuple): A tuple representing the RGB color (R, G, B), where each value is in the range [0, 255].

    Returns:
        tuple: A tuple representing the HSV color (H, S, V), where:
            H is in the range [0, 179],
            S is in the range [0, 255],
            V is in the range [0, 255].
    """
    # Convert the RGB color to a NumPy array
    rgb_array = np.array([[rgb]], dtype=np.uint8)

    # Convert the RGB array to HSV
    hsv_array = cv2.cvtColor(rgb_array, cv2.COLOR_RGB2HSV)

    # Return the HSV values as a tuple
    return tuple(hsv_array[0][0])

def create_color_range(hsv: Tuple[float, float, float], range_size: float = 5) -> Tuple[Tuple[float, float, float], Tuple[float, float, float]]:
    h, s, v = hsv
    h_low = max(0, h - range_size / 2)
    h_high = min(180, h + range_size / 2)
    s_low = max(0, s - range_size)
    s_high = min(255, s + range_size)
    v_low = max(0, v - range_size)
    v_high = min(255, v + range_size)
    return ((h_low, s_low, v_low), (h_high, s_high, v_high))

def parse_and_convert_colors(colors: List[str]) -> List[Tuple[Tuple[float, float, float], Tuple[float, float, float]]]:
    converted_colors = []
    for color in colors:
        rgb = parse.color(color).as_int_triple()
        hsv = rgb_to_opencv_hsv(*rgb)
        converted_colors.append(create_color_range(hsv))
    return converted_colors

def run_simulator(config_file):
    return subprocess.Popen(
        ["python", "simulator.py", "--config", config_file],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )

def run_autoclicker(window_name, duration, good_colors, bad_colors, threads=10, max_distance=180,
                    min_click_distance=8, max_clicked_time=200, target_fps=120, min_rect_area=900):
    good_color_args = []
    for color_range in good_colors:
        good_color_args.extend(["--good-colors", f"{color_range[0][0]},{color_range[0][1]},{color_range[0][2]},{color_range[1][0]},{color_range[1][1]},{color_range[1][2]}"])
    
    bad_color_args = ["--bad-color", f"{bad_colors[0][0][0]},{bad_colors[0][0][1]},{bad_colors[0][0][2]},{bad_colors[0][1][0]},{bad_colors[0][1][1]},{bad_colors[0][1][2]}"]
    
    command = [
        "./bin/autoclicker.exe",
        "--window", window_name,
        "--runtime", str(duration),
        "--threads", str(threads),
        "--max-distance", str(max_distance),
        "--min-click-distance", str(min_click_distance),
        "--max-clicked-time", str(max_clicked_time),
        "--target-fps", str(target_fps),
        "--min-rect-area", str(min_rect_area)
    ] + good_color_args + bad_color_args
    
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    return process

def parse_game_output(output):
    good_clicks = int(re.search(r"Good clicks: (\d+)", output).group(1))
    bad_clicks = int(re.search(r"Bad clicks: (\d+)", output).group(1))
    return good_clicks, bad_clicks

def parse_autoclicker_output(output):
    total_runtime = float(re.search(r"Total runtime: ([\d.]+) seconds", output).group(1))
    total_clicks = int(re.search(r"Total clicks: (\d+)", output).group(1))
    avg_clicks_per_second = float(re.search(r"Average clicks per second: ([\d.]+)", output).group(1))
    return total_runtime, total_clicks, avg_clicks_per_second

def run_test(config_file, window_name, autoclicker_config=None):
    print("Starting automated test...")
    
    # Load game configuration
    with open(config_file, 'r') as f:
        game_config = json.load(f)
    
    # Parse and convert colors
    good_colors = parse_and_convert_colors(game_config['good_colors'])
    bad_colors = parse_and_convert_colors(game_config['bad_colors'])
    print("Good colors:", good_colors)
    print("Bad colors:", bad_colors)
    
    # Start the simulator
    print("Launching simulator...")
    simulator_process = run_simulator(config_file)
    
    # Give the simulator some time to start up
    time.sleep(2)
    
    # Start the autoclicker with custom configuration if provided
    print("Starting autoclicker...")
    if autoclicker_config is None:
        autoclicker_config = {}
    
    autoclicker_process = run_autoclicker(
        window_name,
        game_config['duration'],
        good_colors,
        bad_colors,
        **autoclicker_config
    )
    
    # Wait for the autoclicker to finish
    autoclicker_output, _ = autoclicker_process.communicate()
    
    # Stop the simulator
    simulator_output, _ = simulator_process.communicate()
    
    # Parse outputs
    good_clicks, bad_clicks = parse_game_output(simulator_output)
    total_runtime, total_clicks, avg_clicks_per_second = parse_autoclicker_output(autoclicker_output)
    
    # Display statistics
    print("\nTest Results:")
    print(f"Simulator runtime: {game_config['duration']} seconds")
    print(f"Autoclicker runtime: {total_runtime:.2f} seconds")
    print(f"Good clicks: {good_clicks}")
    print(f"Bad clicks: {bad_clicks}")
    print(f"Total clicks (autoclicker): {total_clicks}")
    print(f"Average clicks per second: {avg_clicks_per_second:.2f}")
    
    accuracy = good_clicks / (good_clicks + bad_clicks) if (good_clicks + bad_clicks) > 0 else 0
    print(f"Accuracy: {accuracy:.2%}")

if __name__ == "__main__":
    simulator_config_file = "config.json"
    window_name = "Circle Click Game"
    
    # Example of custom autoclicker configuration
    autoclicker_config = {
        "threads": 12,
        "max_distance": 50,
        "min_click_distance": 10,
        "max_clicked_time": 250,
        "target_fps": 144,
        "min_rect_area": 31 * 31
    }
    
    run_test(simulator_config_file, window_name, autoclicker_config)
