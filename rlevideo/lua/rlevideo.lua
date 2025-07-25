-- RLE Video is a simple video playback system that plays
--  RLE and palette based video frames to a texture. 
--  There are scripts in the source/tools folder for generating the data files.
--  Output files are a palette file and a rle data video file. 
--  The video file has a header of all the RLE frame offsets, and each texel of data is
--  a stream of two bytes: byte1 - the rle count. byte2 - the palette index.
-- 
-- This file provides an interface to the C++ calls that decode the RLE data and palette into 
-- an RGB texture frame that can be used to directly set a texture. 
-- -----------------------------------------------------------------------------------------

local videos = {

    allvideos   = {},
}

-- -----------------------------------------------------------------------------------------
-- Load both the palette and the rlevideo from the filename
videos.load = function( filename )

    -- These are not stored in lua!! Only on C++ side.
    local palette = nil
    local rletex = nil

    local fh = io.open(string.format("%s.pal", filename), "rb")
    if(fh) then 
        local data = fh:read("*a")
        if(data) then palette = data end 
    end

    local fh = io.open(string.format("%s.rletex", filename), "rb")
    if(fh) then 
        local data = fh:read("*a")
        if(data) then rletex = data end 
    end

    if(palette == nil) then 
        pprint("[Error] Invalid Palette file.")
        return nil
    end
    if(rletex == nil) then 
        pprint("[Error] Invalid RLE Data file.")
        return nil
    end

    local info = rlevideo.videoadd( palette, #palette, rletex, #rletex )
    info.palette = palette -- store palette so we cam switch / modify (like for fades)
    info.buffer = buffer.create(info.width * info.height, {
        { name = hash("rgba"), type = buffer.VALUE_TYPE_UINT8, count = 4 }
    })

    info.tparams = {
        width          = info.width,
        height         = info.height,
        type           = graphics.TEXTURE_TYPE_2D,
        format         = graphics.TEXTURE_FORMAT_RGBA,
        image_buffer   = info.buffer,
    }
    local new_path = "/imgbuffer_"..string.format("%d", info.id)..".texturec"
    info.tex = resource.create_texture(new_path, info.tparams)

    -- Store info, in case it needs to be fetched 
    videos.allvideos[filename] = info
    return info
end

-- -----------------------------------------------------------------------------------------

videos.decode = function( video_info, frame_number )

    rlevideo.videodecode( video_info.id, frame_number, video_info.palette, video_info.palsize, video_info.buffer )

    -- Update texture with pixel buffer
    resource.set_texture(video_info.tex, video_info.tparams, video_info.buffer)
end

-- -----------------------------------------------------------------------------------------

return videos

-- -----------------------------------------------------------------------------------------