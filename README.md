# RLE Video Native Extension

## What this is:

A simple RLE low palette video playback library that allows Defold developers to play simple video feeds within a Defold scene. These are mapped to textures so it could be a video on a sprite, or model, etc.

## What this isnt:

A generic video playback system. 

The video data must be built using the tool, and files can get big if a large palette is used, high fps or large resolution. It will properly decode these, but the performance will suffer.

## Tool

In the tools folder is a simple python script to generate your palette and rledata (more info about the format down below).

To make a video run the command from a CLI (where you have python and libs installed):
```python.exe .\video_gen.py .\replay.mp4 replay```

The instructions format is:
```video_gen.py <input video> <output name>```

After running the command you will end up with three new files:
- A mp4 with and _preview - this can be used to check what the video looks like after scaling and palette generation. 
- A pal file that contains a simple indexed palette of rgb elements. No alpha supported.
- A rletex file which is the rle video stream data which has a specific format.

## Usage

Include the lua script that is in the extension:

```local video 	= require("rlevideo.lua.rlevideo")```

Load in a video of your choosing, make sure you save the returned info table, you will need it! 

```self.vid1 = video.load( "assets/videos/quickstart")```

Now the video is loaded in memory, and you can decode frames to a texture.

Within the self.vid1 table a texture will be updated to contain that frame. 

To use that texture you can call

```go.set("/video2#quad", "texture0", self.vid1.tex)```

Once this is set, anytime a frame is decoded for this video object, all objects using this texture will get the new frame in that texture.

To decode a frame at frame 100:

```video.decode(self.vid1, 100)```

And thats it. The process is CPU based (it could be possible to do this all on GPU) and so keep in mind large videos with large palettes will perform slower than small ones.

## RLE Video Format

I have attempted to make the format as simplistic as possible so that decoding is super fast, and that there doesnt need to be any runtime frame adjustments. 

### Palette format

Array of RGB triples for each palette index. Each element is a uint8_t type (unsigned char byte)

| R1 | G1 | B1 | R2 | G2 | B2 | R3 | G3 | B3 ...

### RLETex format

The top part of the file is a Header. All elements are uint32_t type.

| width | height | fps | frames | 

A secondary part of the header is a list of the frame offsets for the number of frames 

| frame1_offset | frame2_offset | frame3_offset | frame4_offset | ..... | frame(frames-1)_offset |

The remaining data is RLE packed data. All frames one after the other. 
The RLE format is a two byte data for each texel where:
First Byte: Run Length 
Second Byte: Palette Index 

The RLE Data looks like:

| RunLen1 | PalIndex1 | RunLen2 | PalIndex2 | RunLen3 | PalIndex3 | RunLen4 | PalIndex4 |...

## Futures

There are a number of improvements that could be made with this. Some of the things I may look at:
- GPU decoding by loading the rledata as a text into a shader for decode or use compute.
- Remove the offsets for frames, and have markers (this makes frame movement more annoying, so not sure)
- Add an audio track
- Add some compression (simple) - although this is not a priority for me.
- Support some different forms of RLE and Quad/Octree sampling for encoding and playback. 

## License
The MIT License (MIT)
Copyright © 2025 David Lannan

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


