# The documentation for easy-phi

## Simple Types

```cpp
using ep_u8 = uint8_t;
using ep_u16 = uint16_t;
using ep_u32 = uint32_t;
using ep_u64 = uint64_t;

using ep_i8 = int8_t;
using ep_i16 = int16_t;
using ep_i32 = int32_t;
using ep_i64 = int64_t;

using ep_f32 = float;
using ep_f64 = double;
```

## Data

It is a warper of std::vector<ep_u8>, and it is used to store the binary data.

```cpp
Data data;

// create from a pointer
ep_u8 ptr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
data = FromPtr(&ptr[0], 10);

// create from a file
Data::FromFile(&data, "data.bin");
```

## Vec2

It is used to store a 2D vector, you can create it like this: `Vec2 { 3.0, 4.0 }`.

## Rect 

It is used to store a rectangle, you can create it like this: `Rect { 0.0, 0.0, 1.0, 1.0 }`.

## Color

It is used to store a color, you can create it like this: `Color { 1.0, 0.0, 0.0, 1.0 }`.

## PhiChart

It is the main class of the library, it is used to store a loaded chart and render it.

```cpp
Data data;
PhiChartLoadResult result = loadChartFromData(data);

// if the chart is loaded successfully
if (result.success) {
    PhiChart chart = result.chart;
    chart.init(); // initialize the chart
} else {
    // handle the error
    for (const auto& error : result.errors) {
        // error is a std::string
    }
}
```

## StoryboardHelpers

It is a helper class that provides some functions to help you handle the storyboard loader.

```cpp
PhiChart chart = ...;
StoryboardHelpers::loadStoryboard(
    chart.storyboardAssets,
    "./path/to/chart/directory",
    [&](std::string imagePath) -> std::optional<std::pair<ep_u64, Vec2>> {
        // return the id of the image and its size
        // if the image can't be loaded, return std::nullopt
    },
    [&](ep_u64 id) {
        // release the image with the given id
    }
);
```

## CalculateFrameConfig

It is a config class that is used to calculate the frame.

```cpp
CalculateFrameConfig config {};

config.screenSize = Vec2 { 1920.0, 1080.0 };
config.backgroundTextureSize = Vec2 { 1920.0, 1440.0 }; // the size of the background texture
config.songLength = 100.0; // the length of the song in seconds

// configure note textures info
for (auto type : {
    EnumPhiNoteType::Tap,
    EnumPhiNoteType::Hold,
    EnumPhiNoteType::Flick,
    EnumPhiNoteType::Hold
}) {
    config.noteTexturesInfo[type] = CalculateFrameConfig::NoteTextureInfo {
        .single = CalculateFrameConfig::NoteTextureInfo::Item {
            .textureSize = Vec2 { 128.0, 32.0 }, // the size of the texture
            .cutPadding = Vec2 { 32.0, 32.0 } // more details in the next
        },
        .simul = ... // the same as single field
    };
}
```

`cutPadding` is used to cut the texture, `.first` is the padding of the top and the first cut-point (tail), and `.second` is the padding of the bottom and the second cut-point (head).

## CalculatedFrame

It is a struct that contains the calculated frame.

The position origin is the top-left corner of the screen (0, 0), and the right-bottom corner is `config.screenSize`.

