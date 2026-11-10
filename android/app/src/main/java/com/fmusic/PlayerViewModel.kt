package com.fmusic

import android.app.Application
import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.media.session.MediaSession
import android.media.session.PlaybackState
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream

data class Track(val path: String, var title: String = "", var artist: String = "", var duration: Double = 0.0)
data class LyricLine(val id: Int, val time: Double, val text: String)
enum class RepeatMode { OFF, ALL, ONE }

class PlayerViewModel(app: Application) : AndroidViewModel(app) {
    private val core = MusicPlayerCore()
    private var coreHandle: Long = 0L

    private val _isPlaying = MutableStateFlow(false)
    val isPlaying: StateFlow<Boolean> = _isPlaying.asStateFlow()

    private val _position = MutableStateFlow(0.0)
    val position: StateFlow<Double> = _position.asStateFlow()

    private val _duration = MutableStateFlow(0.0)
    val duration: StateFlow<Double> = _duration.asStateFlow()

    private val _title = MutableStateFlow("Chưa có bài hát")
    val title: StateFlow<String> = _title.asStateFlow()

    private val _artist = MutableStateFlow("Mở Danh sách phát để thêm nhạc")
    val artist: StateFlow<String> = _artist.asStateFlow()

    private val _tracks = MutableStateFlow<List<Track>>(emptyList())
    val tracks: StateFlow<List<Track>> = _tracks.asStateFlow()

    private val _currentIndex = MutableStateFlow(-1)
    val currentIndex: StateFlow<Int> = _currentIndex.asStateFlow()

    private val _bands = MutableStateFlow(FloatArray(64))
    val bands: StateFlow<FloatArray> = _bands.asStateFlow()

    private val _bass = MutableStateFlow(0f)
    val bass: StateFlow<Float> = _bass.asStateFlow()

    private val _lyrics = MutableStateFlow<List<LyricLine>>(emptyList())
    val lyrics: StateFlow<List<LyricLine>> = _lyrics.asStateFlow()

    private val _currentLyric = MutableStateFlow(-1)
    val currentLyric: StateFlow<Int> = _currentLyric.asStateFlow()

    private val _eqEnabled = MutableStateFlow(false)
    val eqEnabled: StateFlow<Boolean> = _eqEnabled.asStateFlow()

    private val _eqGains = MutableStateFlow(FloatArray(10))
    val eqGains: StateFlow<FloatArray> = _eqGains.asStateFlow()

    private val _sampleRate = MutableStateFlow(44100)
    val sampleRate: StateFlow<Int> = _sampleRate.asStateFlow()

    private val _shuffle = MutableStateFlow(false)
    val shuffle: StateFlow<Boolean> = _shuffle.asStateFlow()

    private val _repeatMode = MutableStateFlow(RepeatMode.OFF)
    val repeatMode: StateFlow<RepeatMode> = _repeatMode.asStateFlow()

    private val _coverArt = MutableStateFlow<ByteArray?>(null)
    val coverArt: StateFlow<ByteArray?> = _coverArt.asStateFlow()

    private var audioTrack: AudioTrack? = null
    private var audioJob: Job? = null
    private var tickJob: Job? = null
    private val bandBuf = FloatArray(64)

    init {
        coreHandle = core.create()
        initAudioTrack()
        startTick()
    }

    private fun initAudioTrack(sr: Int = 44100) {
        audioJob?.cancel()
        audioTrack?.release()
        val minBuf = AudioTrack.getMinBufferSize(sr, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_FLOAT)
        val bufSize = maxOf(minBuf, sr / 10 * 2 * 4) // ~100ms stereo float
        audioTrack = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setSampleRate(sr)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                    .setEncoding(AudioFormat.ENCODING_PCM_FLOAT).build()
            )
            .setBufferSizeInBytes(bufSize)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
        audioTrack?.play()

