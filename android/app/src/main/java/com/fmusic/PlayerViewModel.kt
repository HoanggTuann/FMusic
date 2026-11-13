package com.fmusic

import android.app.Application
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.ColorMatrix
import android.graphics.ColorMatrixColorFilter
import android.graphics.Paint
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.media.MediaMetadata
import android.media.session.MediaSession
import android.media.session.PlaybackState
import android.net.Uri
import android.provider.OpenableColumns
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import kotlin.math.abs

data class Track(
    val path: String,
    val title: String = "",
    val artist: String = "",
    val duration: Double = 0.0
)

data class LyricLine(val id: Int, val time: Double, val text: String)

data class UserEqPreset(val name: String, val gains: FloatArray) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is UserEqPreset) return false
        return name == other.name && gains.contentEquals(other.gains)
    }

    override fun hashCode(): Int = 31 * name.hashCode() + gains.contentHashCode()
}

enum class RepeatMode(val value: Int) {
    OFF(0), ALL(1), ONE(2);

    companion object {
        fun fromInt(v: Int): RepeatMode = entries.firstOrNull { it.value == v } ?: OFF
    }
}

private object BackdropHelper {
    fun decodeCover(bytes: ByteArray, maxSide: Int = 1024): Bitmap? {
        if (bytes.isEmpty()) return null
        val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        BitmapFactory.decodeByteArray(bytes, 0, bytes.size, bounds)
        if (bounds.outWidth <= 0 || bounds.outHeight <= 0) return null

        var sample = 1
        val maxDim = maxOf(bounds.outWidth, bounds.outHeight)
        while (maxDim / sample > maxSide * 2) sample *= 2

        val opts = BitmapFactory.Options().apply { inSampleSize = sample }
        return BitmapFactory.decodeByteArray(bytes, 0, bytes.size, opts)
    }

    fun makeBackdrop(src: Bitmap): Bitmap {
        val target = 96
        val ratio = minOf(target.toFloat() / src.width, target.toFloat() / src.height)
        val w = maxOf(16, (src.width * ratio).toInt())
        val h = maxOf(16, (src.height * ratio).toInt())
        val small = Bitmap.createScaledBitmap(src, w, h, true)

        // Tang bao hoa & giam sang nhe giong iOS CoreImage Backdrop.make
        val adjusted = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(adjusted)
        val satMatrix = ColorMatrix().apply { setSaturation(1.48f) }
        val darkScale = 0.86f
        val darkMatrix = ColorMatrix(
            floatArrayOf(
                darkScale, 0f, 0f, 0f, 0f,
                0f, darkScale, 0f, 0f, 0f,
                0f, 0f, darkScale, 0f, 0f,
                0f, 0f, 0f, 1f, 0f
            )
        )
        satMatrix.postConcat(darkMatrix)
        val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            colorFilter = ColorMatrixColorFilter(satMatrix)
        }
        canvas.drawBitmap(small, 0f, 0f, paint)
        if (small != src) small.recycle()

