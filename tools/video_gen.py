import sys
import numpy as np
from PIL import Image
import cv2

TARGET_WIDTH = 320
TARGET_HEIGHT = 180
TARGET_FPS = 15
MAX_COLORS = 64

def quantize_np(image, n_colors=64):
    img_rgb = image.convert("RGB")
    img_np = np.asarray(img_rgb).reshape(-1, 3)

    kmeans = KMeans(n_clusters=n_colors, n_init="auto").fit(img_np)
    palette = np.uint8(kmeans.cluster_centers_)
    labels = kmeans.predict(img_np)
    
    quantized_np = palette[labels].reshape(img_rgb.size[1], img_rgb.size[0], 3)
    return Image.fromarray(quantized_np, mode="RGB")

def decode_video_to_frames(video_path):
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        raise RuntimeError(f"Cannot open video: {video_path}")

    input_fps = cap.get(cv2.CAP_PROP_FPS)
    frame_skip = max(1, int(round(input_fps / TARGET_FPS)))

    frames = []
    frame_idx = 0
    while True:
        ret, frame = cap.read()
        if not ret:
            break
        frame = cv2.flip(frame, 0)
        if frame_idx % frame_skip == 0:
            frame_resized = cv2.resize(frame, (TARGET_WIDTH, TARGET_HEIGHT), interpolation=cv2.INTER_AREA)
            # Convert BGR (OpenCV default) to RGB
            frame_rgb = cv2.cvtColor(frame_resized, cv2.COLOR_BGR2RGB)

            frames.append(frame_rgb)
        frame_idx += 1

    cap.release()
    return frames

def build_global_palette(frames):
    # Stack all frames into one big image vertically
    stacked = np.vstack(frames)
    pil_img = Image.fromarray(stacked)
    # Quantize with max 256 colors using Pillow
    pil_quant = pil_img.quantize(colors=MAX_COLORS, method=Image.FASTOCTREE , kmeans=0, palette=None, dither=Image.NONE)

    palette = pil_quant.getpalette()[:MAX_COLORS*3]  # RGB triples
    # Convert palette to numpy array shape (MAX_COLORS, 3)
    palette_np = np.array(palette, dtype=np.uint8).reshape((-1,3))
    return palette_np

def quantize_frames(frames, palette):
    # Build palette image for Pillow quantize remap
    palette_img = Image.new("P", (16,16))
    flat_palette = palette.flatten()
    palette_img.putpalette(flat_palette.tolist() + [0]*(768 - len(flat_palette)))

    indexed_frames = []
    for frame_rgb in frames:
        pil_frame = Image.fromarray(frame_rgb)
        # Quantize frame to our palette by remapping
        pil_quant = pil_frame.quantize(palette=palette_img)
        indexed = np.array(pil_quant, dtype=np.uint8)
        indexed_frames.append(indexed)
    return indexed_frames

def encode_frame_rle(indexed_frame):
    flat = indexed_frame.flatten()
    encoded = []
    run_length = 1
    prev_pixel = flat[0]
    for pix in flat[1:]:
        if pix == prev_pixel and run_length < 255:
            run_length += 1
        else:
            encoded.append(run_length)
            encoded.append(prev_pixel)
            run_length = 1
            prev_pixel = pix
    # Append last run
    encoded.append(run_length)
    encoded.append(prev_pixel)
    return encoded

def encode_frames_rle(indexed_frames):
    rle_stream = []
    frame_offsets = []
    offset = 0  # offset in texels (each texel = 1 run + 1 palette index)
    for frame in indexed_frames:
        frame_offsets.append(offset)
        encoded = encode_frame_rle(frame)
        rle_stream.extend(encoded)
        offset += len(encoded) // 2  # each pair is one texel
    return rle_stream, frame_offsets

def save_palette_as_png(palette_bytes, out_path="palette.png"):
    # Expecting a flat list of bytes: [r, g, b, r, g, b, ...]
    color_count = len(palette_bytes) // 3
    img = Image.new("RGB", (color_count, 1))
    
    pixels = []
    for i in range(color_count):
        r = palette_bytes[i * 3]
        g = palette_bytes[i * 3 + 1]
        b = palette_bytes[i * 3 + 2]
        pixels.append((r, g, b))
    
    img.putdata(pixels)
    img.save(out_path)

def write_output_files(out_prefix, rle_stream, frame_offsets, palette):
    # Write RLE stream texture data (RG8: run_length, palette_index)
    # First texels: frame_offsets (1 texel per frame, 2 bytes)
    import struct
    with open(out_prefix + ".rletex", "wb") as f:
        
        # Add video info in header
        f.write(struct.pack("<I", TARGET_WIDTH))
        f.write(struct.pack("<I", TARGET_HEIGHT))
        f.write(struct.pack("<I", TARGET_FPS))
        f.write(struct.pack("<I", len(frame_offsets)))

        # Write frame offsets first
        for offs in frame_offsets:
            # Write as little endian uint16
            # f.write(struct.pack("<H", offs))
            f.write(struct.pack("<I", offs))            
        # Then write RLE data
        for i in range(0, len(rle_stream), 2):
            run_len = rle_stream[i]
            pal_idx = rle_stream[i+1]
            f.write(struct.pack("BB", run_len, pal_idx))

    # Write palette file: 256 * 3 bytes RGB
    with open(out_prefix + ".pal", "wb") as f:
        f.write(palette.tobytes())

    save_palette_as_png(palette.tobytes(), out_prefix + ".png")

def save_indexed_frames_video(indexed_frames, palette, output_path, fps=15):
    # Reconstruct RGB frames from indexed palette
    height, width = indexed_frames[0].shape
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')  # or 'XVID', 'MJPG'
    out = cv2.VideoWriter(output_path, fourcc, fps, (width, height))

    for idx_frame in indexed_frames:
        # Map palette index to RGB
        rgb_frame = np.zeros((height, width, 3), dtype=np.uint8)
        for i, color in enumerate(palette):
            rgb_frame[idx_frame == i] = color
        # Convert RGB to BGR for OpenCV
        bgr_frame = cv2.cvtColor(rgb_frame, cv2.COLOR_RGB2BGR)
        out.write(bgr_frame)

    out.release()           

def main(video_path, output_prefix):
    print("Decoding video frames...")
    frames = decode_video_to_frames(video_path)
    print(f"Decoded {len(frames)} frames")

    print("Building global palette...")
    palette = build_global_palette(frames)
    print(f"Palette size: {palette.shape}")

    print("Quantizing frames...")
    indexed_frames = quantize_frames(frames, palette)

    save_indexed_frames_video(indexed_frames, palette, output_prefix + "_preview.mp4")
    print(f"Saved preview video: {output_prefix}_preview.mp4")    

    print("Encoding frames with RLE...")
    rle_stream, frame_offsets = encode_frames_rle(indexed_frames)
    print(f"Total RLE texels: {len(rle_stream)//2}")

    print("Writing output files...")
    write_output_files(output_prefix, rle_stream, frame_offsets, palette)
    print("Done.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python script.py input_video.mp4 output_prefix")
        sys.exit(1)
    main(sys.argv[1], sys.argv[2])
