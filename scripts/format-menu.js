function cleanText(value) {
    return String(value || "Video").replace(/[\t\r\n]+/g, " ").trim();
}

function estimatedSize(format, duration) {
    const direct = Number(format.filesize || format.filesize_approx || 0);
    if (direct > 0) {
        return direct;
    }
    const bitrate = Number(format.tbr || format.vbr || format.abr || 0);
    return bitrate > 0 && duration > 0 ? bitrate * 1000 * duration / 8 : 0;
}

function bestMatching(formats, predicate) {
    for (let index = formats.length - 1; index >= 0; --index) {
        if (predicate(formats[index])) {
            return formats[index];
        }
    }
    return null;
}

if (scriptArgs.length !== 3) {
    std.err.puts("usage: format-menu.js <metadata-json> <choices-tsv>\n");
    std.exit(2);
}

const metadata = JSON.parse(std.loadFile(scriptArgs[1]));
const formats = Array.isArray(metadata.formats) ? metadata.formats : [];
const duration = Number(metadata.duration || 0);
const heights = [...new Set(formats
    .filter(format => format.vcodec && format.vcodec !== "none" && Number(format.height) > 0)
    .map(format => Number(format.height)))]
    .sort((left, right) => right - left);
const bestAudio = bestMatching(formats,
    format => format.vcodec === "none" && format.acodec !== "none" && format.ext === "m4a") ||
    bestMatching(formats, format => format.vcodec === "none" && format.acodec !== "none");

const output = std.open(scriptArgs[2], "w");
if (!output) {
    std.err.puts("could not create format choice file\n");
    std.exit(1);
}

output.puts(`title\t${cleanText(metadata.title)}\n`);
for (const height of heights) {
    const bestMp4 = bestMatching(formats, format =>
        format.vcodec !== "none" && Number(format.height) === height && format.ext === "mp4");
    const bestVideo = bestMp4 || bestMatching(formats, format =>
        format.vcodec !== "none" && Number(format.height) === height);
    if (!bestVideo) {
        continue;
    }
    let bytes = estimatedSize(bestVideo, duration);
    if (bestVideo.acodec === "none" && bestAudio) {
        bytes += estimatedSize(bestAudio, duration);
    }
    const selector = `bv*[height=${height}][ext=mp4]+ba[ext=m4a]/` +
        `b[height=${height}][ext=mp4]/bv*[height=${height}]+ba/b[height=${height}]`;
    output.puts(`video\t${height}\t${bestMp4 ? "mp4" : cleanText(bestVideo.ext)}\t` +
        `${Math.round(bytes)}\t${selector}\n`);
}

if (bestAudio) {
    output.puts(`audio\t0\tmp3\t${Math.round(estimatedSize(bestAudio, duration))}\t` +
        "ba[ext=m4a]/ba\n");
}
output.close();

if (heights.length === 0 && !bestAudio) {
    std.err.puts("no downloadable audio or video formats were found\n");
    std.exit(1);
}