        fastBoxBlur(adjusted, radius = 9, passes = 3)
        return adjusted
    }

    private fun fastBoxBlur(bmp: Bitmap, radius: Int, passes: Int) {
        val w = bmp.width
        val h = bmp.height
        val pix = IntArray(w * h)
        val tmp = IntArray(w * h)
        bmp.getPixels(pix, 0, w, 0, 0, w, h)
        val div = radius * 2 + 1

        repeat(passes) {
            // Horizontal pass
            for (y in 0 until h) {
                val rowOff = y * w
                var rSum = 0; var gSum = 0; var bSum = 0
                for (dx in -radius..radius) {
                    val x = dx.coerceIn(0, w - 1)
                    val c = pix[rowOff + x]
                    rSum += (c shr 16) and 0xFF
                    gSum += (c shr 8) and 0xFF
                    bSum += c and 0xFF
                }
                for (x in 0 until w) {
                    tmp[rowOff + x] = (0xFF shl 24) or ((rSum / div) shl 16) or ((gSum / div) shl 8) or (bSum / div)
                    val remX = (x - radius).coerceIn(0, w - 1)
                    val addX = (x + radius + 1).coerceIn(0, w - 1)
                    val cRem = pix[rowOff + remX]
                    val cAdd = pix[rowOff + addX]
                    rSum += ((cAdd shr 16) and 0xFF) - ((cRem shr 16) and 0xFF)
                    gSum += ((cAdd shr 8) and 0xFF) - ((cRem shr 8) and 0xFF)
                    bSum += (cAdd and 0xFF) - (cRem and 0xFF)
                }
            }
            // Vertical pass
            for (x in 0 until w) {
                var rSum = 0; var gSum = 0; var bSum = 0
                for (dy in -radius..radius) {
                    val y = dy.coerceIn(0, h - 1)
                    val c = tmp[y * w + x]
                    rSum += (c shr 16) and 0xFF
                    gSum += (c shr 8) and 0xFF
                    bSum += c and 0xFF
                }
                for (y in 0 until h) {
                    pix[y * w + x] = (0xFF shl 24) or ((rSum / div) shl 16) or ((gSum / div) shl 8) or (bSum / div)
                    val remY = (y - radius).coerceIn(0, h - 1)
                    val addY = (y + radius + 1).coerceIn(0, h - 1)
                    val cRem = tmp[remY * w + x]
                    val cAdd = tmp[addY * w + x]
                    rSum += ((cAdd shr 16) and 0xFF) - ((cRem shr 16) and 0xFF)
                    gSum += ((cAdd shr 8) and 0xFF) - ((cRem shr 8) and 0xFF)
                    bSum += (cAdd and 0xFF) - (cRem and 0xFF)
                }
            }
        }
        bmp.setPixels(pix, 0, w, 0, 0, w, h)
    }
}

class PlayerViewModel(app: Application) : AndroidViewModel(app) {
    private val prefs = app.getSharedPreferences("fmusic_prefs", Context.MODE_PRIVATE)
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

    private val _coverBitmap = MutableStateFlow<Bitmap?>(null)
    val coverBitmap: StateFlow<Bitmap?> = _coverBitmap.asStateFlow()

    private val _backdropBitmap = MutableStateFlow<Bitmap?>(null)
    val backdropBitmap: StateFlow<Bitmap?> = _backdropBitmap.asStateFlow()

    private val _coverToken = MutableStateFlow(0)
    val coverToken: StateFlow<Int> = _coverToken.asStateFlow()

    private val _lyrics = MutableStateFlow<List<LyricLine>>(emptyList())
    val lyrics: StateFlow<List<LyricLine>> = _lyrics.asStateFlow()

    private val _lyricsSynced = MutableStateFlow(false)
    val lyricsSynced: StateFlow<Boolean> = _lyricsSynced.asStateFlow()

    private val _currentLyric = MutableStateFlow(-1)
    val currentLyric: StateFlow<Int> = _currentLyric.asStateFlow()

    // EQ
    val eqPresetNames: List<String> by lazy { MusicPlayerCore.getEqPresetNames().toList() }

    private val _eqEnabled = MutableStateFlow(false)
    val eqEnabled: StateFlow<Boolean> = _eqEnabled.asStateFlow()

    private val _eqPreset = MutableStateFlow(0)
    val eqPreset: StateFlow<Int> = _eqPreset.asStateFlow()

    private val _eqGains = MutableStateFlow(FloatArray(10))
    val eqGains: StateFlow<FloatArray> = _eqGains.asStateFlow()

    private val _userPresets = MutableStateFlow<List<UserEqPreset>>(emptyList())
    val userPresets: StateFlow<List<UserEqPreset>> = _userPresets.asStateFlow()

    // Playback & Settings
    private val _sampleRate = MutableStateFlow(44100)
    val sampleRate: StateFlow<Int> = _sampleRate.asStateFlow()

    private val _shuffle = MutableStateFlow(false)
    val shuffle: StateFlow<Boolean> = _shuffle.asStateFlow()

    private val _repeatMode = MutableStateFlow(RepeatMode.OFF)
    val repeatMode: StateFlow<RepeatMode> = _repeatMode.asStateFlow()

    private val _ambientBackground = MutableStateFlow(true)
    val ambientBackground: StateFlow<Boolean> = _ambientBackground.asStateFlow()