```cpp
CalculatedFrame frame = ...;

frame.backgroundImageBlurRadius; // the blur radius of the background image, relative to the raw size of the background texture

frame.unsafeBackgroundRect; // the rect of the unsafe area background image.
frame.unsafeAreaDim; // 1.0 means the image is black.

frame.backgroundRect; // same as ↑
frame.backgroundDim; // same as ↑

// we should render the unsafe background first and then the main background

frame.objectsClipRect; // the rect of the objects clip area, all the objects should be rendered in this rect

frame.objects; // the objects to render, it is a vector of variants

for (const auto& obj : frame.objects) {
    if (std::holds_alternative<CalculatedFrame::CalculatedLine>(obj)) {
        auto& line = std::get<CalculatedFrame::CalculatedLine>(obj);

        line.p1; // the start point of the line
        line.p2; // the end point of the line
        line.thickness; // the thickness of the line
        line.color; // the color of the line
    } else if (std::holds_alternative<CalculatedFrame::CalculatedNote>(obj)) {
        auto& note = std::get<CalculatedFrame::CalculatedNote>(obj);

        note.position; // the position of the note, it is the connection point of the head and body
        note.rotation; // the rotation of the note, in degrees
        note.width; // the width of the note, in pixels
        note.head; // the head height of the note, in pixels
        note.body; // the body height of the note, in pixels
        note.tail; // the tail height of the note, in pixels
        note.type; // the type of the note
        note.isSimul; // whether the note is simul
        note.color; // the color of the note
    } else if (std::holds_alternative<CalculatedFrame::CalculatedText>(obj)) {
        auto& text = std::get<CalculatedFrame::CalculatedText>(obj);

        text.text; // the text to render
        text.position; // the position of the text
        text.scale; // the scale of the text
        text.align; // the alignment of the text, it is a combination of `EnumTextAlign` enum
        text.baseline; // the baseline of the text, it is a combination of `EnumTextBaseline` enum
        text.fontSize; // the font size of the text, in pixels
        text.rotation; // the rotation of the text, in degrees
        text.color; // the color of the text
    } else if (std::holds_alternative<CalculatedFrame::CalculatedStoryboardTexture>(obj)) {
        auto& sbTexture = std::get<CalculatedFrame::CalculatedStoryboardTexture>(obj);

        sbTexture.texture; // the id of the texture that you loaded before
        sbTexture.position; // the position of the texture
        sbTexture.size; // the size of the texture, in pixels
        sbTexture.scale; // the scale of the texture
        sbTexture.anchor; // the anchor of the texture, { 0.0, 0.0 } means left-top, { 1.0, 1.0 } means right-bottom
        sbTexture.rotation; // the rotation of the texture, in degrees
        sbTexture.color; // the color of the texture
    } else if (std::holds_alternative<CalculatedFrame::CalculatedHitEffectTexture>(obj)) {
        auto& effect = std::get<CalculatedFrame::CalculatedHitEffectTexture>(obj);

        effect.position; // the position of the effect
        effect.size; // the size of the effect, in pixels
        effect.progress; // the progress of the effect, 0.0 ~ 1.0, you should use it to switch the texture
        effect.rotation; // the rotation of the effect, in degrees
        effect.color; // the color of the effect
    } else if (std::holds_alternative<CalculatedFrame::CalculatedHitEffectParticle>(obj)) {
        auto& effect = std::get<CalculatedFrame::CalculatedHitEffectParticle>(obj);

        effect.position; // the position of the effect in the center
        effect.size; // the size of the effect, in pixels
        effect.rotation; // the rotation of the effect, in degrees
        effect.color; // the color of the effect
    } else if (std::holds_alternative<CalculatedFrame::CalculatedPoly>(obj)) {
        auto& poly = std::get<CalculatedFrame::CalculatedPoly>(obj);

        poly.p1; // the first point of the poly
        poly.p2; // the second point of the poly
        poly.p3; // the third point of the poly
        poly.p4; // the fourth point of the poly
        poly.color; // the color of the poly
    }
}

// after rendering the objects, we should play the hit sounds

for (const auto& [type, delta] : frame.hitsounds) {
    type; // the type of the note
    delta; // currentTime - note.time, in seconds, but if you are realtime rendering, it is unnecessary
}
```

## calculateFrame

It is the most important function in the library, and it is used to calculate the frame.

```cpp
PhiChart chart = ...;
ep_f64 time = 1.25; // in seconds
CalculateFrameConfig config = ...;

CalculatedFrame frame {};
calculateFrame(chart, time, config, frame);
// now you can render the frame on your own renderer
```
