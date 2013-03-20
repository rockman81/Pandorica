#!/bin/bash
#set -x
set -e

function idFor() {
    echo -n "$@" | md5sum | cut -f1 -d' '
}

function mediaDataDir() {
    fname="$1"
    medias_dir="$2"
    echo "$medias_dir/$( idFor "$fname")"
}

function saveMediaInfo() {
    file="$1"
    dataDir="$( mediaDataDir "$1" "$2")"
    mkdir -p "$dataDir"
    outfile="$dataDir/media.info"
    ffprobe -loglevel quiet -of json            -show_format -show_streams "$file" > "$outfile.json"
    ffprobe -loglevel quiet -of flat=sep_char=_ -show_format -show_streams "$file" > "$outfile"
    echo "$outfile"
}


function extractSubtitles() {
    file="$1"
    dataDir="$( mediaDataDir "$1" "$2")/subs"
    mkdir -p "$dataDir"
    echo "Extracting subtitles from $file to $dataDir"
    eval "$( cat "$3" )"
    for i in `seq 0 $(( $format_nb_streams-1))`; do
        stream_type="$( eval echo \$streams_stream_${i}_codec_type )"
        track_language="$( eval echo \$streams_stream_${i}_tags_language)"
	track_title="$( eval echo \$streams_stream_${i}_tags_title)"
	sub_filename="${track_language}.${track_title}"
        if test "x$stream_type" == "xsubtitle" && ! [ -r "$dataDir/${sub_filename}.vtt" ] ; then
            ffmpeg -loglevel quiet -y -i "$file" -map 0:$i -c srt "$dataDir/${sub_filename}.srt" 2>/dev/null
            curl -F "subrip_file=@${dataDir}/${sub_filename}.srt" http://atelier.u-sub.net/srt2vtt/index.php > "$dataDir/${sub_filename}.vtt"
        fi
    done
}

function createThumbnail() {
    file="$1"
    dataDir="$( mediaDataDir "$1" "$2")"
    if [ -r "$dataDir/preview.png" ]; then return; fi
    echo "Creating thumbnail for $file to $dataDir"
    eval "$( cat "$3")"
    if test "x$format_duration" == "xN/A"; then return; fi
    one_third="$(( $(echo $format_duration | cut -d. -f1) / 3 ))"
    mkdir -p "$dataDir"
    #ffmpeg -i "$file" -loglevel quiet -ss $one_third -f image2 -vframes 1 "$dataDir/preview.png" 2>/dev/null
    cd "$dataDir"
    mplayer "$( readlink -f "$file" )" -vo png:z=9 -ao null -frames 1 -ss "$one_third" 2>&1 > /dev/null
    mv 00000003.png preview.png
    rm 00000*.png
    cd -
}

function genFileData() {
    fname="$1"; shift
    medias_dir="$1"; shift
    case "$fname" in
        *.mp4|*.m4v)
        mediaInfo="$( saveMediaInfo "$fname" "$medias_dir" )"
        extractSubtitles "$fname" "$medias_dir" "$mediaInfo"
        createThumbnail "$fname" "$medias_dir" "$mediaInfo"
        ;;
        *)
#	echo "Unknown file type: $fname"
	return
        ;;
    esac
}

if test "x$1" == "xfunct"; then
    shift
    functName="$1"; shift
    $functName "$@"
    exit 0
fi
if test "x$1" == "xgenFileData"; then
    genFileData "$2" "$3"
    exit 0
fi
MEDIAS_DIR="$1"; shift
DATA_DIR="${2-$MEDIAS_DIR/.media_data}"
DATA_DIR="$( readlink -f "$DATA_DIR")"

echo "Main data output directory: $DATA_DIR"

find -L "$MEDIAS_DIR" -type f -exec "$0" genFileData {} "$DATA_DIR" \;
#find -L "$MEDIAS_DIR" -type f | while read fname; do genFileData "$fname" "$DATA_DIR"; done