    private val _spectrumRing = MutableStateFlow(true)
    val spectrumRing: StateFlow<Boolean> = _spectrumRing.asStateFlow()

    private val _lyricOffsetMs = MutableStateFlow(0f)
    val lyricOffsetMs: StateFlow<Float> = _lyricOffsetMs.asStateFlow()

    private var audioTrack: AudioTrack? = null
    private var audioJob: Job? = null
    private var tickJob: Job? = null
    private var backdropJob: Job? = null
    private val bandBuf = FloatArray(64)
    private var hasHandledEnd = false

    private val lrcByName = mutableMapOf<String, String>()
    private var embeddedLyricsCache: String = ""

    private val audioManager = app.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private var focusRequest: AudioFocusRequest? = null
    private var mediaSession: MediaSession? = null

    init {
        coreHandle = core.create()
        loadPersistedSettings()
        initMediaSession()
        initAudioTrack(_sampleRate.value)
        startTick()
        indexImportedLrc()
        loadUserPresets()
        restorePlaylist()
    }

    private fun loadPersistedSettings() {
        val sr = prefs.getInt("sample_rate", 44100).let {
            if (it == 48000 || it == 96000) it else 44100
        }
        _sampleRate.value = sr
        core.setSampleRate(coreHandle, sr)

        _shuffle.value = prefs.getBoolean("shuffle", false)
        _repeatMode.value = RepeatMode.fromInt(prefs.getInt("repeat_mode", 0))
        _ambientBackground.value = prefs.getBoolean("ambient_background", true)
        _spectrumRing.value = prefs.getBoolean("spectrum_ring", true)
        _lyricOffsetMs.value = prefs.getFloat("lyric_offset_ms", 0f)

        val eqOn = prefs.getBoolean("eq_enabled", false)
        val preset = prefs.getInt("eq_preset", 0)
        val savedGains = prefs.getString("eq_gains", null)
        if (!savedGains.isNullOrEmpty()) {
            runCatching {
                val arr = JSONArray(savedGains)
                if (arr.length() == 10) {
                    val g = FloatArray(10) { i -> arr.getDouble(i).toFloat() }
                    g.forEachIndexed { b, db -> core.setEqGain(coreHandle, b, db) }
                    _eqGains.value = g
                }
            }
        }
        _eqPreset.value = preset
        _eqEnabled.value = eqOn
        core.setEqEnabled(coreHandle, eqOn)
    }

    private fun savePlaylist() {
        val arr = JSONArray()
        _tracks.value.forEach { arr.put(it.path) }
        prefs.edit()
            .putString("playlist_paths", arr.toString())
            .putInt("playlist_index", _currentIndex.value)
            .apply()
    }

    private fun restorePlaylist() {
        val raw = prefs.getString("playlist_paths", null) ?: return
        val savedIdx = prefs.getInt("playlist_index", -1)
        val paths = mutableListOf<String>()
        runCatching {
            val arr = JSONArray(raw)
            for (i in 0 until arr.length()) {
                val p = arr.optString(i, "")
                if (p.isNotBlank() && File(p).exists()) paths.add(p)
            }
        }
        if (paths.isEmpty()) return
        val initial = paths.map { Track(path = it, title = File(it).nameWithoutExtension) }
        _tracks.value = initial
        probeTracks(initial)
        val target = savedIdx.coerceIn(0, initial.lastIndex)
        loadTrack(target, autoplay = false)
    }

    private fun indexImportedLrc() {
        val dir = File(getApplication<Application>().filesDir, "Imported")
        if (!dir.exists()) return
        dir.listFiles()?.forEach { f ->
            if (f.isFile && f.extension.equals("lrc", ignoreCase = true)) {
                lrcByName[f.nameWithoutExtension.lowercase()] = f.absolutePath
            }
        }
    }

    private fun initMediaSession() {
        mediaSession = MediaSession(getApplication(), "FMusicSession").apply {
            setCallback(object : MediaSession.Callback() {
                override fun onPlay() { if (!_isPlaying.value) togglePlay() }
                override fun onPause() { if (_isPlaying.value) togglePlay() }
                override fun onSkipToNext() { next(automatic = false) }
                override fun onSkipToPrevious() { previous() }
                override fun onSeekTo(pos: Long) { seek(pos / 1000.0) }
            })
            isActive = true
        }
    }

