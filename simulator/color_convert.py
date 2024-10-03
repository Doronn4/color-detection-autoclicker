import colorsys

def rgb_to_opencv_hsv(r, g, b):
    # Normalize RGB values to [0, 1] range for colorsys
    r_norm, g_norm, b_norm = r / 255.0, g / 255.0, b / 255.0
    
    # Convert RGB to normal HSV using colorsys
    hsv_normal = colorsys.rgb_to_hsv(r_norm, g_norm, b_norm)  # Returns H in [0, 1], S in [0, 1], V in [0, 1]
    
    # Convert normal HSV to OpenCV HSV
    h_opencv = hsv_normal[0] / 2  # OpenCV scales H to [0, 180]
    s_opencv = hsv_normal[1] * 255      # OpenCV scales S to [0, 255]
    v_opencv = hsv_normal[2] * 255      # OpenCV scales V to [0, 255]
    
    return (h_opencv, s_opencv, v_opencv)

# Test with sample colors
rgb_values = [
    (255, 255, 255),  # Good Color A
    (243, 208, 141),  # Good Color B
    (235, 106, 74),   # Bad Color
]

for rgb in rgb_values:
    hsv_opencv = rgb_to_opencv_hsv(*rgb)
    print(f"RGB: {rgb} -> OpenCV HSV: H: {hsv_opencv[0]:.2f}, S: {hsv_opencv[1]:.2f}, V: {hsv_opencv[2]:.2f}")
