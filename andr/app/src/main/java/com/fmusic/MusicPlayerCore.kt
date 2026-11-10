package com.fmusic

class MusicPlayerCore {
    companion object {
        init { System.loadLibrary("fmusic_core") }
        @JvmStatic external fun probePath(path: String): TrackInfoData?
        @JvmStatic external fun parseLyrics(text: String): LyricsData
    }
    external fun create(): Long
    external fun destroy(handle: Long)
    external fun loadPath(handle: Long, path: String): Boolean
    external fun play(handle: Long)
    external fun pause(handle: Long)
    external fun seekToSeconds(handle: Long, seconds: Double)
    external fun pullInterleaved(handle: Long, buf: FloatArray, frames: Int)
    external fun copySpectrum(handle: Long, bands: FloatArray): Float
    external fun getPosition(handle: Long): Double
    external fun getDuration(handle: Long): Double
    external fun isPlaying(handle: Long): Boolean
    external fun isEnded(handle: Long): Boolean
    external fun setEqEnabled(handle: Long, enabled: Boolean)
    external fun setEqGain(handle: Long, band: Int, db: Float)
    external fun applyEqPreset(handle: Long, idx: Int)
    external fun getEqGain(handle: Long, band: Int): Float
}

data class TrackInfoData(val title: String, val artist: String, val album: String,
    val duration: Double, val sampleRate: Int, val coverArt: ByteArray?)
data class LyricsData(val synced: Boolean, val lines: List<LyricLineData>)
data class LyricLineData(val time: Double, val text: String)