    private fun updateMediaSessionState() {
        val session = mediaSession ?: return
        val state = if (_isPlaying.value) PlaybackState.STATE_PLAYING else PlaybackState.STATE_PAUSED
        val pb = PlaybackState.Builder()
            .setActions(
                PlaybackState.ACTION_PLAY or PlaybackState.ACTION_PAUSE or
                    PlaybackState.ACTION_PLAY_PAUSE or PlaybackState.ACTION_SKIP_TO_NEXT or
                    PlaybackState.ACTION_SKIP_TO_PREVIOUS or PlaybackState.ACTION_SEEK_TO
            )
            .setState(state, (_position.value * 1000).toLong(), if (_isPlaying.value) 1.0f else 0f)
            .build()
        session.setPlaybackState(pb)

        val meta = MediaMetadata.Builder()
            .putString(MediaMetadata.METADATA_KEY_TITLE, _title.value)
            .putString(MediaMetadata.METADATA_KEY_ARTIST, _artist.value)
            .putLong(MediaMetadata.METADATA_KEY_DURATION, (_duration.value * 1000).toLong())
        _coverBitmap.value?.let { meta.putBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART, it) }
        session.setMetadata(meta.build())
    }

    private fun requestAudioFocus() {
        if (focusRequest == null) {
            focusRequest = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build()
                )
                .setOnAudioFocusChangeListener { change ->
                    if (change == AudioManager.AUDIOFOCUS_LOSS || change == AudioManager.AUDIOFOCUS_LOSS_TRANSIENT) {
                        if (_isPlaying.value) pause()
                    }
                }
                .build()
        }
        focusRequest?.let { audioManager.requestAudioFocus(it) }
    }

    private fun initAudioTrack(sr: Int) {
        audioJob?.cancel()
        audioTrack?.let {
            runCatching { it.stop() }
            runCatching { it.release() }
        }
        val minBuf = AudioTrack.getMinBufferSize(sr, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_FLOAT)
        val safeMin = if (minBuf > 0) minBuf else sr / 10 * 8
        val bufSize = maxOf(safeMin, (sr / 15) * 8)

        val track = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setSampleRate(sr)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                    .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                    .build()
            )
            .setBufferSizeInBytes(bufSize)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
        audioTrack = track
        track.play()

        val framesPerChunk = maxOf(256, bufSize / 8)
        val pullBuf = FloatArray(framesPerChunk * 2)
        audioJob = viewModelScope.launch(Dispatchers.IO) {
            while (isActive) {
                core.pullInterleaved(coreHandle, pullBuf, framesPerChunk)
                val written = track.write(pullBuf, 0, pullBuf.size, AudioTrack.WRITE_BLOCKING)
                if (written < 0) delay(20)
            }
        }
    }

    private fun startTick() {
        tickJob = viewModelScope.launch {
            while (isActive) {
                val bassVal = core.copySpectrum(coreHandle, bandBuf)
                _bands.value = bandBuf.copyOf()
                _bass.value = bassVal

                val pos = core.getPosition(coreHandle)
                if (abs(pos - _position.value) > 0.03) {
                    _position.value = pos
                }

                if (_isPlaying.value && core.isEnded(coreHandle) && !hasHandledEnd) {
                    hasHandledEnd = true
                    next(automatic = true)
                }

                if (_lyricsSynced.value && _lyrics.value.isNotEmpty()) {
                    val offsetSec = _lyricOffsetMs.value / 1000.0
                    val target = pos + 0.05 + offsetSec
                    val list = _lyrics.value
                    var lo = 0
                    var hi = list.lastIndex
                    var ans = -1
                    while (lo <= hi) {
                        val mid = (lo + hi) ushr 1
                        if (list[mid].time <= target) {
                            ans = mid
                            lo = mid + 1
                        } else {
                            hi = mid - 1
                        }
                    }
                    if (ans != _currentLyric.value) _currentLyric.value = ans
                }
                delay(16)
            }
        }
    }

    // Nhap tep tu SAF Uri vao sandbox filesDir/Imported giong iOS de FFmpeg mo an toan & ben vung
    fun importUris(uris: List<Uri>, append: Boolean = true) {
        if (uris.isEmpty()) return
        val app = getApplication<Application>()
        viewModelScope.launch {
            val imported = withContext(Dispatchers.IO) {
                val dir = File(app.filesDir, "Imported").apply { mkdirs() }
                uris.mapNotNull { uri -> copyUriToImported(app, uri, dir) }
            }
            if (imported.isEmpty()) return@launch

            // Kiem tra file .feq
            val feq = imported.firstOrNull { it.extension.equals("feq", ignoreCase = true) }
            if (feq != null) {
                loadEqPreset(feq.absolutePath)
                return@launch
            }

            // Kiem tra file .fpl
            val fpl = imported.firstOrNull { it.extension.equals("fpl", ignoreCase = true) }
            if (fpl != null) {
                loadPlaylist(fpl.absolutePath)
                return@launch
            }

            // Dang ky .lrc
            var hasLrcOnly = true
            val audioPaths = mutableListOf<String>()
            imported.forEach { file ->
                if (file.extension.equals("lrc", ignoreCase = true)) {
                    lrcByName[file.nameWithoutExtension.lowercase()] = file.absolutePath
                } else {
                    hasLrcOnly = false
                    audioPaths.add(file.absolutePath)
                }
            }

            if (audioPaths.isEmpty()) {
                if (hasLrcOnly && _currentIndex.value in _tracks.value.indices) {
                    loadLyrics(_tracks.value[_currentIndex.value].path, embeddedLyricsCache)
                }
                return@launch
            }

            if (append) addFiles(audioPaths) else openFiles(audioPaths)
        }
    }

    private fun copyUriToImported(context: Context, uri: Uri, dir: File): File? {
        if (uri.scheme == "file" && uri.path != null) {
            val f = File(uri.path!!)
            if (f.exists() && f.canRead()) return f
        }
        var name: String? = null
        var srcSize = -1L
        runCatching {
            context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE), null, null, null)?.use { c ->
                if (c.moveToFirst()) {
                    val nameIdx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                    val sizeIdx = c.getColumnIndex(OpenableColumns.SIZE)
                    if (nameIdx >= 0) name = c.getString(nameIdx)
                    if (sizeIdx >= 0 && !c.isNull(sizeIdx)) srcSize = c.getLong(sizeIdx)
                }
            }
        }
        val cleanName = (name ?: "track_${System.currentTimeMillis()}.mp3").replace(Regex("[/\\\\]"), "_")
        val dest = File(dir, cleanName)
        if (dest.exists() && srcSize > 0 && dest.length() == srcSize) {
            return dest
        }
        return runCatching {
            context.contentResolver.openInputStream(uri)?.use { input ->
                FileOutputStream(dest).use { output ->
                    input.copyTo(output, bufferSize = 65536)
                }
            }
            if (dest.exists() && dest.length() > 0) dest else null
        }.getOrNull()
    }

    fun openFiles(paths: List<String>) {
        if (paths.isEmpty()) {
            clearPlaylist()
            return
        }
        _tracks.value = emptyList()
        _currentIndex.value = -1
        addFiles(paths)
    }

    fun addFiles(paths: List<String>) {
        if (paths.isEmpty()) return
        val existing = _tracks.value.map { it.path }.toSet()
        val unique = paths.filter { it !in existing && File(it).exists() }
        val toAdd = unique.ifEmpty { paths.filter { File(it).exists() } }
        if (toAdd.isEmpty()) return

        val wasEmpty = _tracks.value.isEmpty()
        val newTracks = toAdd.map { Track(path = it, title = File(it).nameWithoutExtension) }
        _tracks.value = _tracks.value + newTracks
        savePlaylist()
        probeTracks(newTracks)
        if (_currentIndex.value < 0 || wasEmpty) {
            loadTrack(0, autoplay = true)
        }
    }

    private fun probeTracks(items: List<Track>) {
        viewModelScope.launch(Dispatchers.IO) {
            items.forEach { track ->
                val info = MusicPlayerCore.probePath(track.path) ?: return@forEach
                withContext(Dispatchers.Main) {
                    _tracks.value = _tracks.value.map {
                        if (it.path == track.path) {
                            it.copy(
                                title = info.title.ifBlank { it.title },
                                artist = info.artist,
                                duration = info.duration
                            )
                        } else it
                    }
                }
            }
        }
    }

    fun removeTrack(index: Int) {
        val list = _tracks.value.toMutableList()
        if (index !in list.indices) return
        if (list.size == 1) {
            clearPlaylist()
            return
        }
        val wasPlaying = _isPlaying.value
        val cur = _currentIndex.value
        list.removeAt(index)
        _tracks.value = list
        when {
            index < cur -> {
                _currentIndex.value = cur - 1
                savePlaylist()
            }
            index == cur -> {
                val nextIdx = minOf(index, list.lastIndex)
                loadTrack(nextIdx, autoplay = wasPlaying)
            }
            else -> savePlaylist()
        }
    }

    fun clearPlaylist() {
        core.pause(coreHandle)
        _isPlaying.value = false
        _tracks.value = emptyList()
        _currentIndex.value = -1
        _lyrics.value = emptyList()
        _lyricsSynced.value = false
        _currentLyric.value = -1
        embeddedLyricsCache = ""
        _title.value = "Chưa có bài hát"
        _artist.value = "Mở Danh sách phát để thêm nhạc"
        _duration.value = 0.0
        _position.value = 0.0
        _coverBitmap.value = null
        _backdropBitmap.value = null
        _coverToken.value += 1
        savePlaylist()
        updateMediaSessionState()
    }

    fun loadTrack(index: Int, autoplay: Boolean) {
        val list = _tracks.value
        if (index !in list.indices) return
        hasHandledEnd = false
        _currentIndex.value = index
        val track = list[index]

        if (!core.loadPath(coreHandle, track.path)) {
            _title.value = "Không thể mở tệp"
            _artist.value = File(track.path).name
            _coverBitmap.value = null
            _backdropBitmap.value = null
            _coverToken.value += 1
            savePlaylist()
            return
        }

        val info = core.getTrackInfo(coreHandle)
        val resolvedTitle = info?.title?.takeIf { it.isNotBlank() } ?: track.title.ifBlank { File(track.path).nameWithoutExtension }
        val resolvedArtist = info?.artist ?: track.artist
        val resolvedDuration = info?.duration?.takeIf { it > 0 } ?: core.getDuration(coreHandle)
        embeddedLyricsCache = info?.lyrics ?: ""

        _title.value = resolvedTitle
        _artist.value = resolvedArtist
        _duration.value = resolvedDuration
        _position.value = 0.0

        _tracks.value = list.mapIndexed { i, t ->
            if (i == index) t.copy(title = resolvedTitle, artist = resolvedArtist, duration = resolvedDuration) else t
        }

        val rawCover = info?.coverArt
        backdropJob?.cancel()
        if (rawCover != null && rawCover.isNotEmpty()) {
            val bmp = BackdropHelper.decodeCover(rawCover)
            _coverBitmap.value = bmp
            if (bmp != null) {
                backdropJob = viewModelScope.launch {
                    val bd = withContext(Dispatchers.Default) { BackdropHelper.makeBackdrop(bmp) }
                    _backdropBitmap.value = bd
                    _coverToken.value += 1
                }
            } else {
                _backdropBitmap.value = null
                _coverToken.value += 1
            }
        } else {
            _coverBitmap.value = null
            _backdropBitmap.value = null
            _coverToken.value += 1
        }

        loadLyrics(track.path, embeddedLyricsCache)

        if (autoplay) {
            requestAudioFocus()
            core.play(coreHandle)
            _isPlaying.value = true
        } else {
            core.pause(coreHandle)
            _isPlaying.value = false
        }
        savePlaylist()
        updateMediaSessionState()
    }

    private fun loadLyrics(audioPath: String, embedded: String) {
        val audioFile = File(audioPath)
        val baseLower = audioFile.nameWithoutExtension.lowercase()
        val siblingLrc = audioFile.parentFile?.let { File(it, audioFile.nameWithoutExtension + ".lrc") }
        val mappedLrc = lrcByName[baseLower]?.let { File(it) }

        val text = when {
            siblingLrc != null && siblingLrc.exists() -> runCatching { siblingLrc.readText() }.getOrDefault("")
            mappedLrc != null && mappedLrc.exists() -> runCatching { mappedLrc.readText() }.getOrDefault("")
            else -> ""
        }.ifBlank { embedded }

        val parsed = MusicPlayerCore.parseLyrics(text)
        _lyricsSynced.value = parsed.synced
        _lyrics.value = parsed.lines.mapIndexed { i, l -> LyricLine(i, l.time, l.text) }
        _currentLyric.value = -1
    }

    fun togglePlay() {
        if (_tracks.value.isEmpty()) return
        if (_isPlaying.value) {
            pause()
        } else {
            play()
        }
    }

    fun play() {
        if (_tracks.value.isEmpty()) return
        if (core.isEnded(coreHandle)) {
            core.seekToSeconds(coreHandle, 0.0)
            _position.value = 0.0
        }
        requestAudioFocus()
        hasHandledEnd = false
        core.play(coreHandle)
        _isPlaying.value = true
        updateMediaSessionState()
    }

    fun pause() {
        core.pause(coreHandle)
        _isPlaying.value = false
        updateMediaSessionState()
    }

    fun next(automatic: Boolean = false) {
        val list = _tracks.value
        if (list.isEmpty()) return
        if (automatic && _repeatMode.value == RepeatMode.ONE) {
            hasHandledEnd = false
            core.seekToSeconds(coreHandle, 0.0)
            _position.value = 0.0
            core.play(coreHandle)
            _isPlaying.value = true
            updateMediaSessionState()
            return
        }
        val cur = _currentIndex.value
        val nextIdx = when {
            _shuffle.value && list.size > 1 -> (list.indices).filter { it != cur }.random()
            cur + 1 < list.size -> cur + 1
            _repeatMode.value == RepeatMode.ALL -> 0
            else -> null
        }
        if (nextIdx != null) {
            loadTrack(nextIdx, autoplay = true)
        } else {
            core.pause(coreHandle)
            core.seekToSeconds(coreHandle, 0.0)
            _isPlaying.value = false
            _position.value = 0.0
            updateMediaSessionState()
        }
    }

    fun previous() {
        val pos = core.getPosition(coreHandle)
        if (pos > 3.0 || _currentIndex.value <= 0) {
            seek(0.0)
        } else {
            loadTrack(_currentIndex.value - 1, autoplay = true)
        }
    }

    fun seek(seconds: Double) {
        hasHandledEnd = false
        core.seekToSeconds(coreHandle, seconds)
        _position.value = seconds
        updateMediaSessionState()
    }

    fun toggleShuffle() {
        val next = !_shuffle.value
        _shuffle.value = next
        prefs.edit().putBoolean("shuffle", next).apply()
    }

    fun cycleRepeat() {
        val next = when (_repeatMode.value) {
            RepeatMode.OFF -> RepeatMode.ALL
            RepeatMode.ALL -> RepeatMode.ONE
            RepeatMode.ONE -> RepeatMode.OFF
        }
        _repeatMode.value = next
        prefs.edit().putInt("repeat_mode", next.value).apply()
    }

    // Settings
    fun setAmbientBackground(on: Boolean) {
        _ambientBackground.value = on
        prefs.edit().putBoolean("ambient_background", on).apply()
    }

    fun setSpectrumRing(on: Boolean) {
        _spectrumRing.value = on
        prefs.edit().putBoolean("spectrum_ring", on).apply()
    }

    fun setLyricOffsetMs(ms: Float) {
        _lyricOffsetMs.value = ms
        prefs.edit().putFloat("lyric_offset_ms", ms).apply()
    }

    fun setSampleRate(sr: Int) {
        _sampleRate.value = sr
        core.setSampleRate(coreHandle, sr)
        initAudioTrack(sr)
        prefs.edit().putInt("sample_rate", sr).apply()
    }

    // EQ
    private fun saveEq() {
        val arr = JSONArray()
        _eqGains.value.forEach { arr.put(it.toDouble()) }
        prefs.edit()
            .putBoolean("eq_enabled", _eqEnabled.value)
            .putInt("eq_preset", _eqPreset.value)
            .putString("eq_gains", arr.toString())
            .apply()
    }

    fun setEqEnabled(on: Boolean) {
        _eqEnabled.value = on
        core.setEqEnabled(coreHandle, on)
        saveEq()
    }

    fun setEqGain(band: Int, db: Float) {
        val gains = _eqGains.value.copyOf()
        gains[band] = db
        _eqGains.value = gains
        core.setEqGain(coreHandle, band, db)
        _eqPreset.value = -1
        if (!_eqEnabled.value) {
            _eqEnabled.value = true
            core.setEqEnabled(coreHandle, true)
        }
        saveEq()
    }

    fun applyPreset(idx: Int) {
        core.applyEqPreset(coreHandle, idx)
        _eqGains.value = FloatArray(10) { core.getEqGain(coreHandle, it) }
        _eqPreset.value = idx
        _eqEnabled.value = true
        core.setEqEnabled(coreHandle, true)
        saveEq()
    }

    private fun eqPresetsDir(): File {
        return File(getApplication<Application>().filesDir, "eq_presets").apply { mkdirs() }
    }

    fun loadUserPresets() {
        val files = eqPresetsDir().listFiles()?.filter { it.extension.equals("feq", ignoreCase = true) }.orEmpty()
        val loaded = files.mapNotNull { file ->
            runCatching {
                val json = JSONObject(file.readText())
                val arr = json.getJSONArray("gains")
                if (arr.length() != 10) return@runCatching null
                val gains = FloatArray(10) { arr.getDouble(it).toFloat() }
                val name = json.optString("name", file.nameWithoutExtension)
                UserEqPreset(name, gains)
            }.getOrNull()
        }.sortedBy { it.name.lowercase() }
        _userPresets.value = loaded
    }

    fun saveEqPreset(name: String) {
        val clean = name.trim().replace(Regex("[/\\\\]"), "_")
        if (clean.isEmpty()) return
        val json = JSONObject().apply {
            put("version", 1)
            put("name", clean)
            put("enabled", _eqEnabled.value)
            put("gains", JSONArray(_eqGains.value.map { it.toDouble() }))
        }
        File(eqPresetsDir(), "$clean.feq").writeText(json.toString(2))
        loadUserPresets()
    }

    fun applyUserPreset(preset: UserEqPreset) {
        preset.gains.forEachIndexed { b, db -> core.setEqGain(coreHandle, b, db) }
        _eqGains.value = preset.gains.copyOf()
        _eqPreset.value = -1
        _eqEnabled.value = true
        core.setEqEnabled(coreHandle, true)
        saveEq()
    }

    fun deleteUserPreset(name: String) {
        File(eqPresetsDir(), "$name.feq").delete()
        loadUserPresets()
    }

    fun exportPresetFile(name: String = "FMusic_Preset"): File {
        val dir = File(getApplication<Application>().cacheDir, "share").apply { mkdirs() }
        val file = File(dir, "$name.feq")
        val json = JSONObject().apply {
            put("version", 1)
            put("name", name)
            put("gains", JSONArray(_eqGains.value.map { it.toDouble() }))
        }
        file.writeText(json.toString(2))
        return file
    }

    fun loadEqPreset(path: String) {
        runCatching {
            val json = JSONObject(File(path).readText())
            val arr = json.getJSONArray("gains")
            if (arr.length() != 10) return
            val gains = FloatArray(10) { arr.getDouble(it).toFloat() }
            gains.forEachIndexed { i, g -> core.setEqGain(coreHandle, i, g) }
            _eqGains.value = gains
            _eqPreset.value = -1
            setEqEnabled(json.optBoolean("enabled", true))
        }
    }

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

    override fun onCleared() {
        super.onCleared()
        backdropJob?.cancel()
        audioJob?.cancel()
        tickJob?.cancel()
        focusRequest?.let { audioManager.abandonAudioFocusRequest(it) }
        runCatching { mediaSession?.release() }
        runCatching { audioTrack?.stop() }
        runCatching { audioTrack?.release() }
        if (coreHandle != 0L) {
            core.destroy(coreHandle)
            coreHandle = 0L
        }
    }
}