        val framesPerChunk = bufSize / 8 // float stereo = 8 bytes/frame
        val pullBuf = FloatArray(framesPerChunk * 2)
        audioJob = viewModelScope.launch(Dispatchers.IO) {
            while (isActive) {
                core.pullInterleaved(coreHandle, pullBuf, framesPerChunk)
                audioTrack?.write(pullBuf, 0, pullBuf.size, AudioTrack.WRITE_BLOCKING)
            }
        }
    }

    private fun startTick() {
        tickJob = viewModelScope.launch {
            while (isActive) {
                val bass = core.copySpectrum(coreHandle, bandBuf)
                _bands.value = bandBuf.copyOf()
                _bass.value = bass
                _position.value = core.getPosition(coreHandle)
                // Auto-advance when track ends
                if (_isPlaying.value && core.isEnded(coreHandle)) {
                    next(automatic = true)
                }
                // Lyric sync — find current line by timestamp
                val pos = _position.value
                val lyrList = _lyrics.value
                if (lyrList.isNotEmpty()) {
                    var idx = -1
                    for (i in lyrList.indices) {
                        if (lyrList[i].time <= pos + 0.05) idx = i else break
                    }
                    if (idx != _currentLyric.value) _currentLyric.value = idx
                }
                delay(16) // ~60fps
            }
        }
    }

    fun openFiles(paths: List<String>) {
        _tracks.value = emptyList()
        _currentIndex.value = -1
        if (paths.isNotEmpty()) addFiles(paths)
    }

    fun addFiles(paths: List<String>) {
        val newTracks = paths.map { Track(path = it, title = File(it).nameWithoutExtension) }
        _tracks.value = _tracks.value + newTracks
        viewModelScope.launch(Dispatchers.IO) {
            newTracks.forEach { track ->
                MusicPlayerCore.probePath(track.path)?.let { info ->
                    _tracks.value = _tracks.value.map {
                        if (it.path == track.path) it.copy(
                            title = info.title.ifEmpty { it.title },
                            artist = info.artist,
                            duration = info.duration
                        ) else it
                    }
                }
            }
        }
        if (_currentIndex.value < 0) loadTrack(0, autoplay = true)
    }

    fun loadTrack(index: Int, autoplay: Boolean) {
        val list = _tracks.value
        if (index !in list.indices) return
        _currentIndex.value = index
        val track = list[index]
        core.loadPath(coreHandle, track.path)
        _title.value = track.title
        _artist.value = track.artist
        _duration.value = core.getDuration(coreHandle)
        _position.value = 0.0
        if (autoplay) {
            core.play(coreHandle)
            _isPlaying.value = true
        }
        loadLyrics(track.path)
    }

    private fun loadLyrics(audioPath: String) {
        val lrcFile = File(audioPath).let { File(it.parent, it.nameWithoutExtension + ".lrc") }
        val text = if (lrcFile.exists()) lrcFile.readText() else ""
        val parsed = MusicPlayerCore.parseLyrics(text)
        _lyrics.value = parsed.lines.mapIndexed { i, l -> LyricLine(i, l.time, l.text) }
    }

    fun togglePlay() {
        if (_isPlaying.value) {
            core.pause(coreHandle)
            _isPlaying.value = false
        } else {
            core.play(coreHandle)
            _isPlaying.value = true
        }
    }

    fun next(automatic: Boolean = false) {
        val list = _tracks.value
        if (list.isEmpty()) return
        if (automatic && _repeatMode.value == RepeatMode.ONE) {
            core.seekToSeconds(coreHandle, 0.0)
            core.play(coreHandle)
            return
        }
        val cur = _currentIndex.value
        val next = when {
            _shuffle.value && list.size > 1 -> (0 until list.size).filter { it != cur }.random()
            cur + 1 < list.size -> cur + 1
            _repeatMode.value == RepeatMode.ALL -> 0
            else -> null
        }
        if (next != null) loadTrack(next, autoplay = true)
        else { core.pause(coreHandle); _isPlaying.value = false }
    }

    fun previous() {
        val pos = core.getPosition(coreHandle)
        if (pos > 3 || _currentIndex.value <= 0) {
            core.seekToSeconds(coreHandle, 0.0)
        } else {
            loadTrack(_currentIndex.value - 1, autoplay = true)
        }
    }

    fun seek(seconds: Double) {
        core.seekToSeconds(coreHandle, seconds)
        _position.value = seconds
    }

    fun toggleShuffle() { _shuffle.value = !_shuffle.value }

    fun cycleRepeat() {
        _repeatMode.value = when (_repeatMode.value) {
            RepeatMode.OFF -> RepeatMode.ALL
            RepeatMode.ALL -> RepeatMode.ONE
            RepeatMode.ONE -> RepeatMode.OFF
        }
    }

    fun setEqEnabled(on: Boolean) {
        _eqEnabled.value = on
        core.setEqEnabled(coreHandle, on)
    }

    fun setEqGain(band: Int, db: Float) {
        val gains = _eqGains.value.copyOf()
        gains[band] = db
        _eqGains.value = gains
        core.setEqGain(coreHandle, band, db)
    }

    fun applyPreset(idx: Int) {
        core.applyEqPreset(coreHandle, idx)
        _eqGains.value = FloatArray(10) { core.getEqGain(coreHandle, it) }
    }

    fun setSampleRate(sr: Int) {
        _sampleRate.value = sr
        initAudioTrack(sr)
    }

    // Playlist save/load (.fpl format)
    fun savePlaylist(path: String) {
        File(path).printWriter().use { out ->
            out.println("#FMUSIC PLAYLIST 1.1")
            _tracks.value.forEach { out.println(it.path) }
        }
    }

    fun loadPlaylist(path: String) {
        val lines = File(path).readLines().filter { !it.startsWith('#') && it.isNotBlank() }
        openFiles(lines)
    }

    // EQ preset save/load (.feq format)
    fun saveEqPreset(name: String, context: Context) {
        val dir = File(context.filesDir, "eq_presets").also { it.mkdirs() }
        val json = JSONObject().apply {
            put("version", 1)
            put("name", name)
            put("enabled", _eqEnabled.value)
            put("gains", org.json.JSONArray(_eqGains.value.map { it.toDouble() }))
        }
        File(dir, "$name.feq").writeText(json.toString(2))
    }

    fun loadEqPreset(path: String) {
        val json = JSONObject(File(path).readText())
        val arr = json.getJSONArray("gains")
        val gains = FloatArray(10) { arr.getDouble(it).toFloat() }
        gains.forEachIndexed { i, g -> core.setEqGain(coreHandle, i, g) }
        _eqGains.value = gains
        setEqEnabled(json.optBoolean("enabled", true))
    }

    override fun onCleared() {
        super.onCleared()
        audioJob?.cancel()
        tickJob?.cancel()
        audioTrack?.stop()
        audioTrack?.release()
        core.destroy(coreHandle)
    }
}
